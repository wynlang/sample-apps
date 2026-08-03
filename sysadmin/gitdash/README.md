# gitdash

A one-screen summary of a git repository: branch, divergence from upstream,
working-tree state, and recent commits.

```bash
wyn run src/main.wyn              # the repository containing the current directory
wyn run src/main.wyn /some/repo   # any other repository
```

## Why this one is about parsing, not portability

The other sysadmin apps spend their effort on capability detection, because `vm_stat` and
`/proc/meminfo` are different worlds. `git` is not: it is the same program everywhere, and
`--porcelain` is a **documented, machine-readable, stable** format — that is what the flag
means, as opposed to `git status`'s human output, which changes with the version and with
the locale.

So the whole problem here is reading that format correctly, and all of it lives in
`src/porcelain.wyn` with a suite against captured git output:

```bash
wyn test          # 20 tests passed
```

Testing it that way is the point. Getting one repository to hold a staged file, a
conflicted file, a rename and an untracked directory *at once* — and keeping it there
while you eyeball the display — is fiddly and unrepeatable. Asserting on the text is not.

## What the old version got wrong

**It could not report a merge conflict.** Its rule was "if the code contains `?` it is
untracked, otherwise column 0 means staged and column 1 means modified". A conflicted file
is `UU`, so it was quietly counted as staged *and* modified, and the word "conflict" never
appeared anywhere — for the single most important thing a git dashboard can tell you.
Conflicts now come first and in red:

```
  Working tree
    conflicts  1 -- resolve these first
    untracked  1

      conflict  f.txt
       untracked extra.txt
```

The seven unmerged code pairs (`DD AU UD UA DU AA UU`) are *listed* from git's
documentation rather than guessed at, and checked before the staged/modified split —
otherwise `AA` looks exactly like "changed in both index and work tree".

**It showed a blank branch on a detached HEAD**, because `git branch --show-current`
prints nothing there. The `## ` porcelain header always names something, so a detached
HEAD reads `HEAD (detached)` and a fresh repo reads `No commits yet on main`.

**It could not tell "no upstream" from "in sync".** With no upstream configured both
counts came back 0 and it printed "Up to date with remote" — about a remote that did not
exist. Divergence now returns `-1` for "nothing to compare against" and `0` for "level".

**It dropped the last status line** whenever git's output had no trailing newline: it
scanned for `"\n"` one character at a time and only handled a line when it found one.

## Details worth stealing

- **One `git -C <dir>` per call.** `cd` inside a `System.exec` only affects the subshell
  and vanishes with it — a mistake the old `filebrowser` actually shipped.
- **`status --porcelain -b` once, not three commands.** Branch, upstream, divergence and
  the file list all come from the same text, so the summary cannot drift from the rows.
- **The status field is fixed-width.** Splitting on whitespace to find it breaks on
  `b file.txt`; a rename is `old -> new`, and the new name is the one that now exists.
- **`classify()` returns one bucket**, so no line is counted twice — the old version
  incremented `staged` and `modified` from independent `if`s.

## Build

```bash
wyn build src/main.wyn
```
