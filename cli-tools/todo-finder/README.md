# todo-finder

A code-debt scanner. It walks a source tree, scans every file **concurrently**, and
reports the `TODO` / `FIXME` / `HACK` / `XXX` / `BUG` markers it finds — ranked by
severity, not just listed.

Unlike a grep, it only counts a marker that is a **whole word after a comment opener**.
A `"BUG"` string literal is not debt, and neither is `TODOS`.

```bash
cd cli-tools/todo-finder
wyn run src/main.wyn             # scans "." — this app, including its sample/ tree
wyn run src/main.wyn sample      # just the bundled, debt-laden fixture
wyn run src/main.wyn ../..       # every other app in the repo (207 files)
```

## What it prints

Real output of `wyn run src/main.wyn sample`, colour stripped:

```
═══ code debt ═══
  10 markers in 2 of 3 files under sample

  BUG   ████████████████████ 2
  FIXME ████████████████████ 2
  HACK  ████████████████████ 2
  XXX   ██████████ 1
  TODO  ██████████████████████████████ 3

  worst files
     5  sample/net/retry.wyn
     5  sample/checkout.wyn

  BUG (2)
    sample/net/retry.wyn:4 // BUG: no jitter, so every client retries in lockstep...
    sample/checkout.wyn:5 // BUG: overflows for carts over 2^31; needs a widening...

  FIXME (2)
    sample/net/retry.wyn:9 // FIXME: 429 needs to honour Retry-After rather than...
    sample/checkout.wyn:12 // FIXME: rate table is hardcoded, should come from config
  ...
```

Three views — the severity chart, the worst-offenders table, and the grouped listing —
are all derived from one `[Hit]` list, so they cannot disagree.

## Wyn features on show

- `enum Marker` written in severity order, so a hit's rank **is** its index in that
  ladder — there is no separate severity table to fall out of sync with the enum, and
  the tag a hit is reported under is the very string it was matched by.
- `struct Hit { file, line, sev, text }` as the one record everything reports from.
- `spawn` + `await_all` — every file is scanned off the main thread, one future each.
- Recursive `File::list_dir` walk; `.split()` / `.index_of()` / `.substring()` /
  `.repeat()` / `.pad_left()` / `.pad_right()` for parsing and the chart; `HashMap`
  tallies; `.filter()` / `.sort_by()` / `.max()` with lambdas for grouping and ranking.

## Test

```bash
wyn test tests/test_todo_finder.wyn      # 6 tests
```

The tests pin the severity ordering, the comment rule, the whole-word rule, and that the
bundled `sample/` tree really does contain markers — the last one guards the exact
failure that made the previous version of this app report `Total items found: 0`.

## Build

```bash
wyn build src/main.wyn
```
