# todo-finder

A code-debt scanner. It walks a source tree, reads every file **concurrently**, and
reports the `TODO` / `FIXME` / `HACK` / `XXX` / `BUG` markers it finds — ranked by
severity, not just listed.

Unlike a grep, it only counts a marker that is a **whole word inside a comment**. The
string literal `"BUG"` in this repo's own source is not debt, and neither is `TODOS`.

```bash
cd cli-tools/todo-finder
wyn run src/main.wyn          # the bundled, debt-laden sample/ tree
wyn run src/main.wyn ../..    # every other app in the repo
wyn run src/main.wyn ~/code   # or any tree you like
```

## What it prints

Real output of `wyn run src/main.wyn`, colour stripped:

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
all derived from one `[Hit]` list, so they cannot disagree.

## Wyn features on show

- `enum Marker` written in severity order; `sev`, `tag` and `colour` are each a single
  expression-bodied `match`, so adding a marker is one enum case and three arms.
- `struct Hit { file, line, mark, sev, text }` as the one record everything reports from.
- `spawn` + `await_all` — every file is scanned off the main thread, and a `struct`
  comes back through each future.
- Recursive `File::list_dir` walk, `.split()` / `.index_of()` / `.substring()` /
  `.repeat()` / `.pad_left()` for the parsing and the chart, `HashMap` tallies, and
  `.filter()` / `.sort_by()` with lambdas for the grouping and ranking.

## Test

```bash
wyn test tests/test_todo_finder.wyn      # 6 tests
```

The tests pin the severity ordering, the comment rule, the whole-word rule, and that
the bundled `sample/` tree really does contain markers — the last one guards the exact
failure that made the previous version of this app report `Total items found: 0`.

## Build

```bash
wyn build src/main.wyn
```
