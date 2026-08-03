# csv-analyzer

Profiles any CSV: infers each column's type from its values, then prints an aligned table,
per-column statistics, and a histogram for every numeric column.

There is no schema and no configuration. `infer()` looks at the values and decides
`Int`, `Float`, or `Text`; `match` then picks the statistics that make sense — min / max /
mean / median for numbers, cardinality and top values for text. Every column is profiled
concurrently (`spawn` per column, joined by `await_all`). Every table width is the widest
cell in that column, applied with `pad_right` / `pad_left` — both UTF-8 aware, so the box
rule and non-ASCII values line up — and the inferred type also picks the alignment:
numbers right, text left.

## Run

```bash
wyn run src/main.wyn                # profiles the bundled data.csv
wyn run src/main.wyn other.csv      # profiles any CSV you point it at
```

`data.csv` in this directory is the default input. It is found whether you run from this
directory, from `src/`, or from the sample-apps root.

## Build

```bash
wyn build src/main.wyn
```

## Output

Real output for the bundled `data.csv` (ANSI colour stripped, table abridged):

```
═══ csv analyzer ═══
  data.csv  10 rows x 6 columns

  NAME           DEPARTMENT   CITY    SALARY  RATING  YEARS
  ─────────────  ───────────  ──────  ──────  ──────  ─────
  Alice Chen     Engineering  Berlin  142000     4.8      6
  Bob Marsh      Engineering  Berlin  118500     4.1      3
  Charlie Diaz   Sales        Lisbon   96000     3.6      9
  ...

  column profile

  name           text   distinct 10 -- every value unique

  department     text   distinct 4, top: Engineering 4x, Sales 2x

  city           text   distinct 3, top: Berlin 4x, Austin 3x

  salary         int    min 72500  max 155000  mean 109900.00  median 108125.00
          72500 ████████████████████ 2
          82812 ██████████ 1
          93125 ██████████ 1
         103437 ████████████████████ 2
         113750 ██████████ 1
         124062 ██████████ 1
         134375 ██████████ 1
         144687 ██████████ 1

  rating         float  min 2.90  max 4.90  mean 4.04  median 4.15
           2.90 ████████████████████ 2
           3.15  0
           ...
```

Point it at a CSV with non-ASCII values and the columns still line up, because `pad_left`
and `pad_right` count characters rather than bytes:

```
  CITY          POP    AREA
  ───────  ────────  ──────
  Tōkyō    37400068  2194.0
  Delhi    32900000  1484.0
  Oslo       709000   454.0
```
