#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Sirac Ozmen
#
# An independent reference for Maul Tween's timing (records mtwn-0012,
# mtwn-0020 and mtwn-0027): Web Animations Level 1's timing steps
# written again from the specification's text, with Level 2's effect
# playback rate and Maul Tween's repeat delay and alternate easing, and
# CSS's steps() easing so the before flag shows. It writes
# test/generated/timing_cases.h, inputs from its own SplitMix64 and the
# results it computes. Every input is a dyadic fraction and every
# duration and rate a power of two, so both implementations compute
# exactly and must agree to the bit.
#
# usage: timing_oracle.py [--check]

import math
import os
import sys

PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "test",
                    "generated", "timing_cases.h")

INF = math.inf
NAN = math.nan

BEFORE, ACTIVE, AFTER = 0, 1, 2
FILL_NONE, FILL_FORWARDS, FILL_BACKWARDS, FILL_BOTH = 0, 1, 2, 3
NORMAL, REVERSE, ALTERNATE, ALTERNATE_REVERSE = 0, 1, 2, 3
RUNS_BACK, REPEATS_SHAPE = 0, 1
LINEAR, STEPS_4_START, STEPS_3_END = 0, 1, 2


class SplitMix64:
    def __init__(self, seed):
        self.state = seed & (2 ** 64 - 1)

    def next(self):
        self.state = (self.state + 0x9E3779B97F4A7C15) & (2 ** 64 - 1)
        z = self.state
        z = ((z ^ (z >> 30)) * 0xBF58476D1CE4E5B9) & (2 ** 64 - 1)
        z = ((z ^ (z >> 27)) * 0x94D049BB133111EB) & (2 ** 64 - 1)
        return z ^ (z >> 31)

    def pick(self, values):
        return values[self.next() % len(values)]


def steps(count, jump_start, progress, before):
    # CSS Easing Level 2's step easing function, for jump-start or
    # jump-end.
    current = math.floor(progress * count)
    if jump_start:
        current += 1
    if before and (progress * count) % 1 == 0:
        current -= 1
    if progress >= 0 and current < 0:
        current = 0
    jumps = count
    if progress <= 1 and current > jumps:
        current = jumps
    return current / jumps


def ease(kind, progress, before):
    if kind == LINEAR:
        return progress
    if kind == STEPS_4_START:
        return steps(4, True, progress, before)
    return steps(3, False, progress, before)


def floor(x):
    # floor as C has it, infinities passing through.
    return x if math.isinf(x) else float(math.floor(x))


def ceil(x):
    return x if math.isinf(x) else float(math.ceil(x))


