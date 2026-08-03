# loc - concurrent lines-of-code counter

Walks a source tree, classifies every line as code, comment or blank, and prints a
per-language table ranked by code share - counting all the files concurrently.

```bash
./wyn run src/main.wyn            # the tree you are standing in
./wyn run src/main.wyn samples    # any directory
./wyn run src/main.wyn samples/queue.c   # or a single file
```

Real output, run from this directory:

```
  loc -- concurrent lines-of-code counter
  4 files under "samples", counted concurrently

  LANGUAGE     FILES    CODE  COMMENT  BLANK   SHARE OF CODE
  ──────────────────────────────────────────   ────────────────────────
  C                1      23        4      4   █████████··············· 41%
  Python           1      14        2      7   ██████·················· 25%
  Wyn              1       9        1      2   ███····················· 16%
  JavaScript       1       9        3      1   ███····················· 16%
  ──────────────────────────────────────────   ────────────────────────
  TOTAL            4      55       10     14   79 lines, 18 comments per 100 code

  388us wall-clock; the files total 545us of reading and classifying
```

That last line is the point of the program: the summed per-file cost is the work a
sequential loop would have done, and the wall-clock is what `spawn` + `await_all`
actually took.

`samples/` holds four small files (C, Python, JavaScript, Wyn) so the table has something
to rank the first time you run it. The totals above match `wc -l` exactly.

## What it does

- Recursive walk, skipping `.git`, `node_modules`, `build`, `target`, `dist`.
- Language from the file extension; 11 extensions mapped, unknown ones ignored.
- Comments counted for real, including `/* ... */` blocks that span lines. A trailing
  comment on a code line counts as code, as every loc tool does it.
- Ranked table with a bar chart of each language's share of the code.

## Wyn features on show

- `enum LineKind { Code, Comment, Blank }` + `match` - a line has one kind, and every
  tally is an exhaustive match rather than a chain of `if`s that can disagree.
- `match` on the extension to name the language *and* pick its comment token.
- `spawn` + `await_all` with a **struct returned through a future**, so a file hands back
  its language and all three counts together.
- `HashMap` aggregation keyed by language, so adding a language needs no new variable.
- Array pipelines: `.map()`, `.sum()`, `.sort_by()`.
- `File::list_dir` / `File.is_dir` for the walk; `System.args()` for the argument.

## Tests

```bash
./wyn test tests/test_count.wyn      # 7 tests
```

They pin the classifier - blank lines, per-language comment tokens, multi-line blocks,
one-line blocks, trailing comments, trailing newlines - and cross-check `samples/queue.c`
against `wc -l`.

## What was wrong before

The previous version was broken twice over, and its own output proved it. The file list
was a hardcoded array of `../sample-apps/cli-tools/...` paths that resolve from exactly
one working directory, and the counting was wrong on top of that: every file reported
`Code: 0  Blank: 1  Comment: 0`, with a summary of `Files: 5 / Code: 0 / Blank: 5`,
because the three counts were packed into a single int as
`code*1000000 + blank*1000 + comment` and unpacked wrong. It found 5 lines in 5 files.
