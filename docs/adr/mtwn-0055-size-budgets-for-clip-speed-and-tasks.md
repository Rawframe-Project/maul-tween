# mtwn-0055. Size budgets for the clip players' speed and their tasks

Status: Accepted

## Context

Record mtwn-0051 set each part's budget about 6% over its size at
0.2.0, and allows a part past its budget only by a record that names
the cost. Since 0.2.1 the clip players were made faster: crossings
searched from a guess (mtwn-0053), a track's velocity worked out when
read (mtwn-0054), sibling tracks sharing their timing, kept active
durations, walks remembered within a record, a level's shape kept. In
the bench's clip of 1,000 looping players, the average frame went from
22.5 to 5.9 million instructions and the frame where they repeat from
1,060 to 25.6 million. That used the clips part's margin: 104.2 kB at
0.2.1, 109.9 kB after.

Next, the clip players run on the host's task system (as Maul VFX's
work does), which adds the hooks, a split of each player's update into
work that writes only its own results and a serial step that composes
them, and per-task scratch. That is scope, not speed, and is estimated
at 4 to 6 kB.

## Decision

- Until 0.3.0, the clips part's budget is 118 kB and the whole
  library's 128 kB. The other parts keep theirs.
- At 0.3.0 every budget is restated by mtwn-0051's rule, about 6% over
  each part's size at that release.

## Consequences

The speed work is paid for in size, about 5.7 kB, and named here; the
check on every commit stays. A part growing past 118 kB before 0.3.0
again takes a record.

## Restated at 0.3.0

Measured at the release (x86-64, GCC 14 Release): curves 21.8 kB,
values 45.6, transitions 48.5, clips 114.3, everything 124.4. The
budgets, about 6% over each by mtwn-0051's rule: curves 23 kB, values
48, transitions 51, clips 121, everything 132. The interim budgets
end. Since the interim budgets the clips part grew 4.4 kB (109.9 to
114.3): 3.9 for the task system, against the 4 to 6 estimated, and the
other 0.5 for the reviews' answers since (the reduced-motion count
among them);
transitions grew 1.2 kB with the context's planning of the groups'
scratch.


## Amended 2026-10-16: the clips' budget for composing in the groups

After 0.3.0 the clips part grew from 114.3 to 120.7 kB with the values'
speed (mtwn-0058) and the crossings' fixes (mtwn-0037, mtwn-0038,
mtwn-0042, mtwn-0053), inside its 121 kB budget. Composing in the
groups what one player alone writes (mtwn-0056, amended) takes it to
124.7 kB and the whole library to 135.1: the counting of writers, the
groups' marking, composing and showing bases with their changes
staged, the serial pass's merge, and the checks that a player fits.
Savings were looked for first: keeping `mtwnSampleClip` out of line
saved nothing, and a smaller form whose shared and own paths both
called one function cost the bench's frames 2% more instructions
without the hooks, so it was not taken.

Until 0.4.0 the clips part's budget is 127 kB and the whole library's
138 kB; the other parts keep theirs. At 0.4.0 every budget is restated
by mtwn-0051's rule.

## Amended 2026-10-17: restated at 0.4.0

At 0.4.0 every budget is restated by mtwn-0051's rule, about 6% over
each part's size then (GCC 14, x86-64, `-O2`): curves 21.8 kB, budget
23 kB; values 45.6, 48; transitions 49.5, 52 (51 before); clips 125.1,
132 (127); everything 135.8, 144 (138). Since 0.3.1 the parts grew by
the sharing's capacities in the def, the count of advances left
serial, and the writers kept as they change (transitions 0.3 kB, clips
0.4 kB).

## Amended 2026-10-17: restated at 0.5.0

