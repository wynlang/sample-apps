# envdiff

Diff the live environment against a saved baseline: **added / removed / changed** keys,
with secret-looking values redacted.

An earlier version of this app diffed nothing — it printed a roll-call of 27 hardcoded
variable names marked set/not-set (and an off-by-one `0..26` silently dropped the last
one). This version reads the whole environment at runtime and actually compares it.

## Run

```bash
wyn run src/main.wyn                    # diff against the committed baseline.env
wyn run src/main.wyn other.env          # diff against any snapshot
wyn run src/main.wyn --save mine.env    # write a snapshot of the current environment
```

With no arguments it diffs against `baseline.env`, a committed snapshot of a plausible
Linux production box, so you get a real diff on the first run.

## Self-check

The tool that reads baselines also writes them, so it can verify itself in one session:

```bash
wyn run src/main.wyn --save /tmp/now.env
wyn run src/main.wyn /tmp/now.env
#   ~0 changed  -0 removed  +0 added  =138 identical
#   the environment matches the baseline exactly
```

## Secrets

A key containing `KEY`, `TOKEN`, `SECRET`, `PASSWORD` or `PASSWD` never has its value
printed. envdiff still shows that it changed, by length and by a small fingerprint:

```
API_TOKEN    <redacted 27ch #7756> -> <redacted 30ch #1621>
```

## What it shows off

- `enum Change { Added, Removed, Changed, Same }` + `struct Delta` — a key's fate is one
  value, and every symbol, colour and heading is an exhaustive `match` on it.
- Two `HashMap`s, so the diff is set arithmetic rather than string poking.
- Expression-bodied functions (`fn mark(c: Change) -> string => match c { ... }`).

## Build

```bash
wyn build src/main.wyn
```
