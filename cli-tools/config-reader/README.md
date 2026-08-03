# config-reader

A real config loader: parses a TOML-ish file, infers each value's type, and validates it
against a declared schema — reporting **every** problem at once, with line numbers.

The shipped `app.conf` is deliberately imperfect, so the demo shows failures being caught
rather than a happy path: an out-of-range port, a type mismatch, a malformed line, an
unknown key, and a required key that is absent.

## Run

```bash
wyn run src/main.wyn              # uses app.conf next to it
wyn run src/main.wyn other.conf   # or any file you pass
```

Works from the app directory, from `src/`, or from the sample-apps root. A missing file or
a malformed line is reported, not crashed on.

## Test

```bash
wyn test
```

## What it demonstrates

| Feature | Where |
|---|---|
| `enum Value { Int, Float, Bool, Str }` + `match` | inferred type per value; one `match` names it |
| `struct Entry / Rule / Fault` | parsed entry, schema rule, validation failure |
| Expression-bodied `fn` | `fn type_name(k: Value) -> string => match k { ... }` |
| Typed array params `[Entry]` | `get_int(entries, "database.port")` returns an `int` |
| `sort_by` with a lambda | problems reported in file order |
| String methods | `trim` `split` `substring` `index_of` `pad_right` `to_int` |
| Real nesting | `[database]` + `host` → `database.host`, not a flat invented name |

## Config format

```toml
# comments run to end of line (but not inside quotes)
[server]
host    = "0.0.0.0"    # quoted => always a string
port    = 8080         # bare digits => int
timeout = 2.5          # one dot => float
debug   = false        # true/false => bool
```
