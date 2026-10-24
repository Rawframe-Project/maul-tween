#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Sirac Ozmen
#
# The code a program links, by the parts of the library it uses:
# for each part, a program that takes the
# address of every public function its headers declare is linked against
# a built static library with unused sections dropped, and its .text,
# less an empty program's, is the part's size. The parts are the public
# headers, grouped; every public function must fall in one.
#
#   python3 tools/code_size.py build/libmaul-tween.a [--cc gcc-14]
#       [--map PART] [--budget]
#
# --map PART also prints that part's code by the library's object files,
# read from the linker's map: what links it in. --budget fails when a
# part is over its budget, for a library built in Release with GCC 14 by
# default options; growth past one is a decision, stated in its pull
# request. It fails too when a budget is set more than GROWTH over the
# part's size when the budgets were set, unless RAISED names the part with
# the reason.

import argparse
import os
import re
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
INCLUDE = os.path.join(ROOT, "include", "maul-tween")

PARTS = [
    ("curves", ["easing.h", "sample.h", "spring.h"]),
    ("values", ["interpolate.h", "path.h", "timing.h"]),
    ("transitions", ["base.h", "context.h", "target.h", "transition.h"]),
    ("clips", ["clip.h", "player.h", "event.h"]),
]

# Each part's budget in bytes, about 6% over its size when it was set;
# the curves' moved by what the float environment's wrappers cost (#22),
# and the whole library's by what an overflowing start's exact settling
# time costs (#20).
BUDGETS = {
    "curves": 27900,
    "values": 60000,
    "transitions": 63000,
    "clips": 152000,
    "everything": 169400,
}


# Each part's size in bytes when the budgets were last set, the ceiling
# GROWTH over it a budget may not pass, and the parts whose ceiling a
# pull request raised past it, with the reason.
RELEASED = {
    "curves": 25049,
    "values": 56665,
    "transitions": 59929,
    "clips": 144361,
    "everything": 160985,
}
GROWTH = 0.10
RAISED = {
    "curves": "every public call that computes enters the float environment through a "
    "wrapper of its own (#22), 0.8 kB here with -fno-math-errno",
}


def check_ceilings():
    for name, budget in BUDGETS.items():
        ceiling = RELEASED[name] * (1.0 + GROWTH)
        if budget > ceiling and name not in RAISED:
            sys.exit("code_size.py: the %s budget, %.1f kB, passes %.0f%% over its %.1f kB when "
                     "the budgets were set; RAISED names a part raised past it, with the reason" %
                     (name, budget / 1000.0, GROWTH * 100.0, RELEASED[name] / 1000.0))


def functions(header):
    text = open(os.path.join(INCLUDE, header), encoding="utf-8").read()
    return re.findall(r"MTWN_API\s[^(;]*?\b(mtwn\w+)\s*\(", text)


def check_parts():
    named = {h for _, headers in PARTS for h in headers}
    present = {h for h in os.listdir(INCLUDE) if h.endswith(".h")}
    missing = sorted(h for h in present - named if functions(h))
    if missing:
        sys.exit("code_size.py: headers in no part: " + ", ".join(missing))


def host(names, path):
    with open(path, "w", encoding="utf-8") as out:
        for name in names:
            out.write("extern void %s(void);\n" % name)
        out.write("volatile unsigned long sink;\nint main(void)\n{\n")
        for name in names:
            out.write("    sink += (unsigned long)&%s;\n" % name)
        out.write("    return 0;\n}\n")


def text_size(cc, library, names, work, map_path=None):
    source = os.path.join(work, "host.c")
    program = os.path.join(work, "host")
    host(names, source)
    command = [cc, "-O2", "-w", source, library, "-lm", "-Wl,--gc-sections", "-o", program]
    if map_path:
        command.append("-Wl,-Map=" + map_path)
    subprocess.run(command, check=True)
    sections = subprocess.run(["size", "-A", program], check=True, capture_output=True,
                              text=True).stdout
    for line in sections.splitlines():
        if line.startswith(".text "):
            return int(line.split()[1])
    sys.exit("code_size.py: no .text in the program")


def by_object(map_path):
    sizes = {}
    lines = open(map_path, encoding="utf-8").read().split("\n")
    for k, line in enumerate(lines):
        if not line.startswith(" .text"):
            continue
        fields = line.split()
        if len(fields) < 4 and k + 1 < len(lines):
            fields = fields[:1] + lines[k + 1].split()
        if len(fields) < 4 or not fields[2].startswith("0x"):
            continue
        member = re.search(r"\((\w+)\.c\.o\)$", fields[3])
        if member:
            sizes[member.group(1)] = sizes.get(member.group(1), 0) + int(fields[2], 16)
    return sorted(sizes.items(), key=lambda item: -item[1])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("library")
    parser.add_argument("--cc", default="gcc-14")
    parser.add_argument("--map", choices=[name for name, _ in PARTS] + ["everything"])
    parser.add_argument("--budget", action="store_true")
    args = parser.parse_args()
    over = []
    check_parts()
    rows = [(name, [f for h in headers for f in functions(h)]) for name, headers in PARTS]
    rows.append(("everything", [f for _, names in rows for f in names]))
    with tempfile.TemporaryDirectory() as work:
        empty = text_size(args.cc, args.library, [], work)
        for name, names in rows:
            map_path = os.path.join(work, "host.map") if args.map == name else None
            size = text_size(args.cc, args.library, names, work, map_path) - empty
            print("%-12s %3d functions %8.1f kB" % (name, len(names), size / 1000.0))
            if size > BUDGETS[name]:
                over.append("%s %.1f kB over its %.1f kB" % (name, size / 1000.0,
                                                             BUDGETS[name] / 1000.0))
            if map_path:
                for member, bytes_ in by_object(map_path):
                    print("    %-18s %7d" % (member, bytes_))
    if args.budget and over:
        sys.exit("code_size.py: " + "; ".join(over))
    if args.budget:
        check_ceilings()


if __name__ == "__main__":
    main()
