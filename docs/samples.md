# Samples

Small programs in `samples/`, built with the library when it is the
top-level project (`MAUL_TWEEN_BUILD_SAMPLES`). Each checks what it
shows and returns 0 when it holds, so the test suite runs them.

| Program | What it shows |
|---|---|
| `button.c` | A button's states as transitions: its scale on a spring, its lift and fill eased. The pointer enters, leaves before the hover settles, presses and releases; each change starts from the value and velocity the last one left, so no frame jumps. The host redraws only the targets the update reports changed, and a resting button reports none. |
| `sequence.c` | A card's entrance as a clip: a backdrop fades in, a title and a body slide up, staggered by positions after the previous track, with a mark where a sound plays. The host reads the event records each frame; the same clip played backwards hides the card, every record coming once the other way. |
| `layers.c` | Clips layered on one target: an idle clip breathes a scale forever, a hit clip added over it squashes it for 0.3 s and goes when it ends, adding exactly its own value; a wheel spins on by an incremental track, a quarter turn every half second, one and a half turns in three seconds. |
| `grid.c` | A grid of tiles appearing from its centre: each fades in and slides up, delayed by a stagger, so the centre leads and the corners follow. Played again under reduced motion: the slides land at the first update, while the fades, marked essential because a change of opacity is not motion, still play in their order. |
| `minimal/` | A separate project that uses only the installed package, in C17: it checks the linked version and eases a value to its end. CI builds it against a fresh install. |

Build the minimal sample against an install:

```sh
cmake --install build --prefix "$PWD/package"
cmake -S samples/minimal -B build-minimal -DCMAKE_PREFIX_PATH="$PWD/package"
cmake --build build-minimal
./build-minimal/minimal
```
