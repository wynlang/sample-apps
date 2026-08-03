# WYNSTORM

A Quake-style first-person shooter written in Wyn — raycast 3D rendering, wave-based
combat, three enemy types, on a plain 2D drawing API.

```bash
wyn run src/main.wyn                 # play
wyn build src/main.wyn && ./src/main # or ship a 71KB binary
wyn run src/main.wyn --selfcheck     # headless: verify the render maths with no display
wyn test tests/test_engine.wyn       # 32 tests over the engine, no window needed
```

**Controls** — `W`/`S` move, `A`/`D` or arrows turn, `Q`/`E` strafe, `SPACE` fire, `ESC` quit.

## How it renders 3D without a 3D API

There is no OpenGL here and none is needed. The world is a 2D grid; for each of 320 screen
columns the engine casts a ray, finds the wall it hits, and draws one vertical strip whose
height is inversely proportional to the distance. That is the Wolfenstein 3D technique.

The measured cost of 320 `Gui.rect` strips is **8 ms/frame — about 125 FPS** — which is
why a general-purpose 2D API is enough. I benchmarked that before writing any game code,
because if it had been 40 ms the whole approach would have been wrong.

Details that matter for it to actually look right:

- **DDA, not fixed-step sampling.** Stepping by a small delta both misses thin walls and
  wastes work in open space. DDA jumps exactly to the next grid line: exact, and bounded by
  the cells crossed.
- **Perpendicular distance, not euclidean.** Measuring to the hit point gives the classic
  fisheye bulge. Projecting onto the view direction removes it.
- **Distance fog and side shading.** Without them every wall is equally bright and depth
  vanishes. y-side walls are 30% darker, which is what makes corners legible.
- **Per-axis collision.** Resolving both axes together makes you stick on walls when moving
  diagonally; resolving them separately lets you slide, which is what players expect.
- **Depth-tested sprites.** Enemies are billboards, but each re-casts a ray to check a wall
  isn't in the way — otherwise they show through geometry.

## The design worth copying

`src/engine.wyn` contains **no `Gui` call at all**. The raycast, collision, damage model,
and enemy AI are pure functions over plain data. `src/main.wyn` is the shell: input,
drawing, HUD, frame loop.

That split is why there are 32 real tests. A renderer can't be unit-tested — but a raycast
can, and in a shooter the raycast *is* the game:

```
32 tests passed
```

They assert hand-computed values, not recorded output: a ray fired south from (12.5, 22.5)
must hit the border at 0.5 units and report `side == 1`. A test that records whatever the
code currently does cannot detect the code being wrong.

Two examples of tests earning their keep immediately:

- *"the player spawns in open space"* **failed** on the first run — `(12, 20)` held a metal
  wall, so the player would have started inside geometry and seen the world inside-out.
- *"no ray direction hangs or returns a nonsense distance"* sweeps all 64 headings, which
  is what catches the axis-aligned directions where a naive DDA divides by zero.

## Compiler bugs this game found

Building it surfaced four real defects, all fixed in the compiler with regression tests:

| bug | symptom |
|---|---|
| A module's private enum in an exported signature | prototype said `engine_Cell`, definition said `Cell` — didn't compile |
| A `[int]` parameter in a module function | emitted as `long long`, so every array call site failed |
| Import lists capped at 32 names | 33+ gave a misleading "Expected `}`"; also a latent overflow |
| `wyn build` never linked SDL | GUI apps ran under `wyn run` but built binaries died after one frame |

The last one meant **you could develop a game in Wyn but never ship one**. Worth the
detour.

## What's here

- 24×24 arena: central hall, side corridors, slime pits, a gold exit alcove
- Three enemy types with distinct health/damage/speed/score — `Grunt`, `Brute`, `Wraith`
- Line-of-sight AI: enemies don't see or shoot through walls
- Doom-style armour that soaks a third of incoming damage
- Range-falloff shotgun: one-shots a grunt point blank, chips at distance
- Escalating waves, resupply on clear, minimap, HUD, damage flash, recoil
