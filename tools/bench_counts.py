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
# other counts. A `misses` line bounds a layout's cache misses a frame
# over another's, counted by a perf event in the same way, the median of
# three tries (`misses <layout> <over layout> <runs> <bound> <event>`):
# misses move with what else the machine runs, a ratio of two counted
# side by side much less. An event this machine does not count is
# reported and its line skipped.
#
#   python3 tools/bench_counts.py <maul-tween_bench> bench/counts.txt

import subprocess
import sys


def count(bench, name, n, period, event="instructions:u"):
    command = ["perf", "stat", "-x", ",", "-e", event, bench, "--count", name, str(n)]
    result = subprocess.run(command + period, capture_output=True, text=True)
    for line in result.stderr.splitlines():
        fields = line.split(",")
        if len(fields) > 2 and fields[2] == event and fields[0].isdigit():
            return int(fields[0])
    return None


def each(bench, name, n, period, event="instructions:u"):
    many = count(bench, name, n, period, event)
    none = count(bench, name, 0, period, event)
    if many is None or none is None:
        return None
    return (many - none) / n


def misses(bench, fields, over):
    name, under, n, bound, event = fields[1], fields[2], int(fields[3]), float(fields[4]), fields[5]
    ratios = []
    for _ in range(3):
        a = each(bench, name, n, [], event)
        b = each(bench, under, n, [], event)
        if a is None or b is None or b <= 0:
            print("%-14s %s not counted on this machine, skipped" % (name, event))
            return
        ratios.append(a / b)
    ratio = sorted(ratios)[1]
    label = "%s/%s" % (name, under)
    print("%-22s %6.3f times the misses, bound %6.3f" % (label, ratio, bound))
    if ratio > bound:
        over.append("%s misses %.3f times over its %.3f" % (label, ratio, bound))


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
            if fields[0] == "misses":
                misses(bench, fields, over)
                continue
            name, n, bound = fields[0], int(fields[1]), int(fields[2])
            period = fields[3:4]
            label = name + (" at %s s" % period[0] if period else "")
            counted = each(bench, name, n, period)
            if counted is None:
                sys.exit("bench_counts.py: no instruction count for %s %d" % (name, n))
            frame = int(counted)
            print("%-14s %10d instructions, bound %10d" % (label, frame, bound))
            if frame > bound:
                over.append("%s %d over its %d" % (label, frame, bound))
    compilers = "counted with %s, the bounds set with %s" % (built, bounded)
    if built != bounded:
        print("bench_counts.py: the compilers differ: " + compilers)
    if over:
        sys.exit("bench_counts.py: " + "; ".join(over) + " (" + compilers + ")")


if __name__ == "__main__":
    main()