def compute(t, local, backwards):
    d = t["duration"]
    n = t["iterations"]
    r = t["repeatDelay"]
    rate = t["rate"]
    start = t["iterationStart"]
    repeat = r > 0 and 0 < d < INF
    # Calculating the active duration (Level 2, with the repeat delay).
    if d == 0 or n == 0:
        repeated = 0.0
    elif repeat and n != INF:
        # One delay at each iteration boundary the path crosses.
        repeated = n * d + (ceil(start + n) - math.floor(start) - 1) * r
    else:
        repeated = n * d
    active_duration = repeated / abs(rate)
    end_time = max(t["delay"] + active_duration + t["endDelay"], 0.0)
    # Animation effect phases.
    before_active = max(min(t["delay"], end_time), 0.0)
    active_after = max(min(t["delay"] + active_duration, end_time), 0.0)
    if local < before_active or (backwards and local == before_active):
        phase = BEFORE
    elif local > active_after or (not backwards and local == active_after):
        phase = AFTER
    else:
        phase = ACTIVE
    # Calculating the active time.
    if phase == BEFORE:
        active = max(local - t["delay"], 0.0) if t["fill"] in (FILL_BACKWARDS, FILL_BOTH) else NAN
    elif phase == ACTIVE:
        active = local - t["delay"]
    else:
        if t["fill"] in (FILL_FORWARDS, FILL_BOTH):
            active = max(min(local - t["delay"], active_duration), 0.0)
        else:
            active = NAN
    result = {"phase": phase, "activeDuration": active_duration, "endTime": end_time,
              "activeTime": active, "overall": NAN, "simple": NAN, "iteration": NAN,
              "directed": NAN, "transformed": NAN, "before": 0, "inRepeat": 0}
    if math.isnan(active):
        return result
    # Calculating the overall progress.
    in_repeat = False
    held = 0.0
    # A negative effect rate begins the iterations at their end (Level 2).
    at_start = (phase == BEFORE) if rate > 0 else (phase == AFTER)
    at_end = (phase in (ACTIVE, AFTER) and active == active_duration) if rate > 0 else \
        (phase in (BEFORE, ACTIVE) and active == 0)
    if d == 0:
        overall = (0.0 if at_start else n) + start
    else:
        scaled = (active - active_duration) * rate if rate < 0 else active * rate
        if repeat:
            # On the cycle timeline each iteration takes d + r; the
            # iteration start puts the first one partway in.
            cycle = d + r
            whole = floor(start)
            position = scaled + whole * cycle + (start - whole) * d
            k = floor(position / cycle)
            within = position - k * cycle
            final = n != INF and k + 1 >= ceil(start + n)
            if within >= d and not final:
                in_repeat = True
                held = float(k)
            overall = k + (within / d if within < d else 1.0)
        else:
            overall = scaled / d + start
    # Rounding keeps to the iterations: the active interval's ends are the
    # iteration start and its end exactly.
    overall = min(max(overall, start), start + n)
    result["overall"] = overall
    result["inRepeat"] = 1 if in_repeat else 0
    # Calculating the simple iteration progress.
    simple = math.fmod(start, 1.0) if overall == INF else math.fmod(overall, 1.0)
    if in_repeat or (simple == 0 and at_end and n != 0):
        simple = 1.0
    result["simple"] = simple
    # Calculating the current iteration index.
    if in_repeat:
        iteration = held
    elif phase == AFTER and n == INF:
        iteration = INF
    elif simple == 1.0:
        iteration = floor(overall) - 1
    else:
        iteration = floor(overall)
    result["iteration"] = iteration
    # Calculating the directed progress.
    direction = t["direction"]
    if direction == NORMAL:
        forwards = True
    elif direction == REVERSE:
        forwards = False
    else:
        if iteration == INF:
            forwards = True
        else:
            count = iteration + 1 if direction == ALTERNATE_REVERSE else iteration
            forwards = count % 2 == 0
    directed = simple if forwards else 1.0 - simple
    result["directed"] = directed
    # Calculating the transformed progress, the before flag with the
    # effect's rate.
    going = forwards == (rate > 0)
    flag = (phase == BEFORE and going) or (phase == AFTER and not going)
    result["before"] = 1 if flag else 0
    if not forwards and t["alternate"] == REPEATS_SHAPE:
        result["transformed"] = 1.0 - ease(t["easing"], simple, flag)
    else:
        result["transformed"] = ease(t["easing"], directed, flag)
    return result


def cases():
    random = SplitMix64(2026)
    out = []
    for _ in range(1200):
        t = {
            "delay": random.pick([-2.0, -1.0, -0.5, 0.0, 0.25, 0.5, 1.0, 2.0]),
            "endDelay": random.pick([-2.0, -0.5, 0.0, 0.0, 0.5, 1.5]),
            "iterationStart": random.pick([0.0, 0.0, 0.25, 0.5, 1.75, 2.0]),
            "iterations": random.pick([0.0, 0.5, 1.0, 1.0, 2.0, 2.5, 3.0, INF]),
            "duration": random.pick([0.0, 0.5, 1.0, 1.0, 2.0, INF]),
            "repeatDelay": random.pick([0.0, 0.0, 0.25, 0.5]),
            "rate": random.pick([1.0, 1.0, 2.0, 0.5, -1.0, -2.0]),
            "fill": random.pick([FILL_NONE, FILL_FORWARDS, FILL_BACKWARDS, FILL_BOTH]),
            "direction": random.pick([NORMAL, REVERSE, ALTERNATE, ALTERNATE_REVERSE]),
            "alternate": random.pick([RUNS_BACK, REPEATS_SHAPE]),
            "easing": random.pick([LINEAR, STEPS_4_START, STEPS_3_END]),
        }
        # A backward rate needs a finite active duration to start from.
        if t["rate"] < 0 and t["iterations"] * t["duration"] == INF:
            t["rate"] = -t["rate"]
        # Times at the boundaries that matter and between them.
        probe = compute(t, 0.0, False)
        marks = [t["delay"], t["delay"] + probe["activeDuration"], probe["endTime"], 0.0]
        local = random.pick(marks) if random.next() % 3 == 0 else (
            (random.next() % 1025) / 64.0 - 4.0)
        backwards = random.next() % 4 == 0
        if math.isinf(local) or math.isnan(local):
            local = 1.0
        out.append((t, local, backwards, compute(t, local, backwards)))
    # Repeat delays, at times inside the active interval where they lie.
    for _ in range(400):
        t = {
            "delay": random.pick([0.0, 0.5, -0.5]),
            "endDelay": random.pick([0.0, 1.0]),
            "iterationStart": random.pick([0.0, 0.0, 0.5, 1.25, 2.0]),
            "iterations": random.pick([1.5, 2.0, 3.0, 3.5, INF]),
            "duration": random.pick([0.5, 1.0, 2.0]),
            "repeatDelay": random.pick([0.25, 0.5, 1.0]),
            "rate": random.pick([1.0, 2.0, 0.5, -1.0]),
            "fill": random.pick([FILL_FORWARDS, FILL_BOTH]),
            "direction": random.pick([NORMAL, REVERSE, ALTERNATE, ALTERNATE_REVERSE]),
            "alternate": random.pick([RUNS_BACK, REPEATS_SHAPE]),
            "easing": random.pick([LINEAR, STEPS_4_START, STEPS_3_END]),
        }
        if t["rate"] < 0 and t["iterations"] == INF:
            t["rate"] = 1.0
        local = (random.next() % 1281) / 128.0 - 0.5
        backwards = random.next() % 4 == 0
        out.append((t, local, backwards, compute(t, local, backwards)))
    out.extend(regressions())
    return out


