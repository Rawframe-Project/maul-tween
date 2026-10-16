# Maul Tween API reference

Generated from the public headers by `tools/gen_api.py`. The headers
are the source of truth; this file mirrors them.

## `base.h`

The base of the Maul Tween API: the library version, the export and attribute macros, the result codes every fallible function returns, the allocator a context takes, and the four-channel values and their kinds that targets and tracks hold.

```c
mtwnVersion mtwnGetVersion(void);
```
Returns the version of the library that was linked, which may differ from the MTWN_VERSION macros a program was compiled with.  @return The library version. @par Thread safety Safe from any thread.

```c
const char* mtwnResultName(mtwnResult result);
```
Returns the name of a result code, for diagnostics.  @param result  Any value; an unknown one is named as such. @return A static, NUL-terminated string such as "mtwn_errorCapacity". @par Thread safety Safe from any thread.

## `clip.h`

Clips (records mtwn-0004, mtwn-0013, mtwn-0033 and mtwn-0034): immutable descriptions of tracks on nested timelines with labels and marks, built node by node at positions resolved when added, then finished. A finished clip is read through the values layer from any thread while it lives, and plays on any set of targets, its tracks addressing slots.

```c
static inline mtwnClipDef mtwnDefaultClipDef(void);
```
Makes a clip def for a small clip: 16 nodes, 32 keyframes, 8 slots, about 13 KB of clip storage (mtwnGetClipBytes says how much). Built in the program, so it carries the version of the headers the program includes.  @return The def. @par Thread safety Safe from any thread.

```c
static inline mtwnClipTrackDef mtwnDefaultClipTrackDef(void);
```
Makes a clip track def: on the root at time 0, slot 0, a scalar track (mtwnDefaultTrackDef), Web Animations' default timing with a duration of 1 second, no keyframes. Built in the program, so it carries the version of the headers the program includes.  @return The def. @par Thread safety Safe from any thread.

```c
static inline mtwnClipTimelineDef mtwnDefaultClipTimelineDef(void);
```
Makes a clip timeline def: on the root at time 0, lasting to its latest child's end, Web Animations' default timing. Built in the program, so it carries the version of the headers the program includes.  @return The def. @par Thread safety Safe from any thread.

```c
MTWN_NODISCARD MTWN_API mtwnResult mtwnGetClipBytes(const mtwnClipDef* def, uint64_t* bytesOut);
```
Gives the clip storage a clip of a def takes, so a host can size its context's clipStorageBytes for the clips it makes.  @param def         The clip's def. @param bytesOut    Receives the bytes. @return mtwn_success; mtwn_errorInvalid for a null pointer, a def without its cookie or a capacity past MTWN_MAX_COUNT; mtwn_errorVersion for a def of another version. @par Thread safety Safe from any thread.

```c
MTWN_NODISCARD MTWN_API mtwnResult mtwnCreateClip(mtwnContext* context, const mtwnClipDef* def, mtwnClipId* clipOut);
```
Makes a clip: allocates its storage in one block, counted against the context's clip storage. Its root timeline, node 1, lasts as long as its latest child.  @param context  The context. @param def      The def. @param clipOut  Receives the clip's id, or the null id on failure. @return mtwn_success; mtwn_errorInvalid for a null argument, a def without its cookie or a capacity past MTWN_MAX_COUNT; mtwn_errorVersion for a def of another major or minor version; mtwn_errorLimit when the context holds its capacity of clips or the storage would pass its clip storage; mtwn_errorCapacity when the allocator fails. @par Thread safety Safe from any thread; the context is used by one thread at a time.

```c
MTWN_NODISCARD MTWN_API mtwnResult mtwnDestroyClip(mtwnContext* context, mtwnClipId clip);
```
Destroys a clip and frees its storage.  @param context  The context. @param clip     The clip. @return mtwn_success; mtwn_errorInvalid for a null context; mtwn_errorStale for an id that names no live clip; mtwn_errorBusy while a player plays it. @par Thread safety Safe from any thread; the context is used by one thread at a time.

```c
bool mtwnClip_IsValid(const mtwnContext* context, mtwnClipId clip);
```
Tells whether an id names a live clip. Not counted as misuse.  @param context  The context, or NULL. @param clip     The id. @return Whether the clip is alive. @par Thread safety Safe from any thread; the context is used by one thread at a time.

```c
MTWN_NODISCARD MTWN_API mtwnResult mtwnClip_AddTrack(mtwnContext* context, mtwnClipId clip, const mtwnClipTrackDef* def, mtwnNode* nodeOut);
```
Adds a track: copies its keyframes into the clip, makes the track there (mtwnMakeTrack, with the lengths and mixes in the clip) and resolves its position to a start time.  @param context  The context. @param clip     The clip, not finished. @param def      The def: a timeline of the clip as parent, a known position kind and a finite offset, a slot below the slot capacity and of the same kind as its other tracks, a timing mtwnValidateTiming accepts, and a track and keyframes mtwnMakeTrack accepts. @param nodeOut  Receives the node, or 0 on failure. @return mtwn_success; mtwn_errorInvalid for a null argument, a def without its cookie or outside the above; mtwn_errorVersion for a def of another version; mtwn_errorStale for a clip that is not alive; mtwn_errorState for a finished clip; mtwn_errorLimit when the clip holds its capacity of nodes or keyframes. @par Thread safety Safe from any thread; the context is used by one thread at a time.

```c
MTWN_NODISCARD MTWN_API mtwnResult mtwnClip_AddTimeline(mtwnContext* context, mtwnClipId clip, const mtwnClipTimelineDef* def, mtwnNode* nodeOut);
```
Adds a timeline: a group of children taking its transformed time. Its end in its parent follows its children as they arrive when its duration is theirs; children already placed after it keep their starts.  @param context  The context. @param clip     The clip, not finished. @param def      The def: a timeline of the clip as parent, a position as for tracks, a timing mtwnValidateTiming accepts with a monotonic easing (mtwnIsEasingMonotonic). @param nodeOut  Receives the node, or 0 on failure. @return As mtwnClip_AddTrack; mtwn_errorLimit also for a timeline past MTWN_MAX_CLIP_DEPTH. @par Thread safety Safe from any thread; the context is used by one thread at a time.

```c
MTWN_NODISCARD MTWN_API mtwnResult mtwnClip_AddLabel(mtwnContext* context, mtwnClipId clip, mtwnNode parent, mtwnPosition position, uint32_t key, mtwnNode* nodeOut);
```
Adds a label: a time in its parent named by a host key, unique in the parent, for positions and seeking. It is not a previous child and does not lengthen its parent.  @param context   The context. @param clip      The clip, not finished. @param parent    A timeline of the clip. @param position  Where, as for tracks. @param key       The host's key. @param nodeOut   Receives the node, or 0 on failure. @return As mtwnClip_AddTrack; mtwn_errorInvalid also for a key the parent already has. @par Thread safety Safe from any thread; the context is used by one thread at a time.

```c
MTWN_NODISCARD MTWN_API mtwnResult mtwnClip_AddMark(mtwnContext* context, mtwnClipId clip, mtwnNode parent, mtwnPosition position, uint32_t key, mtwnNode* nodeOut);
```
Adds a mark: an instant in its parent that gives an event, with the host's key, each time playback crosses it. It is not a previous child and does not lengthen its parent.  @param context   The context. @param clip      The clip, not finished. @param parent    A timeline of the clip. @param position  Where, as for tracks. @param key       The host's key; marks may share one. @param nodeOut   Receives the node, or 0 on failure. @return As mtwnClip_AddTrack. @par Thread safety Safe from any thread; the context is used by one thread at a time.

```c
MTWN_NODISCARD MTWN_API mtwnResult mtwnClip_Finish(mtwnContext* context, mtwnClipId clip);
```
Finishes a clip: it can be read and played, and no longer changed.  @param context  The context. @param clip     The clip. @return mtwn_success; mtwn_errorInvalid for a null context; mtwn_errorStale for a clip that is not alive; mtwn_errorState for a clip already finished. @par Thread safety Safe from any thread; the context is used by one thread at a time.

