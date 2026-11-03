#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Sirac Ozmen
#
# An independent reference for Maul Tween's clip players: Web Animations
# Level 1's procedures for an animation (its current time, play states,
# play, pause, finish, setting the current time, updating the finished
# state and seamlessly updating the playback rate), written again from the
# specification's text, with Maul Tween's two adaptations:
#
# - the ready time of a pending task is the next update's time, and a
#   pending pause is ready at once, at the last update's time;
# - a moving player's new rate is a pending playback rate, not a pending
#   play task: until the next update applies it, from the time then
#   current, the player finishes, holds and seeks at its old rate, where
#   a pending play task would stop the finished state's updates and let
#   the old rate run past an end for as long as the wait;
# - a finished player given a rate other than 0 takes its start time from
#   the time it holds, start = timeline time - current time / rate, where
#   the specification's seamless update takes it from the time not held
#   (docs/internals/timing.md).
#
# It writes test/generated/player_cases.h: scripts of controls and updates
# on a player of a 2 s clip, from its own SplitMix64, with the time, play
# state and effective rate after each. Every time is a dyadic fraction and
# every rate a power of two or 0, so both implementations compute exactly
# and must agree to the bit.
#
# usage: player_oracle.py [--check]

import math
import os
import sys

PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "test",
                    "generated", "player_cases.h")

NAN = math.nan
END = 2.0

IDLE, RUNNING, PAUSED, FINISHED = 0, 1, 2, 3
UPDATE, PLAY, PAUSE, SEEK, FINISH, RATE = 0, 1, 2, 3, 4, 5
SUCCESS, REFUSED = 0, 1


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


def resolved(x):
    return not math.isnan(x)


class Animation:
    """An animation on a monotonically increasing, active timeline."""

    def __init__(self):
        self.start = NAN
        self.hold = NAN
        self.previous = NAN
        self.rate = 1.0
        self.pending_rate = None
        self.pending_play = False
        self.timeline = 0.0

    def effective_rate(self):
        return self.pending_rate if self.pending_rate is not None else self.rate

    # 4.4.3: the current time.
    def current(self):
        if resolved(self.hold):
            return self.hold
        if not resolved(self.start):
            return NAN
        return (self.timeline - self.start) * self.rate

    # 4.4.17: the play state.
    def state(self):
        t = self.current()
        if not resolved(t) and not resolved(self.start) and not self.pending_play:
            return IDLE
        if not resolved(self.start) and not self.pending_play:
            return PAUSED
        rate = self.effective_rate()
        if resolved(t) and ((rate > 0 and t >= END) or (rate < 0 and t <= 0)):
            return FINISHED
        return RUNNING

    def apply_pending_rate(self):
        if self.pending_rate is not None:
            self.rate = self.pending_rate
            self.pending_rate = None

    # 4.4.12: updating the finished state.
    def update_finished(self, did_seek):
        if did_seek:
            unconstrained = self.current()
        else:
            unconstrained = NAN if not resolved(self.start) else \
                (self.timeline - self.start) * self.rate
        if resolved(unconstrained) and resolved(self.start) and not self.pending_play:
            if self.rate > 0 and unconstrained >= END:
                if did_seek:
                    self.hold = unconstrained
                else:
                    p = self.previous
                    self.hold = END if not resolved(p) else max(p, END)
            elif self.rate < 0 and unconstrained <= 0:
                if did_seek:
                    self.hold = unconstrained
                else:
                    p = self.previous
                    self.hold = 0.0 if not resolved(p) else min(p, 0.0)
            elif self.rate != 0:
                if did_seek and resolved(self.hold):
                    self.start = self.timeline - self.hold / self.rate
                self.hold = NAN
        self.previous = self.current()

    # 4.4.4: silently setting, and setting, the current time.
    def silently_set(self, seek):
        if resolved(self.hold) or not resolved(self.start) or self.rate == 0:
            self.hold = seek
        else:
            self.start = self.timeline - seek / self.rate
        self.previous = NAN

    def set_current(self, seek):
        self.silently_set(seek)
        self.update_finished(True)

    # 4.4.10: playing, with auto-rewind.
    def play(self, auto_rewind=True):
        seek = NAN
        t = self.current()
        rate = self.effective_rate()
        if auto_rewind:
            if rate >= 0 and (not resolved(t) or t < 0 or t >= END):
                seek = 0.0
            elif rate < 0 and (not resolved(t) or t <= 0 or t > END):
                seek = END
        # An idle animation plays whatever the auto-rewind flag.
        if not resolved(seek) and not resolved(self.start) and not resolved(t):
            seek = 0.0
        if resolved(seek):
            self.hold = seek
        if resolved(self.hold):
            self.start = NAN
        self.pending_play = False
        if not resolved(self.hold) and not resolved(seek) and self.pending_rate is None:
            return SUCCESS
        self.pending_play = True
        self.update_finished(False)
        return SUCCESS

    # The pending play's commit, at its ready time: the next update's.
    def commit_play(self):
        ready = self.timeline
        if resolved(self.hold):
            self.apply_pending_rate()
            self.start = ready if self.rate == 0 else ready - self.hold / self.rate
            if self.rate != 0:
                self.hold = NAN
        elif resolved(self.start) and self.pending_rate is not None:
            match = (ready - self.start) * self.rate
            self.apply_pending_rate()
            if self.rate == 0:
                self.hold = match
            self.start = ready if self.rate == 0 else ready - match / self.rate
        self.pending_play = False
        self.update_finished(False)

    # 4.4.11: pausing, its pending pause ready at once.
    def pause(self):
        if self.state() == PAUSED:
            return SUCCESS
        if not resolved(self.current()):
            self.hold = 0.0 if self.rate >= 0 else END
        self.pending_play = False
        # The pending pause's commit.
        if resolved(self.start) and not resolved(self.hold):
            self.hold = (self.timeline - self.start) * self.rate
        self.apply_pending_rate()
        self.start = NAN
        self.update_finished(False)
        return SUCCESS

    # 4.4.14: finishing.
    def finish(self):
        rate = self.effective_rate()
        if rate == 0:
            return REFUSED
        self.apply_pending_rate()
        limit = END if self.rate > 0 else 0.0
        self.silently_set(limit)
        if not resolved(self.start):
            self.start = self.timeline - limit / self.rate
        if self.pending_play and resolved(self.start):
            self.pending_play = False
        self.update_finished(True)
        return SUCCESS

    # 4.4.15.2: seamlessly updating the playback rate, Maul Tween's rule in
    # its finished branch.
    def set_rate(self, rate):
        previous_state = self.state()
        self.pending_rate = rate
        if self.pending_play:
            return SUCCESS
        if previous_state in (IDLE, PAUSED) or not resolved(self.current()):
            self.apply_pending_rate()
            return SUCCESS
        if previous_state == FINISHED:
            if rate != 0:
                # Maul Tween: from the time held, not the time not held.
                self.start = self.timeline - self.current() / rate
                self.apply_pending_rate()
                self.update_finished(True)
            else:
                self.start = self.timeline
                self.apply_pending_rate()
                self.update_finished(False)
            return SUCCESS
        # Maul Tween: a pending rate the next update applies, no play task.
        return SUCCESS

    # A moving player's pending rate, applied from the time it reached.
    def commit_rate(self):
        ready = self.timeline
        match = self.current()
        self.apply_pending_rate()
        if self.rate == 0:
            self.hold = match
        self.start = ready if self.rate == 0 else ready - match / self.rate

    def update(self, time):
        self.timeline = time
        if self.pending_play:
            self.commit_play()
        elif self.pending_rate is not None and resolved(self.start):
            self.commit_rate()
        self.update_finished(False)