def regressions():
    """Fixed cases for defects the crossing oracle found (record mtwn-0027), kept
    so a later change to repeat delays or negative rates meets them."""
    base = {"delay": 0.0, "endDelay": 0.0, "iterationStart": 0.0, "iterations": 2.0,
            "duration": 1.0, "repeatDelay": 0.0, "rate": 1.0, "fill": FILL_BOTH,
            "direction": NORMAL, "alternate": RUNS_BACK, "easing": LINEAR}
    out = []
    # A fractional iteration start with a repeat delay: two boundaries, two
    # delays; the active interval ends on the last partial iteration.
    t = dict(base, iterationStart=0.25, repeatDelay=0.5)
    for local in [0.75, 1.0, 1.25, 2.0, 2.25, 2.5, 2.75, 3.0, 3.5]:
        out.append((t, local, False, compute(t, local, False)))
    # A negative rate begins at the iterations' end: before its delay
    # ends (filling backwards) and at its active start.
    t = dict(base, delay=1.0, rate=-1.0, fill=FILL_BACKWARDS)
    for local in [0.0, 0.5, 1.0, 1.5]:
        out.append((t, local, False, compute(t, local, False)))
    out.append((dict(t, duration=0.0), 0.5, False, compute(dict(t, duration=0.0), 0.5, False)))
    # Rounding past the iterations' end: 3.7 iterations of 0.1875 from 0.3,
    # backwards, at the active start.
    t = dict(base, duration=0.1875, iterations=3.7, iterationStart=0.3, rate=-1.0)
    for local in [0.0, 0.69375]:
        out.append((t, local, False, compute(t, local, False)))
    return out


def number(value):
    if math.isnan(value):
        return "NAN_CASE"
    if math.isinf(value):
        return "HUGE_VAL" if value > 0 else "-HUGE_VAL"
    return float(value).hex()


def render():
    lines = [
        "// SPDX-License-Identifier: MIT",
        "// Copyright (c) 2026 Sirac Ozmen",
        "//",
        "// Generated by tools/oracle/timing_oracle.py; do not edit. Timing",
        "// cases and the results the oracle computes for them.",
        "// clang-format off",
        "",
        "#ifndef MTWN_TIMING_CASES_H",
        "#define MTWN_TIMING_CASES_H",
        "",
        "#define NAN_CASE ((double)NAN)",
        "",
        "static const TimingCase s_cases[] = {",
    ]
    for t, local, backwards, r in cases():
        inputs = ", ".join(number(t[k]) for k in ("delay", "endDelay", "iterationStart",
                                                  "iterations", "duration", "repeatDelay",
                                                  "rate"))
        enums = f"{t['fill']}, {t['direction']}, {t['alternate']}, {t['easing']}, {int(backwards)}"
        outputs = ", ".join(number(r[k]) for k in ("activeDuration", "endTime", "activeTime",
                                                   "overall", "simple", "iteration", "directed",
                                                   "transformed"))
        lines.append(f"    {{{{{inputs}, {enums}}}, {number(local)}, {r['phase']}, "
                     f"{{{outputs}}}, {r['before']}, {r['inRepeat']}}},")
    lines += ["};", "", "#endif", ""]
    return "\n".join(lines)


def main():
    text = render()
    if len(sys.argv) > 1 and sys.argv[1] == "--check":
        current = open(PATH, encoding="utf-8").read() if os.path.exists(PATH) else ""
        if current != text:
            print("test/generated/timing_cases.h is stale: run tools/oracle/timing_oracle.py")
            return 1
        print("timing cases: current")
        return 0
    os.makedirs(os.path.dirname(PATH), exist_ok=True)
    with open(PATH, "w", encoding="utf-8", newline="\n") as f:
        f.write(text)
    print("wrote " + os.path.normpath(PATH))
    return 0


if __name__ == "__main__":
    sys.exit(main())