```c
MTWN_NODISCARD MTWN_API mtwnResult mtwnClip_GetTrack(mtwnContext* context, mtwnClipId clip, mtwnNode node, mtwnClipTrack* trackOut);
```
Reads a finished clip's track node.  @param context   The context. @param clip      The clip. @param node      A track node of the clip. @param trackOut  Receives the track, its timing, start, parent and slot; sample it with mtwnComputeTiming and mtwnSampleTrack from any thread while the clip lives. @return mtwn_success; mtwn_errorInvalid for a null argument or a node that is not a track of the clip; mtwn_errorStale for a clip that is not alive; mtwn_errorState for one not finished. @par Thread safety Safe from any thread; the context is used by one thread at a time.

```c
MTWN_NODISCARD MTWN_API mtwnResult mtwnClip_GetNode(mtwnContext* context, mtwnClipId clip, mtwnNode node, mtwnClipNodeInfo* infoOut);
```
Reads any node of a finished clip.  @param context  The context. @param clip     The clip. @param node     A node of the clip, the root included. @param infoOut  Receives what the node is. @return As mtwnClip_GetTrack, for any node. @par Thread safety Safe from any thread; the context is used by one thread at a time.

```c
MTWN_NODISCARD MTWN_API mtwnResult mtwnClip_FindLabel(mtwnContext* context, mtwnClipId clip, mtwnNode parent, uint32_t key, double* secondsOut);
```
Finds a label of a finished clip by its parent and key.  @param context     The context. @param clip        The clip. @param parent      The timeline the label is in. @param key         The label's key. @param secondsOut  Receives its time in the parent. @return mtwn_success; mtwn_errorInvalid for a null argument, a parent that is not a timeline of the clip, or no such label; mtwn_errorStale and mtwn_errorState as mtwnClip_GetTrack. @par Thread safety Safe from any thread; the context is used by one thread at a time.

```c
MTWN_NODISCARD MTWN_API mtwnResult mtwnClip_GetDuration(mtwnContext* context, mtwnClipId clip, double* secondsOut);
```
Reads a finished clip's duration: its root's end.  @param context     The context. @param clip        The clip. @param secondsOut  Receives the duration. @return As mtwnClip_GetTrack, without the node. @par Thread safety Safe from any thread; the context is used by one thread at a time.

## `context.h`

The context (records mtwn-0003, mtwn-0006 and mtwn-0028): the owner of targets, clips and players, made from a def that names every capacity and allocated in one block through the host's allocator. Nothing allocates after creation.

```c
static inline mtwnContextDef mtwnDefaultContextDef(void);
```
Makes a context def for a medium host: 1024 targets, 256 players, 16 clips, 256 events, 1024 bindings and 208 KiB of clip storage, which holds 16 clips of the default clip def, the C library's allocator and no task system. Built in the program, so it carries the version of the headers the program includes.  @return The def. @par Thread safety Safe from any thread.

```c
MTWN_NODISCARD MTWN_API mtwnResult mtwnCreateContext(const mtwnContextDef* def, mtwnContext** contextOut);
```
Makes a context: checks the def and allocates everything the context holds in one block.  @param def         The def. @param contextOut  Receives the context, or NULL on failure. @return mtwn_success; mtwn_errorInvalid for a null argument, a def without its cookie, an allocator or a task system with one function of the two, or a capacity past its maximum; mtwn_errorVersion for a def built against another major or minor version, before anything else in it is read; mtwn_errorCapacity when the allocator fails or the block's size does not fit. @par Thread safety Safe from any thread.

```c
void mtwnDestroyContext(mtwnContext* context);
```
Destroys a context and everything it holds.  @param context  The context, or NULL. @par Thread safety Safe from any thread; the context is used by one thread at a time.

```c
MTWN_NODISCARD MTWN_API mtwnResult mtwnGrowContext(mtwnContext** contextInOut, const mtwnContextDef* def);
```
Grows a context to a def's capacities, each at least the context's own, and takes the def's task system, with the allocator it was made with: the one time after its creation it allocates, and only when asked (it never grows by itself). Its objects move to one new block, their ids unchanged, and the old block is freed, so the context's address changes: the call writes the new one. On any failure nothing changes.  @param contextInOut  The context; receives the grown one. @param def           The capacities: start from the context's own. @return mtwn_success; mtwn_errorInvalid for a null argument, a def without its cookie, a capacity below the context's or past its limit, another allocator, or a task system with one function of the two; mtwn_errorVersion for a def of another version; mtwn_errorCapacity when the allocator fails. @par Thread safety Safe from any thread; the context is used by one thread at a time.

```c
uint64_t mtwnContext_GetMisuseCount(const mtwnContext* context);
```
Returns how many calls against the context were refused for an invalid argument or a stale id since it was made.  @param context  The context. @return The count. @par Thread safety Safe from any thread; the context is used by one thread at a time.

```c
uint64_t mtwnContext_GetMemoryBytes(const mtwnContext* context);
```
Returns the size of the block the context holds everything in.  @param context  The context. @return Bytes. @par Thread safety Safe from any thread; the context is used by one thread at a time.

```c
MTWN_NODISCARD MTWN_API mtwnResult mtwnContext_SetReducedMotion(mtwnContext* context, bool reduced);
```
Sets reduced motion (record mtwn-0049), off when a context is made. While it is on, every transition and every running clip's player with a finite end finishes at the next update, its delay skipped and its events at that update's time, unless its def marks it essential; a player with an infinite end plays on, the host's to keep, seek or stop. Turning it off reaches what has not finished. The host reads the platform's preference and sets it.  @param context  The context. @param reduced  Whether motion is reduced. @return mtwn_success; mtwn_errorInvalid for a null context. @par Thread safety Safe from any thread; the context is used by one thread at a time.

```c
bool mtwnContext_IsReducedMotion(const mtwnContext* context);
```
Returns whether motion is reduced.  @param context  The context, or NULL. @return Whether it is; false for a null context. @par Thread safety Safe from any thread; the context is used by one thread at a time.

```c
MTWN_NODISCARD MTWN_API mtwnResult mtwnUpdate(mtwnContext* context, uint64_t timeNs, mtwnTargetId* changedOut, uint32_t changedCapacity, uint32_t* changedTotalOut);
```
Advances every player to the host's time, in the order they were made, later ones writing over earlier ones: transitions sample their curves, write their targets and end; clips' players sample their clips into their bound targets, and those that finish, unless kept, commit what they fill forward as their targets' bases and are freed. A target bound to a clip's player that no player writes shows its base. Reports the targets whose value or base changed since the last report, in the order they changed; a target appears once.  @param context          The context. @param timeNs           The host's time in nanoseconds, never less than the last update's. @param changedOut       Receives the first changedCapacity changed targets; NULL only with a capacity of 0. @param changedCapacity  Its length. @param changedTotalOut  Receives how many targets changed, which may exceed the capacity; those past it are left out of the report only. May be NULL. @return mtwn_success; mtwn_errorInvalid for a null context, a null array with a capacity, or a time before the last update's, nothing advanced. @par Thread safety Safe from any thread; the context is used by one thread at a time.

## `easing.h`

Easing: curves that map a timing's progress to eased progress (record mtwn-0011). An easing is a value made by a function that validates it and sampled by mtwnEase with no status to check: Penner's families, which land exactly on 0 and 1, and CSS Easing Level 2 entire. Every kind has the same four modes.

```c
mtwnEasing mtwnLinearEasing(void);
```
Returns the linear easing, whose eased progress is its progress.  @return The easing. @par Thread safety Safe from any thread.

```c
MTWN_NODISCARD MTWN_API mtwnResult mtwnMakePowerEasing(mtwnEaseMode mode, double exponent, mtwnEasing* easingOut);
```
Makes the power easing p^k: quad for k = 2, cubic 3, quart 4, quint 5. GSAP's power(n) is k = n + 1. Integer exponents up to 8 are computed by multiplication.  @param mode        The mode. @param exponent    k, finite and above 0. @param easingOut   Receives the easing. @return mtwn_success; mtwn_errorInvalid for a null pointer, an unknown mode or an exponent outside the above. @par Thread safety Safe from any thread.

