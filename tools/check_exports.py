#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Sirac Ozmen
#
# The symbols a shared build exports are the public functions, no more
# and no fewer: every function the public headers declare MTWN_API, and
# nothing else, so the tests' hooks (src/test_hooks.h), every internal
# function and any global stay out of the binary interface. The public
# headers declare no data, so every exported data symbol fails it.
#
#   python3 tools/check_exports.py out/linux-shared/libmaul-tween.so
#
# Reads the dynamic symbol table with nm; for ELF shared libraries.

import glob
import os
import re
import subprocess
import sys


def declared(include):
    names = set()
    for path in glob.glob(os.path.join(include, "maul-tween", "*.h")):
        with open(path, encoding="utf-8") as f:
            names.update(re.findall(r"MTWN_API\s[^(;]*?\b(mtwn\w+)\s*\(", f.read()))
    return names


# nm's letters: code, and data initialized, zeroed, read-only, small or
# unique, global or weak.
CODE = set("TtWw")
DATA = set("DdBbRrGgSsVvu")


def exported(library):
    out = subprocess.run(["nm", "-D", "--defined-only", library], check=True,
                         capture_output=True, text=True).stdout
    code = set()
    data = set()
    for line in out.splitlines():
        fields = line.split()
        if len(fields) != 3 or fields[2].startswith("_"):
            continue
        if fields[1] in CODE:
            code.add(fields[2])
        elif fields[1] in DATA:
            data.add(fields[2])
    return code, data


def main():
    if len(sys.argv) != 2:
        print("usage: tools/check_exports.py LIBRARY", file=sys.stderr)
        return 2
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    public = declared(os.path.join(root, "include"))
    code, data = exported(sys.argv[1])
    extra = sorted(code - public)
    missing = sorted(public - code)
    for name in extra:
        print(f"exported but not public: {name}")
    for name in missing:
        print(f"public but not exported: {name}")
    for name in sorted(data):
        print(f"exported data: {name}")
    if extra or missing or data or not public:
        return 1
    print(f"exports: the {len(public)} public functions, no data")
    return 0


if __name__ == "__main__":
    sys.exit(main())
