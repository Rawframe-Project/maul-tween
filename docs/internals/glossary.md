# Glossary

Terms the code, the tests and the documentation use with one meaning.

- **Progress:** a timing's position in its iteration, from 0 to 1. **Eased progress** is what an easing makes of it, which may leave 0 to 1.
- **Easing:** a curve from progress to eased progress, with a **mode** (in, out, in-out, out-in) that reflects it. **Monotonic:** never decreasing, which a timeline's easing must be.
- **Values layer:** the functions with no owner: easings, springs, decay, interpolation, paths, timing, staggers. Safe from any thread.
- **Context:** the one owner object, a block holding targets, players, clips' records and every capacity.
- **Target:** one animated value of a fixed **kind**, with a **base** value the host sets, a current **value** animations compose over the base, and a **velocity**.
- **Kind:** what a value is: scalar, vectors, color, angle, rotation, integer, discrete or scale. It fixes the channels used and how the value interpolates and composes.
- **Channel:** one float of a value's four.
- **Mix:** an interpolation between two values, made once and sampled at any progress: a color mix, a rotation mix, a scale mix, a zoom mix.
- **Def:** the struct a context, clip, track, timeline, transition or player is made from, started from its default function, carrying a **cookie** and a **version**.
- **Id:** a 1-based index and a generation naming a target, player or clip. A **stale** id names one destroyed since.
- **Misuse:** a call on a live context refused for an invalid argument, a stale id or a call back in, counted on the context.
- **Transition:** a player that moves one target to a value by an easing, a spring or a decay, started from the target's current value.
- **Spec:** how a transition moves: an easing, a spring or a decay.
- **Clip:** an immutable description of tracks on timelines, built node by node and **sealed** before it plays.
- **Node:** a clip's track, timeline, label or mark, named by its 1-based index; node 1 is the **root** timeline.
- **Track:** keyframes of one value kind on a **slot**. A **path** is the curve through keyframes of one to four channels that a track is built on.
- **Keyframe:** a value at an offset from 0 to 1, starting a **segment** of a kind (linear, step, Bezier, Catmull-Rom, monotone) with an easing.
- **Slot:** a clip's numbered output, bound by a player to a target.
- **Timeline:** a node whose children take its transformed time. **Group** and **sequence** are timelines filled in parallel or one after another.
- **Position:** where a child starts in its parent, resolved to a start time when it is added.
- **Label:** a named time in a timeline, to place children at and seek to. **Mark:** an instant that gives an event each time it is crossed.
- **Player:** what plays a clip on targets by Web Animations' procedures. **Kept:** not freed when it finishes.
- **Binding:** a slot of a clip bound to a target, with a **composite** operation (replace or add) and a **weight**.
- **Underlying value:** what lies under a player at a target: what earlier players wrote in this update, or the base.
- **Commit:** writing a finished player's or transition's final value into its targets' bases.
- **Crossing:** a point (an active start or end, an iteration start, a mark) that a player's time passes. A **sweep** counts the crossings from one time to another.
- **Record:** an event in the context's queue: one per player, node or mark, kind and direction an update crosses, with its count.
- **Host time:** the host's clock in integer nanoseconds, passed to each update. **Local time:** a node's time, from its parent's.
- **Floating-point environment:** a thread's rounding, subnormal and exception settings, which the library sets for itself during each call.
- **Held:** a number past the floats' range kept at the largest finite float.
- **Reduced motion:** a context's setting under which what can finish finishes at the next update. **Essential:** marked to play on all the same.
- **Group (tasks):** one of up to 16 work items an update hands the host's task system, each advancing some clips' players with its own scratch.
- **In step:** players of one clip at the same time and rate, which share their sample and records.
