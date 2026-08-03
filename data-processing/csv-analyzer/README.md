# csv-analyzer

Profiles any CSV: infers each column's type from its values, then prints an aligned table,
per-column statistics, and a histogram for every numeric column.

There is no schema and no configuration. `infer()` looks at the values and decides
`Int`, `Float`, or `Text`; `match` then picks the statistics that make sense — min / max /
mean / median for numbers, cardinality and top values for text. Every column is profiled
concurrently (`spawn` per column, joined by `await_all`), and every table width is computed
from the widest cell and applied with `pad_right`.

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

```
═══ csv analyzer ═══
  data.csv  10 rows x 6 columns

  NAME           DEPARTMENT   CITY    SALARY  RATING  YEARS
  ─────────────  ───────────  ──────  ──────  ──────  ─────
  Alice Chen     Engineering  Berlin  142000  4.8     6
  ...

  column profile

  department   text   distinct 4, top: Engineering 4x, Sales 2x

  salary       int    min 72500  max 155000  mean 109900.00  median 108125.00
          72500 ████████████████████ 2
          82812 ██████████ 1
          ...
```
