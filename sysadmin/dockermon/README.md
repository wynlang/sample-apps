# dockermon

Inventory a Docker host with every shell-out in flight at once — and print both clocks so
the speedup is measured, not claimed.

## Run

From this directory, no arguments needed:

```bash
wyn run src/main.wyn          # running containers
wyn run src/main.wyn --all    # include stopped/created ones
```

If Docker isn't installed or its daemon is down, it says so in one line and exits — no
crash, no empty table.

## What it shows off

- **`spawn` + `await_all`, twice.** Round 1 fires `docker ps`, `docker images`,
  `docker volume ls` and `docker network ls` together. Round 2 fires one
  `docker inspect` per container together. Nothing is ever waited on with a sleep.
- **The payoff, measured.** It runs each batch *both* ways — a plain sequential loop, then
  concurrently — and prints both wall clocks. On a 4-container host that's 8 process
  spawns, roughly 435 ms one at a time vs 238 ms concurrent.
- **`enum State` + `struct Container`.** A container's state is one value; the coloured
  dot, the text label and the up/down tally all come from `match` on it, so they cannot
  drift apart.
- **`.lines()` / `.split("\t")`** for parsing tabular command output.

## What was wrong before

The old version advertised concurrency and demonstrated its opposite: it `spawn`ed
**one** function and waited for it with `Time.sleep(2000)`. It could never finish faster
than 2 s, would have printed zeroes had Docker been slower than that guess, and the four
`docker` calls inside that single task still ran one after another. It also scanned
`docker ps` output character by character looking for `"\n"`.
