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
  concurrently — and prints both wall clocks. On an 8-container host that's 12 process
  spawns: **522 ms one at a time vs 220 ms with `spawn` + `await_all`.** Your numbers will
  differ; that's the point of measuring them.
- **`enum State` + `struct Container`.** A container's state is one value; the coloured
  dot, the text label and the up/down tally all come from `match` on it, so they cannot
  drift apart.
- **`.lines()` / `.split("\t")`** for parsing tabular command output — a container status
  legitimately contains spaces (`Exited (0) 5 minutes ago`), so only the tab may separate.

## Tests

```bash
wyn test tests/test_psparse.wyn    # 18 tests, no Docker daemon required
```

The parsing and column arithmetic live in `src/psparse.wyn` and are tested against
captured `docker ps` output, so the suite gives the same answer on a CI runner with no
Docker as on a laptop with twenty containers. Two of those tests pin bugs found by
actually looking at this app's real output:

- `pedantic_lederberg` is exactly 18 characters, and an 18-cell-wide column with no
  reserved gap printed it fused into the next column:
  `pedantic_lederberghashicorp/terraform-mcp-server:0.…`.
- Truncating with `.len()` measures **bytes**. The 9-character rule row `─────────` is 27
  bytes, so it looked too long and got cut one byte into a 3-byte `─`, printing mojibake.
  `short()` now compares and cuts in cells via `cell_start()`.

## What was wrong before

The old version advertised concurrency and demonstrated its opposite: it `spawn`ed
**one** function and waited for it with `Time.sleep(2000)`. It could never finish faster
than 2 s, would have printed zeroes had Docker been slower than that guess, and the four
`docker` calls inside that single task still ran one after another. It also scanned
`docker ps` output character by character looking for `"\n"`.