```c
MTWN_NODISCARD MTWN_API mtwnResult mtwnMakePennerEasing(mtwnEasingKind kind, mtwnEaseMode mode, mtwnEasing* easingOut);
```
Makes one of Penner's easings without parameters: sine, expo, circ or bounce. Each lands exactly on 0 at 0 and on 1 at 1; expo is normalized to do so and differs from Penner's by at most 2^-10. GSAP 3's expo is not Penner's but a blend of its own, 2^(10 (p - 1)) p + p^6 (1 - p), up to 2^-7 from this one.  @param kind       mtwn_easingSine, mtwn_easingExpo, mtwn_easingCirc or mtwn_easingBounce. @param mode       The mode. @param easingOut  Receives the easing. @return mtwn_success; mtwn_errorInvalid for a null pointer, another kind or an unknown mode. @par Thread safety Safe from any thread.

```c
MTWN_NODISCARD MTWN_API mtwnResult mtwnMakeBackEasing(mtwnEaseMode mode, double overshoot, mtwnEasing* easingOut);
```
Makes the back easing p^2 ((s + 1) p - s), normalized to land on 1. GSAP's back(s) and Penner's in and out use s as given, 1.70158 by default; Penner's and tween.js's in-out use 1.525 s, which is this easing with that s.  @param mode       The mode. @param overshoot  s, finite and at least 0; 0 is the cubic. @param easingOut  Receives the easing. @return mtwn_success; mtwn_errorInvalid for a null pointer, an unknown mode or an overshoot outside the above. @par Thread safety Safe from any thread.

```c
MTWN_NODISCARD MTWN_API mtwnResult mtwnMakeElasticEasing(mtwnEaseMode mode, double amplitude, double period, mtwnEasing* easingOut);
```
Makes the elastic easing, GSAP's elastic(amplitude, period): out is 1 + a 2^(-10 p) sin((p - c) 2 pi / P) with c = P asin(1 / a) / (2 pi), normalized to land on 0 and 1, and in its reflection. An amplitude below 1 is 1 with the period divided by it, as GSAP does. GSAP's defaults are amplitude 1 with period 0.3, or 0.45 in-out; Penner's and tween.js's are amplitude 1, period 0.4. GSAP and tween.js miss 1 at the end by up to a 2^-10 and set it there, so they differ from this by at most a (1 + a) 2^-10.  @param mode       The mode. @param amplitude  a, finite and above 0. @param period     P, finite and above 0, in units of progress. @param easingOut  Receives the easing. @return mtwn_success; mtwn_errorInvalid for a null pointer, an unknown mode or a parameter outside the above. @par Thread safety Safe from any thread.

```c
MTWN_NODISCARD MTWN_API mtwnResult mtwnMakeHostEasing(mtwnEaseMode mode, mtwnEaseFunction function, void* context, mtwnEasing* easingOut);
```
Makes an easing of a host function, called with progress clamped to 0 to 1, under the mode's reflections. The library neither checks nor forces its ends.  @param mode       The mode. @param function   The function; it must be safe to call from every thread that samples the easing. @param context    Passed to the function; may be NULL. @param easingOut  Receives the easing. @return mtwn_success; mtwn_errorInvalid for a null function or easingOut or an unknown mode. @par Thread safety Safe from any thread.

```c
MTWN_NODISCARD MTWN_API mtwnResult mtwnMakeCubicBezier(double x1, double y1, double x2, double y2, mtwnEasing* easingOut);
```
Makes CSS's cubic-bezier(x1, y1, x2, y2), solved by safeguarded Newton to |x(t) - x| <= 1e-14 (record mtwn-0009): exactly 0 at 0 and 1 at 1, and outside 0 to 1 extended along CSS's end tangents.  @param x1         The first control point's x, from 0 to 1. @param y1         Its y, finite. @param x2         The second control point's x, from 0 to 1. @param y2         Its y, finite. @param easingOut  Receives the easing, in mode mtwn_easeIn. @return mtwn_success; mtwn_errorInvalid for a null pointer or a point outside the above. @par Thread safety Safe from any thread.

```c
MTWN_NODISCARD MTWN_API mtwnResult mtwnMakeSteps(uint32_t count, mtwnStepPosition position, mtwnEasing* easingOut);
```
Makes CSS's steps(count, position), with CSS's before flag: in the before phase, a progress landing exactly on a step takes the step below.  @param count      The steps: at least 1, or 2 for mtwn_jumpNone. @param position   Where it jumps. @param easingOut  Receives the easing, in mode mtwn_easeIn. @return mtwn_success; mtwn_errorInvalid for a null pointer, an unknown position or a count outside the above. @par Thread safety Safe from any thread.

```c
MTWN_NODISCARD MTWN_API mtwnResult mtwnMakeCssEasing(mtwnCssKeyword keyword, mtwnEasing* easingOut);
```
Makes the easing a CSS keyword names.  @param keyword    The keyword. @param easingOut  Receives the easing, in mode mtwn_easeIn. @return mtwn_success; mtwn_errorInvalid for a null pointer or an unknown keyword. @par Thread safety Safe from any thread.

```c
MTWN_NODISCARD MTWN_API mtwnResult mtwnMakeLinearEasing(mtwnLinearPoint* points, uint32_t count, mtwnEasing* easingOut);
```
Makes CSS's linear() from its points, canonicalizing them in place as CSS does: a first input not given is 0 and a last one 1 or the largest before it; an input below an earlier one is raised to it; runs not given are spread evenly between their neighbors. The easing references the points, which must outlive it and not change. Outside the points' inputs it extends their end segments.  @param points     The points, in order: at least 2, each output finite and each input finite or NaN. @param count      The number of points. @param easingOut  Receives the easing, in mode mtwn_easeIn. @return mtwn_success; mtwn_errorInvalid for a null pointer or points outside the above, leaving the points as they were. @par Thread safety Safe from any thread; the points are used by one thread at a time.

```c
MTWN_NODISCARD MTWN_API mtwnResult mtwnSetEaseMode(mtwnEasing* easing, mtwnEaseMode mode);
```
Changes an easing's mode, keeping its curve.  @param easing  An easing made by this library. @param mode    The mode. @return mtwn_success; mtwn_errorInvalid for a null pointer or an unknown mode, leaving the easing as it was. @par Thread safety Safe from any thread.

```c
bool mtwnIsEasingMonotonic(const mtwnEasing* easing);
```
Tells whether an easing never decreases, as a timeline's easing must: linear, power, sine, expo, circ and steps in every mode, a cubic bezier with both y from 0 to 1, linear() with outputs that never decrease; not back, elastic, bounce or a spring, and not a host function, which the library cannot inspect.  @param easing  An easing made by this library. @return Whether it never decreases. @par Thread safety Safe from any thread.

```c
double mtwnEase(const mtwnEasing* easing, double progress, bool before);
```
Samples an easing. CSS's curves follow CSS Easing Level 2 outside 0 to 1; Penner's curves, springs and host functions see progress clamped to 0 to 1. The modes reflect the curve, and the before flag reaches it where it samples progress unreflected.  @param easing    An easing made by this library. @param progress  The progress, finite. @param before    Whether the timing is in its before phase; it moves only curves that jump at their start. @return The eased progress: exactly 0 at 0 and 1 at 1 for Penner's curves, springs, linear and cubic bezier; what CSS defines for steps and linear(). @par Thread safety Safe from any thread.

## `event.h`

