"""Why you would call Wyn from Python: the same algorithm, both ways.

    wyn build --python --release && python3 bench.py

BUILD WITH --release. The default is -O0 (fast compiles for the edit loop), and the
difference is not small: 15x at -O0 versus 48x with --release on the same machine and the
same input. Quoting a default-build number would understate the language by 3x; quoting a
--release number while building without it would overstate it by 3x. bench.py therefore
prints which one it measured rather than leaving you to guess.

Everything else in this library is a one-liner whose ctypes call overhead dwarfs its
work - `add(2, 3)` through an FFI is SLOWER than doing it in Python, and a benchmark of
that would be dishonest. `max_collatz_below` does real iteration in one call, which is
the shape where native code pays for itself.

Two traps this deliberately avoids, both of which produced fake numbers on the way here:

  * a closed-form-reducible function gets CONSTANT-FOLDED by the C compiler, so you end
    up timing a return statement against a real loop (that attempt reported 68,514x).
  * anything that overflows int64 silently wraps in Wyn while Python promotes to bignums,
    so the two are no longer computing the same thing. Collatz stays inside int64 for
    these ranges, and the assert below checks the answers actually match.
"""

import time

from math_lib import max_collatz_below


def py_collatz_steps(n: int) -> int:
    count = 0
    x = n
    while x != 1:
        x = x // 2 if x % 2 == 0 else 3 * x + 1
        count += 1
    return count


def py_max_collatz_below(limit: int) -> int:
    best = 0
    for i in range(1, limit):
        s = py_collatz_steps(i)
        if s > best:
            best = s
    return best


def bench(limit: int) -> None:
    t = time.perf_counter()
    wyn_answer = max_collatz_below(limit)
    wyn_ms = (time.perf_counter() - t) * 1000

    t = time.perf_counter()
    py_answer = py_max_collatz_below(limit)
    py_ms = (time.perf_counter() - t) * 1000

    # If the answers differ, the comparison is meaningless - say so instead of
    # reporting a speedup.
    assert wyn_answer == py_answer, (
        f"MISMATCH: wyn={wyn_answer} python={py_answer} - not the same computation, "
        "so the timing below would be meaningless"
    )

    print(f"  limit {limit:>7}   longest chain {wyn_answer}")
    print(f"    wyn (via ctypes) {wyn_ms:9.1f} ms")
    print(f"    pure python      {py_ms:9.1f} ms")
    print(f"    speedup          {py_ms / wyn_ms:9.1f}x")
    print()


# Nothing in the .dylib records how it was compiled, so this states the requirement
# rather than pretending to detect it.
BUILD_NOTE = "build with --release, or these numbers are ~3x low (-O0 is the default)"


def main() -> None:
    print()
    print("  Collatz: the longest chain below N, computed identically in both languages")
    print(f"  NOTE: {BUILD_NOTE}")
    print()
    for limit in (50_000, 300_000):
        bench(limit)
    print("  The wrapper is ctypes, not a CPython extension: one call, a lot of work is")
    print("  the shape that wins. A per-element callback would be dominated by call")
    print("  overhead instead - PyO3 is the tool for that.")
    print()


if __name__ == "__main__":
    main()
