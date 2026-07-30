# Golden reference images

Each PNG here is the committed expected output of a rendering test. They catch
regressions unit tests cannot see: tile seams, channel swaps, premultiply
errors, and off-by-one geometry.

## scene.png (64x64)

Built by `build_scene()` in `tests/test_golden.wyn`:

- **Bottom layer** — horizontal ramp, red (1,0,0) at x=0 to green (0,1,0) at x=63, fully opaque
- **Top layer** — solid blue (0.2, 0.3, 0.9), blend mode `multiply`, opacity 0.75
- **Mask** — left half (x < 32) white, right half black

Expected appearance: a red-to-green ramp whose left half is **darkened**, with a
hard vertical edge at x=32.

### The left half is NOT blue-tinted, and that is correct

The plan and the design spec both describe this scene as "blue-tinted" on the
left. It is not, and the compositor is right — `multiply` **preserves zeros**:

    blend(cb, cs) = cb * cs        so    blend(0.0, 0.9) = 0.0

The ramp has **no blue at any pixel**, so multiplying by a blue layer cannot
introduce any. Source-over cannot either, since both the blended colour and the
backdrop blue are 0. The blue channel of `scene.png` is `0x00` everywhere —
verified directly from the committed PNG bytes.

What the top layer actually does is **darken**, plus shift the hue toward olive:
red is attenuated by the blue layer's 0.2 while green is attenuated by only 0.3,
so green survives comparatively better. That is exactly what the image shows.

`tests/test_golden.wyn` asserts the zero-blue property explicitly, in
"the multiply cannot add blue the ramp does not have", so that nobody later
"fixes" the compositor to match the incorrect prose.

### Reference values (hand-derived, alpha 1, linear light)

For the left half, with ramp colour `r` and blue-layer channel `b`:

    out = r*b*0.75 + r*0.25

| x  | R        | G        | B   |
|----|----------|----------|-----|
| 0  | 0.400000 | 0.000000 | 0.0 |
| 20 | 0.273016 | 0.150794 | 0.0 |
| 31 | 0.203175 | 0.233730 | 0.0 |
| 32 | 0.492063 | 0.507937 | 0.0 |  ← first unmasked column
| 63 | 0.000000 | 1.000000 | 0.0 |

The edge at x=32 is a genuine discontinuity: R jumps by ~0.289 there, while
adjacent columns within either half differ by at most ~0.016 — an 18x ratio.
The test asserts that ratio, so a feathered or one-column-off mask fails.

## Regenerating

Only regenerate when the *intended* output changes, never to make a failing
test pass:

    ~/.wyn/bin/wyn test test_golden
    cp /tmp/wyncanvas_scene_actual.png tests/golden/scene.png

Then **look at the new image** before committing it. Tolerance is 1.5/255,
which absorbs 8-bit quantization but not real rendering changes.

Note that `tests/test_golden.wyn` also asserts the scene numerically (edge
position, per-channel values, row-to-row identity, opacity, determinism). Those
checks are independent of the PNG, so a wrong golden committed by mistake still
fails the suite rather than silently becoming the new truth.

## Verifying the golden test can still fail

A golden test that cannot fail is worthless. To re-confirm it detects breakage,
temporarily weaken the multiply kernel in `csrc/wynimg_blend.c`:

    case 1:  r = cb * cs * 0.5; break;     // was: r = cb * cs

then `./csrc/build.sh && ~/.wyn/bin/wyn test test_golden` — it must FAIL.
Revert, rebuild, and confirm it passes again. Last verified 2026-07-29:
the broken kernel produced a max channel difference of ~0.10, about 17x the
0.006 tolerance.