Events (records mtwn-0004, mtwn-0013 and mtwn-0038): records in the context's bounded queue, never callbacks. Each update appends what its players crossed, at most one record per player, node or mark, kind and direction, with the count of crossings, ordered by the time of the first crossing, then the players' creation order, then the clip's order; the host reads them when it likes. The queue holds the context def's eventCapacity records. One that arrives at a full queue is dropped and counted, never silently: mtwnReadEvents reports the drops since the last read, and mtwnContext_GetDroppedEvents all of them. Records arrive in the order they are made (players in creation order, each player's nodes in clip order), so a full queue keeps the first made and drops the rest of an update's; those kept are sorted.

```c
MTWN_NODISCARD MTWN_API mtwnResult mtwnReadEvents(mtwnContext* context, mtwnEvent* eventsOut, uint32_t capacity, uint32_t* countOut, uint64_t* droppedOut);
```
Takes the oldest records off the queue, as many as fit, and reports how many were dropped at a full queue since the last read.  @param context     The context. @param eventsOut   Receives the records; NULL only with a capacity of 0. @param capacity    Its length. @param countOut    Receives how many were taken. @param droppedOut  Receives the records dropped since the last read (the count then starts again); may be NULL. @return mtwn_success; mtwn_errorInvalid for a null context or countOut, or a null array with a capacity. @par Thread safety Safe from any thread; the context is used by one thread at a time.

```c
uint32_t mtwnContext_GetEventCount(const mtwnContext* context);
```
Returns how many records waited in the queue, not yet taken.  @param context  The context. @return The count; 0 for a null context. @par Thread safety Safe from any thread; the context is used by one thread at a time.

```c
uint64_t mtwnContext_GetDroppedEvents(const mtwnContext* context);
```
Returns how many records were dropped since the context was made, each arriving at a full queue (its capacity is the context def's eventCapacity).  @param context  The context. @return The count; 0 for a null context. @par Thread safety Safe from any thread; the context is used by one thread at a time.

## `interpolate.h`

Interpolation of values that are not plain numbers (records mtwn-0014, mtwn-0015, mtwn-0016, mtwn-0017, mtwn-0024 to mtwn-0026): colors in a named space as CSS Color 4 interpolates them, angles with CSS's hue methods, rotations by slerp, integers and discrete values, and punch and shake, the signals a host adds to a value.

```c
MTWN_NODISCARD MTWN_API mtwnResult mtwnMakeColorMix(mtwnColor from, mtwnColor to, mtwnColorSpace space, mtwnHueMethod hue, mtwnColorMix* mixOut);
```
Makes the interpolation between two colors in a space, following CSS Color 4: a powerless OkLCh hue (chroma at most 0.000004) takes the other color's, the hues are fixed up by the method, and the colors are premultiplied, the hue excepted.  @param from    The color at progress 0: finite, alpha from 0 to 1. @param to      The color at progress 1, likewise. @param space   The space to interpolate in. @param hue     The hue method, read for OkLCh only. @param mixOut  Receives the mix. @return mtwn_success; mtwn_errorInvalid for a null pointer, an unknown space or method, or a color outside the above. @par Thread safety Safe from any thread.

```c
mtwnColor mtwnMixColor(const mtwnColorMix* mix, double progress);
```
Samples a mix: its colors exactly at 0 and 1, between them the premultiplied coordinates interpolated and divided by the interpolated alpha, or kept as they are where that alpha is 0 or below. Progress outside 0 to 1 extends the line, with alpha clamped to 0 to 1; the channels are not clamped.  @param mix       A mix made by this library. @param progress  The progress, finite. @return The color. @par Thread safety Safe from any thread.

```c
MTWN_NODISCARD MTWN_API mtwnResult mtwnColorToSpace(mtwnColor color, mtwnColorSpace space, double coordinatesOut[4]);
```
Converts a color to a space's coordinates and its alpha, for a host that springs or composes in the space.  @param color           Finite, alpha from 0 to 1. @param space           The space. @param coordinatesOut  Receives the three coordinates and the alpha; an OkLCh hue in radians from 0 to 2 pi (CSS's degrees times pi / 180), NaN where it is powerless. @return mtwn_success; mtwn_errorInvalid for a null pointer, an unknown space or a color outside the above. @par Thread safety Safe from any thread.

```c
MTWN_NODISCARD MTWN_API mtwnResult mtwnColorFromSpace(const double coordinates[4], mtwnColorSpace space, mtwnColor* colorOut);
```
Converts a space's coordinates and an alpha back to a color.  @param coordinates  The three coordinates and the alpha; a NaN OkLCh hue counts as 0, as CSS's missing hue does. @param space        The space. @param colorOut     Receives the color, alpha clamped to 0 to 1. @return mtwn_success; mtwn_errorInvalid for a null pointer, an unknown space, or a coordinate or alpha not finite, a NaN hue excepted. @par Thread safety Safe from any thread.

```c
double mtwnMixAngle(double from, double to, mtwnAngleMethod method, double progress);
```
Interpolates an angle in radians: the end at progress 1 is moved by whole turns as the method asks, then the two are interpolated linearly. At 0 the result is from exactly, and at 1 to exactly when no turn was added.  @param from      The angle at progress 0, finite. @param to        The angle at progress 1, finite. @param method    The method; an unknown one is raw. @param progress  The progress, finite; outside 0 to 1 extends. @return The angle. @par Thread safety Safe from any thread.

```c
MTWN_NODISCARD MTWN_API mtwnResult mtwnMakeRotationMix(mtwnQuat from, mtwnQuat to, mtwnRotationPath path, mtwnRotationMix* mixOut);
```
Makes the spherical interpolation between two rotations, at constant angular speed, the shortest way or the longest.  @param from    The rotation at progress 0: finite, not zero; it is normalized. @param to      The rotation at progress 1, likewise. @param path    Which way. @param mixOut  Receives the mix. @return mtwn_success; mtwn_errorInvalid for a null pointer, an unknown path, a rotation outside the above, or the longest way between rotations so close (|q0 + q1| under 1e-6 once the path's sign is taken) that it turns a full circle about no defined axis. @par Thread safety Safe from any thread.

```c
mtwnQuat mtwnMixRotation(const mtwnRotationMix* mix, double progress);
```
Samples a rotation mix: its rotations normalized exactly at 0 and 1 (the second negated when the path needed it), between and beyond them slerp, of unit length to a float's rounding.  @param mix       A mix made by this library. @param progress  The progress, finite. @return The rotation. @par Thread safety Safe from any thread.

```c
int64_t mtwnMixInteger(int64_t from, int64_t to, double progress);
```
Interpolates an integer: the interpolation in double, rounded to nearest with ties away from zero, so it passes through every integer between. Exact at the ends, and between them for values within 2^53.  @param from      The value at progress 0. @param to        The value at progress 1. @param progress  The progress, finite; outside 0 to 1 extends, clamped to the type's range. @return The value. @par Thread safety Safe from any thread.

```c
uint32_t mtwnMixDiscrete(uint32_t from, uint32_t to, double progress);
```
Interpolates a discrete value the host indexes (an enumeration, a flag): from below progress 1/2, to from it, as Web Animations steps values it cannot interpolate.  @param from      The value at progress 0. @param to        The value at progress 1. @param progress  The progress. @return from or to. @par Thread safety Safe from any thread.

```c
MTWN_NODISCARD MTWN_API mtwnResult mtwnMakePunch(double frequency, double duration, double exponent, mtwnPunch* punchOut);
```
Makes a punch.  @param frequency  Cycles a second, finite and above 0 (DOTween's vibrato over the duration). @param duration   Seconds, finite and above 0. @param exponent   The envelope's, finite and above 0: 1 fades linearly, above 1 sooner, below 1 later. @param punchOut   Receives the punch. @return mtwn_success; mtwn_errorInvalid for a null pointer or a parameter outside the above. @par Thread safety Safe from any thread.

```c
double mtwnSamplePunch(const mtwnPunch* punch, double seconds);
```
Samples a punch: a factor from -1 to 1 the host scales by each channel's amplitude and adds to the value; exactly 0 at 0, at the duration and outside them.  @param punch    A punch made by this library. @param seconds  The time since its start. @return The factor. @par Thread safety Safe from any thread.

```c
MTWN_NODISCARD MTWN_API mtwnResult mtwnMakeShake(double frequency, double duration, double exponent, uint64_t seed, mtwnShake* shakeOut);
```
Makes a shake.  @param frequency  Lattice points a second, finite and above 0: how often the noise changes direction. @param duration   Seconds, finite and above 0. @param exponent   The envelope's, finite and above 0. @param seed       Any value; the same seed shakes the same way. @param shakeOut   Receives the shake. @return mtwn_success; mtwn_errorInvalid for a null pointer or a parameter outside the above. @par Thread safety Safe from any thread.

```c
double mtwnSampleShake(const mtwnShake* shake, uint32_t channel, double seconds);
```
Samples a shake for one channel: a factor from -1 to 1, each channel its own noise; exactly 0 at 0, at the duration and outside them.  @param shake    A shake made by this library. @param channel  Which channel, any value. @param seconds  The time since its start. @return The factor. @par Thread safety Safe from any thread.

## `path.h`

Paths and tracks (records mtwn-0016, mtwn-0031 and mtwn-0032): keyframe curves of one to four channels, segment by segment linear, step, cubic Bezier or Catmull-Rom (centripetal by default), timed by their keyframes or traveled at constant speed by arc length; and tracks, which give every value kind its keyframes. Both reference the caller's arrays and allocate nothing.

```c
MTWN_NODISCARD MTWN_API mtwnResult mtwnMakePath(const mtwnKeyframe* keyframes, uint32_t count, uint8_t channels, double alpha, double* lengthsOut, mtwnPath* pathOut);
```
Makes a path. With lengths it travels at constant speed: the cumulative arc length at each keyframe is measured into them, and a progress is a share of the whole length, the keyframes' offsets and easings no longer timing it.  @param keyframes  At least 2: offsets finite, not decreasing, the first 0 and the last 1; values finite in the path's channels, and Bezier controls too; known kinds. @param count      The number of keyframes. @param channels   From 1 to 4. @param alpha      Catmull-Rom's knot exponent from 0 to 1: 0.5 centripetal (no cusps or self-intersections within a segment), 0 uniform, 1 chordal. @param lengthsOut Count doubles for constant speed, or NULL. Every segment but the last keyframe's then may not be a step, which has no length to travel. @param pathOut    Receives the path. @return mtwn_success; mtwn_errorInvalid for a null pointer or an argument outside the above. @par Thread safety Safe from any thread; the lengths are used by one thread at a time.

```c
void mtwnSamplePath(const mtwnPath* path, double progress, double valueOut[4], double tangentOut[4]);
```
Samples a path: the segment a progress falls in (the last keyframe at or below it; past the ends the first or last segment extended), its local progress eased by the segment's easing, then its kind; or at constant speed the point at that share of the length.  @param path        A path made by this library. @param progress    The progress, finite. @param valueOut    Receives the path's channels, 0 past them. @param tangentOut  Receives the segment's derivative by its own parameter there, not normalized, for orienting along the path; may be NULL. @par Thread safety Safe from any thread.

```c
static inline mtwnTrackDef mtwnDefaultTrackDef(mtwnValueKind kind);
```
Makes a track def for a kind: centripetal Catmull-Rom, colors in Oklab with the shorter hue, angles the shorter way, rotations the shortest.  @param kind  The kind. @return The def. @par Thread safety Safe from any thread.

```c
MTWN_NODISCARD MTWN_API mtwnResult mtwnMakeTrack(const mtwnTrackDef* def, mtwnKeyframe* keyframes, uint32_t count, double* lengthsOut, mtwnSegmentMix* mixesOut, mtwnTrack* trackOut);
```
Makes a track. Numbers and vectors are their path; integers the path rounded to nearest, ties away from zero; discrete values step at an eased 1/2 of a linear segment, or hold through a step. Angles are unwrapped in place, each keyframe (with its controls) moved by the method's whole turns from the one before. Colors mix linear segments in the def's space and hold through steps. Rotations slerp linear segments, hold through steps, and take Bezier and Catmull-Rom per component, normalized after; with the shortest path the keyframes and controls are first flipped in place into the hemisphere of the one before. On failure the keyframes are as they were.  @param def         The def: a known kind, alpha from 0 to 1, known choices for its kind. @param keyframes   As mtwnMakePath takes them, in the kind's channels; a rotation not all zero; a color's alpha from 0 to 1. Colors and discrete values take linear and step segments only. Rewritten in place for angles and rotations on the shortest path (unwrapped, flipped), so one array must not feed two tracks with different angle methods or rotation paths; a clip copies them first. @param count       The number of keyframes. @param lengthsOut  Count doubles for constant speed, or NULL; only numbers, vectors, angles and integers take it. @param mixesOut    Count - 1 mixes for colors and rotations, else NULL or unused. @param trackOut    Receives the track. @return mtwn_success; mtwn_errorInvalid for a null pointer or an argument outside the above. @par Thread safety Safe from any thread; the keyframes are used by one thread at a time.

```c
mtwnVec4 mtwnSampleTrack(const mtwnTrack* track, double progress);
```
Samples a track: its value at a progress, as mtwnSamplePath finds the segment and eases it.  @param track     A track made by this library. @param progress  The progress, finite. @return The value, the kind's channels used and the rest 0. @par Thread safety Safe from any thread.

## `player.h`

Players (records mtwn-0004, mtwn-0012, mtwn-0029 and mtwn-0035): what writes targets. A player plays a finished clip on targets bound to its slots, by Web Animations' procedures on the host's clock; a transition is a player too. A player not kept ends by committing its last value into its targets' base values and is then freed, its id stale.

```c
static inline mtwnBinding mtwnBind(uint32_t slot, mtwnTargetId target);
```
Makes a binding of a slot to a target: replace, at full weight.  @param slot    The clip's slot. @param target  The target. @return The binding. @par Thread safety Safe from any thread.

```c
static inline mtwnPlayerDef mtwnDefaultPlayerDef(void);
```
Makes a player def: no clip, no bindings, playing at once at rate 1 and full weight, freed when it ends. Built in the program, so it carries the version of the headers the program includes.  @return The def. @par Thread safety Safe from any thread.

```c
MTWN_NODISCARD MTWN_API mtwnResult mtwnCreatePlayer(mtwnContext* context, const mtwnPlayerDef* def, mtwnPlayerId* playerOut);
```
Makes a player of a finished clip, which it holds busy until it is freed; it starts at the next update if the def says play, else idle.  @param context    The context. @param def        The def. @param playerOut  Receives the player's id, or the null id on failure. @return mtwn_success; mtwn_errorInvalid for a null argument, a def without its cookie, a rate NaN or above MTWN_RATE_LIMIT in size, a weight outside [0, 1], a binding outside the above; mtwn_errorVersion for a def of another version; mtwn_errorStale for a clip that is not alive; mtwn_errorState for one not finished; mtwn_errorLimit when the context holds its capacity of players or of bindings. @par Thread safety Safe from any thread; the context is used by one thread at a time.

```c
MTWN_NODISCARD MTWN_API mtwnResult mtwnPlayer_Play(mtwnContext* context, mtwnPlayerId player);
```
Plays a clip's player, as Web Animations plays an animation with auto-rewind: from the start (or the end, backwards) when idle, at its end or before its start; a paused player resumes. One that needs a start time takes the next update's.  @param context  The context. @param player   A clip's player. @return mtwn_success; mtwn_errorInvalid for a null context; mtwn_errorStale for an id that names no live player; mtwn_errorState for a transition, or backwards from an end that is infinite. @par Thread safety Safe from any thread; the context is used by one thread at a time.

```c
MTWN_NODISCARD MTWN_API mtwnResult mtwnPlayer_Pause(mtwnContext* context, mtwnPlayerId player);
```
Pauses a clip's player at its current time, or at its start (its end, backwards) when idle.  @param context  The context. @param player   A clip's player. @return As mtwnPlayer_Play. @par Thread safety Safe from any thread; the context is used by one thread at a time.

```c
MTWN_NODISCARD MTWN_API mtwnResult mtwnPlayer_Reverse(mtwnContext* context, mtwnPlayerId player);
```
Reverses a clip's player: negates its rate, keeping its current time, and plays it with auto-rewind; refused, and nothing changed, when that would play an infinite clip backwards from its end.  @param context  The context. @param player   A clip's player. @return As mtwnPlayer_Play. @par Thread safety Safe from any thread; the context is used by one thread at a time.

```c
MTWN_NODISCARD MTWN_API mtwnResult mtwnPlayer_Finish(mtwnContext* context, mtwnPlayerId player);
```
Finishes a clip's player: its current time goes to the end of its playback direction, the clip's end forwards, 0 backwards, crossing what lies before it, its records at once.  @param context  The context. @param player   A clip's player. @return As mtwnPlayer_Play; mtwn_errorState also for a rate of 0 or an infinite clip played forwards. @par Thread safety Safe from any thread; the context is used by one thread at a time.

```c
MTWN_NODISCARD MTWN_API mtwnResult mtwnPlayer_Cancel(mtwnContext* context, mtwnPlayerId player);
```
Cancels a clip's player: it becomes idle, without effect, and is kept, to be played again; a cancel record says so if it had a time.  @param context  The context. @param player   A clip's player. @return As mtwnPlayer_Play. @par Thread safety Safe from any thread; the context is used by one thread at a time.

```c
MTWN_NODISCARD MTWN_API mtwnResult mtwnPlayer_Seek(mtwnContext* context, mtwnPlayerId player, double seconds);
```
Seeks a clip's player to a current time, jumping: what lies between is not crossed, and a seek record says so (mtwnPlayer_Sweep crosses it). Seeks by progress and label jump too.  @param context  The context. @param player   A clip's player. @param seconds  The time into the clip, finite. @return As mtwnPlayer_Play; mtwn_errorInvalid also for a time not finite. @par Thread safety Safe from any thread; the context is used by one thread at a time.

```c
MTWN_NODISCARD MTWN_API mtwnResult mtwnPlayer_Sweep(mtwnContext* context, mtwnPlayerId player, double seconds);
```
Seeks a clip's player to a time crossing everything between, as an update sweeping there would: its records come at once, at the last update's time. A seek otherwise jumps, leaving a seek record.  @param context  The context. @param player   A clip's player. @param seconds  The time into the clip, finite. @return As mtwnPlayer_Seek. @par Thread safety Safe from any thread; the context is used by one thread at a time.

```c
MTWN_NODISCARD MTWN_API mtwnResult mtwnPlayer_SeekProgress(mtwnContext* context, mtwnPlayerId player, double progress);
```
Seeks a clip's player to a share of its clip's duration.  @param context   The context. @param player    A clip's player. @param progress  The share, finite; the duration must be finite. @return As mtwnPlayer_Seek; mtwn_errorState also for an infinite duration. @par Thread safety Safe from any thread; the context is used by one thread at a time.

```c
MTWN_NODISCARD MTWN_API mtwnResult mtwnPlayer_SeekLabel(mtwnContext* context, mtwnPlayerId player, uint32_t key);
```
Seeks a clip's player to a label of its clip's root.  @param context  The context. @param player   A clip's player. @param key      The label's key. @return As mtwnPlayer_Play; mtwn_errorInvalid also for no such label. @par Thread safety Safe from any thread; the context is used by one thread at a time.

```c
MTWN_NODISCARD MTWN_API mtwnResult mtwnPlayer_SetRate(mtwnContext* context, mtwnPlayerId player, double rate);
```
Sets a clip's player's rate, keeping its current time: as Web Animations keeps it, by the start time, so to within an ulp of the host time times the rate (see MTWN_RATE_LIMIT). A finished player held at its end keeps its start time too, as Web Animations' steps say: given a lower rate, the next update finds it short of its end and it plays again from there, as Firefox does (Chrome keeps a player that played to its end finished). To stay finished at a new rate, set the rate before the end (record mtwn-0052).  @param context  The context. @param player   A clip's player. @param rate     At most MTWN_RATE_LIMIT in size; negative plays backwards, 0 holds. A larger one or NaN is refused as misuse, the player unchanged. @return As mtwnPlayer_Seek. @par Thread safety Safe from any thread; the context is used by one thread at a time.

```c
MTWN_NODISCARD MTWN_API mtwnResult mtwnPlayer_SetWeight(mtwnContext* context, mtwnPlayerId player, double weight);
```
Sets a clip's player's weight, which scales its bindings' from the next update: a layer faded in or out.  @param context  The context. @param player   A clip's player. @param weight   In [0, 1]. @return As mtwnPlayer_Play; mtwn_errorInvalid also for a weight outside [0, 1]. @par Thread safety Safe from any thread; the context is used by one thread at a time.

```c
MTWN_NODISCARD MTWN_API mtwnResult mtwnPlayer_GetState(mtwnContext* context, mtwnPlayerId player, mtwnPlayerState* stateOut);
```
Reads a clip's player's state at the last update's time.  @param context   The context. @param player    A clip's player. @param stateOut  Receives the state. @return As mtwnPlayer_Play; mtwn_errorInvalid also for a null stateOut. @par Thread safety Safe from any thread; the context is used by one thread at a time.

```c
MTWN_NODISCARD MTWN_API mtwnResult mtwnDestroyPlayer(mtwnContext* context, mtwnPlayerId player);
```
Stops a player without committing: its targets show their base values again at once.  @param context  The context. @param player   The player. @return mtwn_success; mtwn_errorInvalid for a null context; mtwn_errorStale for an id that names no live player, as one that ended does. @par Thread safety Safe from any thread; the context is used by one thread at a time.

```c
bool mtwnPlayer_IsValid(const mtwnContext* context, mtwnPlayerId player);
```
Tells whether an id names a live player: one that has not ended, been replaced or been destroyed. Not counted as misuse.  @param context  The context, or NULL. @param player   The id. @return Whether the player is alive. @par Thread safety Safe from any thread; the context is used by one thread at a time.

## `sample.h`

Sampling (record mtwn-0022): an easing turned into points in caller memory, as an even table for a GPU or a host lookup, or as the points of a CSS linear() fitted to a tolerance. Nothing is allocated.

```c
MTWN_NODISCARD MTWN_API mtwnResult mtwnSampleEasing(const mtwnEasing* easing, uint32_t count, float* valuesOut);
```
Samples an easing evenly over progress 0 to 1: valuesOut[i] is the eased progress at i / (count - 1), rounded to float.  @param easing     An easing made by this library. @param count      The number of samples, at least 2. @param valuesOut  Receives count values. @return mtwn_success; mtwn_errorInvalid for a null pointer or a count below 2. @par Thread safety Safe from any thread.

```c
MTWN_NODISCARD MTWN_API mtwnResult mtwnFitLinearEasing(const mtwnEasing* easing, double tolerance, mtwnLinearPoint* pointsOut, uint32_t capacity, uint32_t* countOut);
```
Fits CSS linear() points to an easing over progress 0 to 1: their interpolation is within the tolerance of the eased progress at every probe, 32 inside each segment, and is not proven so between them; the points lie on the curve, the first at 0 and the last at 1. A segment narrower than 2^-24 is kept as it is, so a host function's jump costs points and may stay over the tolerance within 2^-24 of it. Steps are refused: CSS writes them as steps().  @param easing     An easing made by this library, not steps. @param tolerance  The largest error allowed, finite and above 0. @param pointsOut  Receives the points, with their inputs; may be NULL when capacity is 0. @param capacity   The number of points pointsOut holds. @param countOut   Receives the number of points the fit has, also when they do not fit. @return mtwn_success; mtwn_errorCapacity when more than capacity points are needed, the contents of pointsOut then unspecified; mtwn_errorInvalid for a null easing or countOut, a NULL pointsOut with a capacity, steps or a tolerance outside the above. @par Thread safety Safe from any thread.

## `spring.h`

Springs, decay and the rubber band (records mtwn-0010 and mtwn-0023): a damped spring pulling a value to its target, and a velocity decaying to rest, both solved in closed form from where they start, so the same times give the same values at every frame rate; and iOS's rubber band for dragging past a limit. A spring's start is an offset from the target and a velocity, in the value's units (per second); a retarget keeps the velocity and changes the offset.

```c
MTWN_NODISCARD MTWN_API mtwnResult mtwnMakeSpring(double stiffness, double damping, double mass, mtwnSpring* springOut);
```
Makes a spring from its physical parameters: natural frequency sqrt(stiffness / mass), damping ratio damping / (2 sqrt(stiffness mass)).  @param stiffness  Finite and above 0. @param damping    Finite and at least 0. @param mass       Finite and above 0. @param springOut  Receives the spring. @return mtwn_success; mtwn_errorInvalid for a null pointer or a parameter outside the above or giving a frequency outside the finite range above 0. @par Thread safety Safe from any thread.

```c
MTWN_NODISCARD MTWN_API mtwnResult mtwnMakeSpringFromFrequency(double hertz, double dampingRatio, mtwnSpring* springOut);
```
Makes a spring from its natural frequency in hertz and its damping ratio: 1 is critical, below it overshoots, 0 never settles.  @param hertz         Finite and above 0. @param dampingRatio  Finite and at least 0. @param springOut     Receives the spring. @return mtwn_success; mtwn_errorInvalid for a null pointer or a parameter outside the above. @par Thread safety Safe from any thread.

```c
MTWN_NODISCARD MTWN_API mtwnResult mtwnMakeSpringFromDuration(double duration, double bounce, mtwnSpring* springOut);
```
Makes a spring from Apple's perceptual duration and bounce, as SwiftUI, Motion and anime.js map them: natural frequency 2 pi / duration, damping ratio 1 - bounce for a bounce from 0, 1 / (1 + bounce) below it.  @param duration   Seconds, finite and above 0. @param bounce     Above -1 and at most 1: 0 is critical, 1 never settles, below 0 is overdamped. @param springOut  Receives the spring. @return mtwn_success; mtwn_errorInvalid for a null pointer or a parameter outside the above. @par Thread safety Safe from any thread.

```c
mtwnSpringMotion mtwnSpringMotionAt(const mtwnSpring* spring, double seconds);
```
Computes how a spring moves any start over a time, for applying to every channel of a value.  @param spring   A spring made by this library. @param seconds  The time since the start, at least 0; infinity gives the rest, all four zero when the spring is damped. @return The motion. @par Thread safety Safe from any thread.

```c
void mtwnSampleSpring(const mtwnSpring* spring, double offset, double velocity, double seconds, double* offsetOut, double* velocityOut);
```
Samples a spring from a start.  @param spring       A spring made by this library. @param offset       The start's offset from the target. @param velocity     The start's velocity, per second. @param seconds      The time since the start, at least 0. @param offsetOut    Receives the offset then; may be NULL. @param velocityOut  Receives the velocity then; may be NULL. @par Thread safety Safe from any thread.

```c
double mtwnSpringSettlingTime(const mtwnSpring* spring, double offset, double velocity, double offsetTolerance, double velocityTolerance);
```
Returns the time after which a spring from a start stays within both tolerances of its target and of rest, by bounds on its offset and velocity that never end it early; the host then snaps it to the target.  @param spring             A spring made by this library. @param offset             The start's offset from the target. @param velocity           The start's velocity, per second. @param offsetTolerance    How far from the target is at rest, above 0. @param velocityTolerance  How fast is at rest, per second, above 0. @return Seconds from the start: 0 when it starts within both; infinity for an undamped spring that does not, or a tolerance not above 0. @par Thread safety Safe from any thread.

```c
MTWN_NODISCARD MTWN_API mtwnResult mtwnMakeSpringEasing(const mtwnSpring* spring, double duration, double velocity, mtwnEaseMode mode, mtwnEasing* easingOut);
```
Makes an easing of a spring over a duration: the spring from an offset of -1 with a start velocity, sampled at progress times the duration and divided by its value at the duration, so it is exactly 0 at 0 and 1 at 1; a spring settled by then is changed by the distance it had left. For CSS linear() (mtwnFitLinearEasing) and for tracks timed by duration.  @param spring     A spring made by this library. @param duration   Seconds, finite and above 0. @param velocity   The start velocity in units of the whole change a second, finite: 0 from rest. @param mode       The mode. @param easingOut  Receives the easing. @return mtwn_success; mtwn_errorInvalid for a null pointer, an unknown mode, a duration or velocity outside the above, or a spring back at its start at the duration. @par Thread safety Safe from any thread.

```c
MTWN_NODISCARD MTWN_API mtwnResult mtwnMakeDecay(double rate, mtwnDecay* decayOut);
```
Makes a decay whose velocity falls as e^(-rate t).  @param rate      The rate a second, finite and above 0. @param decayOut  Receives the decay. @return mtwn_success; mtwn_errorInvalid for a null pointer or a rate outside the above. @par Thread safety Safe from any thread.

```c
MTWN_NODISCARD MTWN_API mtwnResult mtwnMakeDecayFromDeceleration(double perMillisecond, mtwnDecay* decayOut);
```
Makes a decay from iOS's deceleration rate, the share of velocity kept each millisecond (0.998 normal, 0.99 fast): the rate a second is -1000 ln of it.  @param perMillisecond  Above 0 and below 1. @param decayOut        Receives the decay. @return mtwn_success; mtwn_errorInvalid for a null pointer or a share outside the above. @par Thread safety Safe from any thread.

```c
void mtwnSampleDecay(const mtwnDecay* decay, double position, double velocity, double seconds, double* positionOut, double* velocityOut);
```
Samples a decay from a start: the position is position + velocity (1 - e^(-k t)) / k, the velocity velocity e^(-k t).  @param decay        A decay made by this library. @param position     The start's position. @param velocity     The start's velocity, per second. @param seconds      The time since the start, at least 0; infinity gives the rest. @param positionOut  Receives the position then; may be NULL. @param velocityOut  Receives the velocity then; may be NULL. @par Thread safety Safe from any thread.

```c
double mtwnDecayStopTime(const mtwnDecay* decay, double velocity, double stopSpeed);
```
Returns the time a decay's speed falls to a stop speed.  @param decay      A decay made by this library. @param velocity   The start's velocity, per second. @param stopSpeed  The speed counted as stopped, per second. @return Seconds from the start: 0 when the start is no faster; infinity for a stop speed not above 0. @par Thread safety Safe from any thread.

```c
double mtwnDecayCrossingTime(const mtwnDecay* decay, double position, double velocity, double target);
```
Returns the time a decay reaches a position, where a host hands it over to a spring back from a limit.  @param decay     A decay made by this library. @param position  The start's position. @param velocity  The start's velocity, per second. @param target    The position to reach. @return Seconds from the start: 0 when it starts there; infinity when it never gets there, moving away or coming to rest short of it or exactly on it. @par Thread safety Safe from any thread.

```c
double mtwnRubberBand(double distance, double dimension, double coefficient);
```
iOS's rubber band: how far content moves when dragged a distance past a limit, in a dimension (the view's size along the drag): c x d / (c x + d), with slope c at the limit, never reaching d. Signed: a negative distance moves the other way.  @param distance     The drag past the limit; may be infinite. @param dimension    The view's size along it. @param coefficient  c, 0.55 on iOS. @return The distance moved; 0 for a dimension or coefficient not above 0 or NaN. @par Thread safety Safe from any thread.

```c
double mtwnRubberBandInverse(double offset, double dimension, double coefficient);
```
The inverse of mtwnRubberBand: the drag that moves content an offset past a limit, d o / (c (d - o)).  @param offset       The distance moved, signed. @param dimension    The view's size along it. @param coefficient  c, 0.55 on iOS. @return The drag; infinite, signed, for an offset of at least the dimension; 0 for a dimension or coefficient not above 0 or NaN. @par Thread safety Safe from any thread.

## `target.h`

Targets (records mtwn-0004 and mtwn-0028): one animated value of a fixed kind, holding the host's base value, the current value animations compose over it and its velocity. Animations write targets; the host reads them by id.

```c
MTWN_NODISCARD MTWN_API mtwnResult mtwnCreateTarget(mtwnContext* context, mtwnValueKind kind, mtwnVec4 value, mtwnTargetId* targetOut);
```
Makes a target holding a value as its base and current value, at rest. Channels past the kind's are stored as 0.  @param context   The context. @param kind      The kind. @param value     The value: finite in the kind's channels, and for a rotation not all zero. Stored as given. @param targetOut Receives the target's id, or the null id on failure. @return mtwn_success; mtwn_errorInvalid for a null argument, an unknown kind or a value outside the above; mtwn_errorLimit when the context holds its capacity of targets. @par Thread safety Safe from any thread; the context is used by one thread at a time.

```c
MTWN_NODISCARD MTWN_API mtwnResult mtwnDestroyTarget(mtwnContext* context, mtwnTargetId target);
```
Destroys a target; its id and every copy of it become stale.  @param context  The context. @param target   The target. @return mtwn_success; mtwn_errorInvalid for a null context; mtwn_errorStale for an id that names no live target. @par Thread safety Safe from any thread; the context is used by one thread at a time.

```c
bool mtwnTarget_IsValid(const mtwnContext* context, mtwnTargetId target);
```
Tells whether an id names a live target. Not counted as misuse.  @param context  The context, or NULL. @param target   The id. @return Whether the target is alive. @par Thread safety Safe from any thread; the context is used by one thread at a time.

```c
MTWN_NODISCARD MTWN_API mtwnResult mtwnTarget_SetBase(mtwnContext* context, mtwnTargetId target, mtwnVec4 value);
```
Sets a target's base value, the value animations compose over. A target no animation writes shows it at once; the next update reports the target as changed.  @param context  The context. @param target   The target. @param value    The value, as for mtwnCreateTarget. @return mtwn_success; mtwn_errorInvalid for a null context or a value its kind refuses; mtwn_errorStale for an id that names no live target. @par Thread safety Safe from any thread; the context is used by one thread at a time.

```c
MTWN_NODISCARD MTWN_API mtwnResult mtwnTarget_GetValue(mtwnContext* context, mtwnTargetId target, mtwnVec4* valueOut);
```
Reads a target's current value.  @param context   The context. @param target    The target. @param valueOut  Receives the value, zero on failure. @return mtwn_success; mtwn_errorInvalid for a null argument; mtwn_errorStale for an id that names no live target. @par Thread safety Safe from any thread; the context is used by one thread at a time.

```c
MTWN_NODISCARD MTWN_API mtwnResult mtwnTarget_GetState(mtwnContext* context, mtwnTargetId target, mtwnTargetState* stateOut);
```
Reads all of a target: kind, base value, current value and velocity.  @param context   The context. @param target    The target. @param stateOut  Receives the state, zero on failure. @return As mtwnTarget_GetValue. @par Thread safety Safe from any thread; the context is used by one thread at a time.

```c
MTWN_NODISCARD MTWN_API mtwnResult mtwnReadTargets(mtwnContext* context, const mtwnTargetId* targets, uint32_t count, mtwnVec4* valuesOut);
```
Reads many targets' current values, in the order of their ids.  @param context    The context. @param targets    The ids; NULL only with a count of 0. @param count      How many. @param valuesOut  Receives count values, zero for an id that names no live target. @return mtwn_success; mtwn_errorInvalid for a null argument; mtwn_errorStale when any id names no live target, each such id counted as misuse, the others read. @par Thread safety Safe from any thread; the context is used by one thread at a time.

## `timing.h`

Timing (records mtwn-0012 and mtwn-0027): Web Animations Level 1's calculation of a node's phase, iteration and progress from its local time, with Level 2's effect playback rate, a repeat delay between iterations and a choice of how reverse iterations ease. Staggers (record mtwn-0048): offsets for items in a grid by their distance from an origin.

```c
mtwnTiming mtwnDefaultTiming(void);
```
Returns Web Animations' default timing: no delays, fill none, one iteration from 0, a duration of 0, normal direction, linear easing, no repeat delay, time running back in reverse iterations, rate 1.  @return The timing. @par Thread safety Safe from any thread.

```c
MTWN_NODISCARD MTWN_API mtwnResult mtwnValidateTiming(const mtwnTiming* timing);
```
Checks a timing: delays finite, iteration start finite and from 0, iterations and duration from 0 (infinity allowed, NaN not), repeat delay finite and from 0, playback rate finite and not 0 (negative only with a finite active duration, whose end a backward play starts from), every enumeration known.  @param timing  The timing. @return mtwn_success; mtwn_errorInvalid for a null pointer or a timing outside the above. @par Thread safety Safe from any thread.

```c
void mtwnComputeTiming(const mtwnTiming* timing, double localTime, bool backwards, mtwnTimingResult* resultOut);
```
Computes a node's timing at a local time, by Web Animations' steps.  @param timing     A timing mtwnValidateTiming accepts. @param localTime  The node's local time, seconds, finite. @param backwards  Whether the time is running backwards (the playing animation's rate is negative), which moves the active interval's boundaries. @param resultOut  Receives the result. @par Thread safety Safe from any thread.

```c
mtwnStagger mtwnDefaultStagger(void);
```
Returns the default stagger: from the first item, 0.1 between neighbours in one row, from 0, linear, both axes.  @return The stagger. @par Thread safety Safe from any thread.

```c
MTWN_NODISCARD MTWN_API mtwnResult mtwnComputeStagger(const mtwnStagger* stagger, uint32_t count, double* offsetsOut);
```
Computes a stagger's offsets (record mtwn-0048). Item i sits in column i mod columns and row i / columns; its distance d from the origin is Euclidean, or along one axis, or to the nearest edge; t = (d - min) / (max - min), 0 when all are equal, and the offset is start + span * ease(t), where span is the spacing for mtwn_staggerAmount and the spacing times (max - min) for mtwn_staggerEach. The offsets are the same bits on every platform.  @param stagger     The stagger; its easing made by this library. @param count       The items; 0 writes nothing. @param offsetsOut  Receives count offsets. @return mtwn_success; mtwn_errorInvalid for a null pointer with a count, an unknown origin, axis or spacing, an index not below a count above 0 for mtwn_staggerIndex, a start or spacing not finite, a point beyond 2^20, or a spread (the spacing, or for mtwn_staggerEach the spacing times the distances' range) not finite, writing nothing. @par Thread safety Safe from any thread.

