# counter — a mobile app you can run and test on your laptop

```bash
wyn run  src/main.wyn              # run the logic natively
wyn test tests/test_counter.wyn    # 10 tests, no device, no mocks
wyn cross ios     src/main.wyn     # build the same file for a device
wyn cross android src/main.wyn
```

## The idea

A mobile app is usually untestable off-device because its behaviour lives in callbacks
only the platform ever calls. This one is split in two:

- **A pure core** — `struct Counter` and `apply(counter, Action) -> Counter`. Ordinary
  Wyn, no SDK in sight. Every rule lives here: clamping at a ceiling, clamping at zero,
  tap counting, one-step undo.
- **A thin shell** — the `wyn_*` hooks. They translate taps into `Action`s and state into
  widgets, and contain no decisions.

Because the core is plain functions over plain data, `main()` can run the same code a
device runs, and `tests/test_counter.wyn` can cover all of it with `assert_eq` — no
simulator, no mocking, no `#ifdef`.

The shape that makes this work is a total function from state plus an action to new
state, expressed as a `match` that yields a struct:

```wyn
fn apply(c: Counter, a: Action) -> Counter => match a {
    Action.Inc   => Counter { value: clamp(c.value + 1), taps: c.taps + 1, undone: c.value },
    Action.Dec   => Counter { value: clamp(c.value - 1), taps: c.taps + 1, undone: c.value },
    Action.Reset => Counter { value: 0, taps: c.taps + 1, undone: c.value },
    Action.Undo  => Counter { value: c.undone, taps: c.taps + 1, undone: c.value }
}
```

`match` is exhaustive, so adding a variant to `Action` makes the compiler point at every
place that has to handle it.

## What this replaced

`mobile/counter-app` and `mobile/notes-app`. Both were pure stubs: every SDK function had
an empty body, `main` did nothing but `return wyn_ios_main(0, 0)`, and running either
printed **nothing at all**. They were also near-duplicates of
`tests/mobile/smoke_mobile_shim.wyn` in the compiler repo, which is what CI actually
cross-compiles — so as demos they showed a reader nothing, and as coverage they were
already redundant.

## Bugs this app found in the compiler

Writing it surfaced two real defects, both since fixed:

- An array of payload-free enum values (`[Action.Inc, Action.Dec, ...]`) **collapsed to
  its first element**, silently — so every scripted action read as `Inc`. `test_counter.wyn`
  has a regression test for it (*"an action list keeps its distinct elements"*).
- A `match` that yields a **struct** did not compile: the result temporary was typed
  `long long`, so `apply` above was unwritable.
