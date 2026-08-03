# sysmon

An htop-lite terminal monitor: CPU and memory always on screen, with a switchable body
view of processes, disks or listening ports.

```bash
wyn run src/main.wyn
```

| Key | |
|---|---|
| `1` `2` `3` | processes / disks / network |
| `Tab` | cycle views |
| `r` | refresh |
| `q` | quit |

## A TUI with a test suite

The usual reason a terminal UI has no tests is that "it draws, and drawing needs a
terminal". But almost none of sysmon is drawing — it is parsing `ps`, `df` and `vm_stat`
output and deciding which view a keypress selects, and both of those are pure functions.

So they live in `src/metrics.wyn`, `src/main.wyn` keeps only the `Terminal` calls, and:

```bash
wyn test          # 34 tests passed
```

The fixtures are **captured output from a real macOS box and a real Debian container**,
so the suite needs no terminal, no live system, and passes on either platform.

It paid for itself immediately. Three bugs it caught while this was being written:

- `overlay` was on the "not a real filesystem" list. It is the actual root filesystem of
  every Docker container, so the disks view showed **no `/` at all** on Linux.
- A process whose `%CPU` is unparseable (kernel threads print `-`) dropped out of the
  ranking entirely instead of sorting last.
- Ranking each table position separately re-scanned the whole `ps` output per candidate.
  With ~650 processes one frame took **22 seconds**; ranking in one pass took it to ~1.5s.

## What was wrong with the old sysmon

- **It assumed 4096-byte memory pages** on a Mac whose pages are 16384 — every memory
  figure it printed was a quarter of the truth. The page size now comes from `vm_stat`'s
  own header, and a truncated read reports unknown rather than a plausible smaller total.
- **Its CPU percentage summed the `%CPU` column of every process**, which counts each
  thread separately and drifts far past 100%. It is now the 1-minute load average scaled
  by core count.
- **`1`, `2` and `3` were three independent `if`s** that each assigned *and* redrew, so
  one keypress could redraw twice, and a fourth view meant remembering two places. There
  is now one `next_view(view, key)` — which is also why "an unrecognised key leaves the
  view alone" is a testable property rather than a hope.
- **It was macOS-only** (`sysctl`, `vm_stat`, `lsof`) and printed blank fields elsewhere.

## Cross-platform, and honest when it cannot measure

Nothing branches on the platform. Each metric lists the sources it could come from and
the first one that exists is used — `vm_stat` else `/proc/meminfo`, `ss` else `lsof`,
`uptime` else `/proc/loadavg`.

When none of them exists, the metric says so where its bar would be:

```
 MEM
   Used    unavailable -- no vm_stat and no /proc/meminfo
```

An unmeasurable bar renders as a grey `????` track, never as an empty one — 0% and
"failed to measure" must not look the same. You can see the whole path at once:

```bash
env PATH=/nonexistent ./src/main    # every metric reads "unavailable -- <reason>"
```

Two portability details worth stealing:

- **`ps -r` is a trap.** On macOS it sorts by CPU; on Linux procps it filters to
  *running only*, which on an idle box matches nothing. `metrics.ps_top` sorts in Wyn.
- **`/proc` files report size 0** to `fseek`, so a size-then-read (`File.read`) returns
  empty even though `cat` prints kilobytes. They have to be streamed.

## Build

```bash
wyn build src/main.wyn
```