At 0.5.0 every budget is restated by mtwn-0051's rule, about 6% over
each part's size then (GCC 14, x86-64): curves 22.9 kB, budget 24 kB
(23 before); values 47.6, 50 (48); transitions 50.7, 54 (52); clips
127.4, 135 (132); everything 138.1, 146 (144). Since 0.4.1 the parts
grew by the guards the values' fuzz asked for: far phases reduced in
springs, punches and rotations, a rotation's overflowing cubic scaled,
the fit's limit and its finite points, the length's cap on rule pairs,
linear()'s overflowing span (curves 1.1 kB, values 1.8 kB, the parts
above them by what they call). By object (GCC 14, bytes, 0.4.1 to
0.5.0's code): spring +800, the far phases' reduction inlined into its
three callers; path +442, a rotation's cubic scaled where it overflows
and the length's cap on rule pairs; rotation +291, a far progress
reduced by whole turns; css_easing +128, linear()'s span; clip +128,
the refusal of an incremental track of endless iterations of no
duration; sample +94, the fit's limit and its finite points; decay +65
and signal +64, the rubber band's and the punch's and shake's guards;
easing -32 (back's form), timing -24 and crossing -48 (the first
iteration's floor, after 0.5.0). Every growth is a guard a defect asked
for; none is a feature's.

## Amended 2026-10-18: restated at 0.6.0

At 0.6.0 every budget is restated by mtwn-0051's rule, about 6% over
each part's size then (GCC 14, x86-64): curves 25.0 kB, budget 27 kB
(25 before); values 56.3, 60 (57); transitions 56.6, 60 (58); clips
138.8, 147 (139); everything 155.4, 165 (156). Since 0.5.1 the parts
grew by features, each restating its part's budget by its own record
as it came (mtwn-0059 to mtwn-0067): curves 2.1 kB, values 8.8 kB,
transitions 5.9 kB, clips 11.4 kB, everything 17.4 kB. By object (GCC
14, bytes, 0.5.1 to 0.6.0's code): path and path_length +4,119 (the
arc length moved to a file of its own, scale tracks over their
logarithms, monotone segments; path -1,871, path_length +5,990);
transition +3,569, bounded and landing decays; zoom +2,778 and scale
+466, van Wijk and Nuij's zoom and the scale mix; path_direction
+1,815; rough +1,319; inertia +1,111; clip_sample +1,087, scale tracks
composed and moved on, the moving on out of line; compose +955, scales
in their logarithm; update +877, scales and coasts in the update; coast
+554; signal +488, the shake's noise on its own clock; target +129;
spring +40. Every growth is a feature's or its guard's.

## Amended 2026-10-18: a ceiling over the last release

The budgets bound a part between records, and each feature's record
raised its part's as it came: from 0.5.1 to 0.6.0 they rose by 12.6%
overall and 18.5% for the values, every step reasoned, nothing bounding
the sum. `code_size.py` now keeps each part's size at the last release
(0.6.0: curves 24,985 bytes, values 56,281, transitions 56,601, clips
138,762, everything 155,353) and a growth of 10%: `--budget` fails a
budget set past its part's ceiling unless `RAISED` names the record
that allows it, so a release cycle's growth past 10% is decided as
such, not reached step by step. The table moves at each release with
the budgets.


## Amended 2026-10-18: restated at 0.7.0

At 0.7.0 every budget is restated by mtwn-0051's rule, about 6% over
each part's size then (GCC 14, x86-64): curves 25.0 kB, budget 27 kB
(unchanged); values 56.4, 60 (unchanged); transitions 59.5, 63 (60);
clips 143.0, 152 (147); everything 159.7, 169 (165). Since 0.6.1 (GCC 14,
bytes): curves +0, values -192, transitions +832, clips +704, everything
+640. By object: update +896, of which the mix's test of t (record
mtwn-0070, every mix `t == 1 ? b : a + t (b - a)`) about +960, which GCC
compiles into each loop's two versions, and the unreachable number
branch removed after the coverage audit about -512 (`update.c` alone at
-O3: 13,291 bytes at 0.6.1, 12,779 without the branch, 13,739 with the
new mix); the rest within 256 bytes either way, the reserved bytes'
checks (record mtwn-0071: timing, transition, context and path +64 to
+80, player +16) and the mix's form (color -112, signal -64, rotation
-48) among them, clip -256 and stagger -98. No instruction count moved
with the mix (record mtwn-0058's targets and the bench's frames).

## Amended 2026-10-18: 1.0.0

At 1.0.0 the parts are 0.7.0's to the byte (GCC 14, x86-64; the misuse
count's rename moves no code), so the budgets restated at 0.7.0 stand.
Clang 20's clips and everything are 0.1 kB more.
