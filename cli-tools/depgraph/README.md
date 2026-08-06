# depgraph

Read a source tree's `import` statements and report the dependency graph: what depends on
what, which modules nothing imports, and whether there is a cycle.

```
wyn run src/main.wyn                    # the tree you are standing in
wyn run src/main.wyn samples            # a cyclic example
wyn run src/main.wyn samples_acyclic    # a clean layered one
wyn run tests/test_depgraph.wyn         # unit tests
```

Exit status is `0` for an acyclic graph and `1` when a cycle is found, so it works as a
build-order check in a script.

## Why this app exists

A dependency graph is the smallest realistic problem that needs a **graph** rather than a
list, so it reaches parts of the language that flat text-processing apps never touch: an
adjacency structure, a walk that must not loop forever, and a search that has to report
"no answer" distinctly from "zero answers".

## What it shows off

- **A three-colour DFS.** Two colours (seen / not seen) only tell you whether a node was
  *visited*, which is not the question — a diamond (`a→b`, `a→c`, `b→d`, `c→d`) revisits
  `d` and is perfectly acyclic. The cycle is reaching a node still **on the stack**, which
  needs a third state. `enum Mark { Unvisited, InProgress, Done }` names it.
- **A missing root is itself evidence.** If nothing is a root and the graph is non-empty,
  every module is imported by another — impossible for a DAG. The output says so instead
  of computing it silently.
- **Whole-entry matching.** `has_dep` splits on the separator rather than substring-testing,
  because `parser` must not match `parser_util`. The wrong version silently drops edges,
  which is worse than a crash: the graph still looks plausible.
- **The selective import form puts the module LAST** (`import { a, b } from utils`).
  Reading the second token gives you the first imported *symbol*, and every edge then
  points at a node that does not exist. `tests/test_depgraph.wyn` pins this, and a
  mutation to the naive version fails it.

## Sample output

```
depgraph: 6 modules under 'samples'

  report  (no imports)
  util -> lexer
  orphan  (no imports)
  lexer -> util
  app -> parser,report
  parser -> lexer,util

roots: orphan, app

CYCLE: reachable from 'util' - it imports its way back to itself
  (a cycle means no build order exists; break one edge to fix it)
```

## Compiler bugs this app found

Written as a dogfooding exercise; every one of these was fixed in the compiler with a
regression test rather than worked around here.

| Bug | Symptom |
|---|---|
| Import scanners were not interpolation-aware | This app prints import lines through `${...}`, and a nested string literal's `import utils` was read as a **real import** — build failed with `Module 'utils' not found` for a module appearing nowhere in the program |
| `for x in <string>` passed `wyn check` then ICE'd | `File::walk_dir` returns **one newline-joined string**, not an array (unlike `File::list_dir`), so the natural loop was an "internal codegen error" naming no cause. Now a check-time error naming the fix |
| Array string elements were borrowed, not owned | `names.push(parts[0])` after `split()` stored a pointer the source array then freed — the graph came back as garbage or as loop **indices**, with exit status 0. ASan: heap-use-after-free |
| Reassigning a parameter alias over-released | `var base = path` then `base = parts[...]` released the **caller's** string. The damage landed a scope away: an accumulator in a sibling loop came back empty |

Three of the four were **silently wrong answers with exit status 0**, not crashes — which
is exactly the class of bug that only writing a real program surfaces.
