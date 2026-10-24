#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Sirac Ozmen
#
# The symbols a shared build exports are the public functions, no more
# and no fewer: every function the public headers declare MTWN_API, and
# nothing else, so the tests' hooks (src/test_hooks.h) and every internal
# function stay out of the binary interface.
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


def exported(library):
    out = subprocess.run(["nm", "-D", "--defined-only", library], check=True,
                         capture_output=True, text=True).stdout
    names = set()
    for line in out.splitlines():
        fields = line.split()
        if len(fields) == 3 and fields[1] in "TtWw":
            names.add(fields[2])
    return names


def main():
    if len(sys.argv) != 2:
        print("usage: tools/check_exports.py LIBRARY", file=sys.stderr)
        return 2
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    public = declared(os.path.join(root, "include"))
    symbols = exported(sys.argv[1])
    extra = sorted(name for name in symbols - public if not name.startswith("_"))
    missing = sorted(public - symbols)
    for name in extra:
        print(f"exported but not public: {name}")
    for name in missing:
        print(f"public but not exported: {name}")
    if extra or missing or not public:
        return 1
    print(f"exports: the {len(public)} public functions, nothing else")
    return 0


if __name__ == "__main__":
    sys.exit(main())