## `transition.h`

Transitions (records mtwn-0004 and mtwn-0029): the short path, one call that animates a target from its current value to another. A new transition on a target replaces the one running there, starting from the value it had reached.

```c
static inline mtwnTransitionDef mtwnDefaultTransitionDef(void);
```
Makes a transition def: no target, an easing over 0.25 seconds, no delay, CSS's ease; for a spring, a critical one of 0.5 seconds' perceptual duration; for a decay, iOS's normal deceleration, 0.998 kept a millisecond; rest within 1e-3 and 1e-2 a second; the target's velocity; colors in Oklab with the shorter hue, angles the shorter way, rotations the shortest. Built in the program, so it carries the version of the headers the program includes.  @return The def. @par Thread safety Safe from any thread.

```c
MTWN_NODISCARD MTWN_API mtwnResult mtwnStartTransition(mtwnContext* context, const mtwnTransitionDef* def, mtwnPlayerId* playerOut);
```
Starts a transition: from the target's current value, at the next update, to the def's value. It replaces the transition running on the target, whose id goes stale. During the delay the start holds at rest. A spring or decay starts with the start velocity: on numbers, vectors, angles and integers per channel, with the spring's or decay's own velocity; colors and rotations spring their path's progress, the start velocity projected on the end minus the start. An angle's end moves by the method's whole turns when started. At its end, a spring's settling time or a decay's stop, the end value (a decay's: where it stopped) becomes the target's base value, its velocity 0, and the transition is freed; an undamped spring runs until replaced or stopped.  @param context    The context. @param def        The def. @param playerOut  Receives the transition's player id, or the null id on failure. @return mtwn_success; mtwn_errorInvalid for a null argument, a def without its cookie, an end value the target's kind refuses, a delay, duration, given velocity or rest tolerance outside the above, a spring or decay not made by this library, an unknown spec, velocity source, color space, hue method or rotation path, a color with alpha outside 0 to 1, the longest way between equal rotations, a spring or decay for a discrete value or a decay for a color or rotation; mtwn_errorVersion for a def built against another major or minor version; mtwn_errorStale for a target that is not alive; mtwn_errorLimit when the context holds its capacity of players. @par Thread safety Safe from any thread; the context is used by one thread at a time.

---

108 functions across 13 headers.
