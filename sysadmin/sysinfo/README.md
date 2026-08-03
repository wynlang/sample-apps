# sysinfo

One non-interactive system report — host, CPU, memory, disks, processes, network —
that runs on macOS **and** Linux and tells you when it cannot measure something.

```bash
wyn run src/main.wyn
```

This app replaces four that overlapped almost completely: the old `sysinfo`,
`procwatch`, `netstat-lite`, and the read-only half of `sysmon`. All four had the same
shape and the same two problems.

## Problem 1: they were macOS-only, silently

Each pasted a macOS shell pipeline into `System.exec` — `sysctl -n hw.pagesize`,
`vm_stat`, `ifconfig`, `df -h` — and printed whatever came back. On Linux that is a
page of blank fields.

Nothing here branches on the platform. Each fact declares the sources it *could* come
from, and `source()` runs the first one that actually exists:

```wyn
var cores = source(["sysctl -n hw.ncpu", "nproc", "getconf _NPROCESSORS_ONLN"])
```

A new platform means a new entry in that list, not a new `if`. Where two platforms
disagree in *format* rather than in command, one parser reads both — `first_ipv4`
finds the word `inet` and takes the next word, which is true of macOS `ifconfig` and
of Linux `ip -o -4 addr` despite their looking nothing alike.

## Problem 2: a failed measurement looked like a measurement

The old apps printed `0` or an empty string when a command was missing, so you could
not tell an idle machine from a broken probe. Here every value either states a fact or
states that it could not get one, and why:

```
● Memory
    Used        [████████████░░░░░░░░░░░░] 62%  12924 / 20526 MB

● Processes
    Top CPU     unavailable -- ps returned no process rows (restricted PID namespace?)
```

There is deliberately no third option that prints a blank. You can see the whole
degradation path at once by taking every command away:

```bash
env PATH=/nonexistent ./src/main    # every row reads "unavailable -- <reason>"
```

## Tests

```bash
wyn test          # 34 tests passed
```

The parsing all lives in `src/facts.wyn`, apart from the `System.exec` calls that feed
it, and `tests/test_facts.wyn` exercises it with **captured output from a real macOS
box and a real Debian container**. So the suite needs neither platform to run, and it
fails when a parser regresses rather than when the machine changes.

That split earned its keep — the tests caught four real bugs while this app was being
written, including a `df` header row being read as a filesystem and a process with an
unparseable `%CPU` vanishing from the ranking.

## Things it demonstrates

- **Capability detection over platform detection**, with an honest fallback chain.
- **Sentinels instead of panics.** `to_int()` and `to_float()` *panic* on bad input, so
  `facts.num` and `facts.centi` are the only gates to them; everything else returns
  `-1`/`""` for "unknown". `ps` really does print `-` in `%CPU` for kernel threads.
- **Sorting in Wyn, not in the shell.** macOS `ps -r` sorts by CPU; Linux `ps -r`
  filters to *running only* and matches nothing on an idle box. `facts.ps_rank` ranks
  the rows itself, identically everywhere.
- **`df -Pk`, not `df -h`.** The POSIX form has fixed columns and 1024-byte blocks on
  both platforms; pseudo-filesystems are classified out by `is_real_fs` rather than by
  a `grep -v devfs | grep -v map` chain.
- **Page size read, never assumed.** This Mac has 16384-byte pages; assuming 4096
  understates every memory figure fourfold.
- **`/proc` needs streaming.** Its files report size 0 to `fseek`, so a size-then-read
  (`File.read`) comes back empty even though `cat` prints 8 kB. That one detail was why
  the first Linux run claimed `/proc/meminfo` was absent while it sat right there.

## Build

```bash
wyn build src/main.wyn
```
