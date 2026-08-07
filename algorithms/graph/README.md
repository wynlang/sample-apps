# graph

Graph algorithms in Wyn: weighted shortest paths, breadth-first reachability, and cycle
detection.

## Run

```bash
wyn run
```

## Build

```bash
wyn build
```

## Test

```bash
wyn test tests/test_graph.wyn
```

15 tests. Each targets a *plausible wrong version* of the algorithm rather than the happy
path, and all three were mutation-verified — the mutation is stated next to each test.

## What it demonstrates

- **`Result<Struct, Enum>`** as the return type of every algorithm, with the error variant
  carrying which failure occurred (`Empty`, `BadNode`, `Unreachable`) instead of a
  sentinel. Matching on the result and reading a field off the ok payload in one arm is
  the shape this app leans on.
- **An array of structs where each struct owns its own array** (`[Node]`, each `Node`
  holding `[Edge]`), walked in a nested loop indexed by a value computed in the outer
  loop. This is deliberate: that combination has historically produced silently-wrong
  answers rather than a crash.
- **An explicit stack** for the iterative DFS, kept as two parallel arrays with
  `push`/`pop`, so a deep graph cannot blow the C stack.

No adjacency matrix is used anywhere — a matrix would make the interesting indexing
trivial and is not how a real graph is stored.

## The graph the tests use

```
a --4--> b --1--> d
|                 ^
1                 |
v                 |
c --2--> b -------+
```

`a -> d` costs **5** via `a->b->d` (two hops) but **4** via `a->c->b->d` (three hops), so
the cheapest route is *not* the one with the fewest hops. That is on purpose: a test that
only checked the total cost could not tell a correct path from a different route that
happens to add up the same, so the tests assert the **hop list** as well.

## Notes on two compiler defects hit while writing this

Writing this app surfaced two compiler bugs. **Both have since been fixed**, but the
workarounds stay in the source until the fixes ship in a release, so the app keeps building
on a released `wyn` rather than only on `dev`.

- **`struct Path` broke the file** — FIXED. A user struct named after any of the 42 builtin
  stdlib namespaces (`Path`, `Time`, `Color`, `Log`, `Env`, `Task`, …) made type-checking
  fail with `Type mismatch at line 1:0 / Expected: enum, Got: string` — a message naming
  neither the struct nor a real line. A user struct now shadows the namespace. The struct
  here is still called `Route`; rename it to `Path` once the fix is released.
- **`from` and `root` were reserved words** — FIXED (they are contextual now).
  `fn shortest_path(g: [Node], from: int, to: int)`
  did not parse, and neither did `for root in 0..n`. Both are used *only* in the import
  grammar, so they were contextual by role but reserved globally. `to` was never reserved,
  so a `from`/`to` pair half-worked — which is exactly how this surfaced. The parameters
  here stay `src`/`dst` for the same release reason.