def scripts():
    random = SplitMix64(0x6A09E667F3BCC908)
    out = []
    for _ in range(300):
        animation = Animation()
        ops = []
        for _ in range(12):
            kind = random.pick([UPDATE, UPDATE, UPDATE, PLAY, PAUSE, SEEK, FINISH, RATE, RATE])
            if kind == UPDATE:
                argument = animation.timeline + random.pick([0.125, 0.25, 0.5, 1.0, 2.0, 4.0])
                animation.update(argument)
                result = SUCCESS
            elif kind == PLAY:
                argument = 0.0
                result = animation.play()
            elif kind == PAUSE:
                argument = 0.0
                result = animation.pause()
            elif kind == SEEK:
                argument = random.pick([0.0, 0.5, 1.0, 1.75, 2.0, 3.0, -0.5])
                animation.set_current(argument)
                result = SUCCESS
            elif kind == FINISH:
                argument = 0.0
                result = animation.finish()
            else:
                argument = random.pick([-2.0, -1.0, -0.5, 0.0, 0.5, 1.0, 2.0])
                result = animation.set_rate(argument)
            ops.append((kind, argument, result, animation.current(), animation.state(),
                        animation.effective_rate()))
        out.append(ops)
    return out


def number(value):
    if math.isnan(value):
        return "NAN_CASE"
    return float(value).hex()


def render():
    lines = [
        "// SPDX-License-Identifier: MIT",
        "// Copyright (c) 2026 Sirac Ozmen",
        "//",
        "// Generated by tools/oracle/player_oracle.py; do not edit. Scripts of",
        "// controls and updates on a player of a 2 s clip (0 an update to the",
        "// time given, 1 play, 2 pause, 3 seek, 4 finish, 5 a rate), with the",
        "// result (0 success, 1 refused), time, play state and effective rate",
        "// the oracle computes after each.",
        "// clang-format off",
        "",
        "#ifndef MTWN_PLAYER_CASES_H",
        "#define MTWN_PLAYER_CASES_H",
        "",
        "#define NAN_CASE ((double)NAN)",
        "",
        "static const PlayerScript s_playerScripts[] = {",
    ]
    for ops in scripts():
        steps = ", ".join(f"{{{k}, {number(a)}, {r}, {number(t)}, {s}, {number(rate)}}}"
                          for k, a, r, t, s, rate in ops)
        lines.append(f"    {{{{{steps}}}}},")
    lines += ["};", "", "#endif", ""]
    return "\n".join(lines)


def main():
    text = render()
    if len(sys.argv) > 1 and sys.argv[1] == "--check":
        current = open(PATH, encoding="utf-8").read() if os.path.exists(PATH) else ""
        if current != text:
            print("test/generated/player_cases.h is stale: run tools/oracle/player_oracle.py")
            return 1
        print("player cases: current")
        return 0
    os.makedirs(os.path.dirname(PATH), exist_ok=True)
    with open(PATH, "w", encoding="utf-8", newline="\n") as f:
        f.write(text)
    print("wrote " + os.path.normpath(PATH))
    return 0


if __name__ == "__main__":
    sys.exit(main())
