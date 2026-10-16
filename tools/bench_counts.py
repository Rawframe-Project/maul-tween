#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Sirac Ozmen
#
# The instructions the benchmark's frames and samples take, against the
# bounds in bench/counts.txt (record mtwn-0043): a shortcut lost costs
# instructions though no result changes, and no test can see it. Each
# line names a --count layout or values case and how many updates or
# samples to run; one frame or sample is the difference of a run of that
# many and a run of none, counted by `perf stat -e instructions:u`
# (Linux); a fourth column gives the clip's period, for a layout whose
# players repeat more often than each second. The bounds are for one
# build: the file's `compiler` line names the compiler they were set
# with, and the check names the bench's own (its --compiler) beside it
# when they differ or a bound fails; another compiler or flags give
# other counts.
#
#   python3 tools/bench_counts.py <maul-tween_bench> bench/counts.txt

import subprocess
import sys


def count(bench, name, n, period):
    command = ["perf", "stat", "-x", ",", "-e", "instructions:u", bench, "--count", name, str(n)]
    result = subprocess.run(command + period, capture_output=True, text=True, check=True)
    for line in result.stderr.splitlines():
        fields = line.split(",")
        if len(fields) > 2 and fields[2].startswith("instructions"):
            return int(fields[0])
    sys.exit("bench_counts.py: no instruction count for %s %d" % (name, n))


def main():
    if len(sys.argv) != 3:
        sys.exit("usage: bench_counts.py <maul-tween_bench> <counts.txt>")
    bench, table = sys.argv[1], sys.argv[2]
    built = subprocess.run([bench, "--compiler"], capture_output=True, text=True,
                           check=True).stdout.strip()
    bounded = "not named"
    over = []
    with open(table, encoding="utf-8") as lines:
        for line in lines:
            line = line.split("#", 1)[0].strip()
            if not line:
                continue
            fields = line.split()
            if fields[0] == "compiler":
                bounded = " ".join(fields[1:])
                continue
            name, n, bound = fields[0], int(fields[1]), int(fields[2])
            period = fields[3:4]
            label = name + (" at %s s" % period[0] if period else "")
            each = (count(bench, name, n, period) - count(bench, name, 0, period)) // n
            print("%-14s %10d instructions, bound %10d" % (label, each, bound))
            if each > bound:
                over.append("%s %d over its %d" % (label, each, bound))
    compilers = "counted with %s, the bounds set with %s" % (built, bounded)
    if built != bounded:
        print("bench_counts.py: the compilers differ: " + compilers)
    if over:
        sys.exit("bench_counts.py: " + "; ".join(over) + " (" + compilers + ")")


if __name__ == "__main__":
    main()
