# Wyn → Python Math Library

A math library written in Wyn, compiled to a shared library, and called from Python.

## Build

```bash
cd sample-apps/python/math-lib
wyn build --python
```

This generates:
- `libmath_lib.dylib` (macOS) / `.so` (Linux) / `.dll` (Windows)
- `math_lib.py` - Python wrapper with typed bindings

The module is `math_lib`, not `math-lib`: the project name is sanitized to a valid
identifier, because `from math-lib import add` is a Python syntax error.

Neither is committed. They are build output, and a stale 227KB `libmain.dylib` from an
old compiler was in this repo for weeks - it would have kept working while the source
drifted, which is the worst way for a demo to fail.

## Test

```bash
python3 test.py
```

## Usage

```python
from main import add, factorial, greet, is_prime

print(add(2, 3))           # 5
print(factorial(20))        # 2432902008176640000
print(greet("World"))       # Hello from Wyn, World!
print(is_prime(17))         # True
```

## Functions

| Function | Signature | Description |
|----------|-----------|-------------|
| `add` | `(int, int) → int` | Addition |
| `subtract` | `(int, int) → int` | Subtraction |
| `multiply` | `(int, int) → int` | Multiplication |
| `divide` | `(int, int) → int` | Integer division (0 on div-by-zero) |
| `modulo` | `(int, int) → int` | Modulo (0 on div-by-zero) |
| `power` | `(int, int) → int` | Exponentiation |
| `factorial` | `(int) → int` | Factorial (64-bit) |
| `fibonacci` | `(int) → int` | Fibonacci (iterative) |
| `gcd` | `(int, int) → int` | Greatest common divisor |
| `is_prime` | `(int) → bool` | Primality test |
| `clamp` | `(int, int, int) → int` | Clamp value to range |
| `abs_val` | `(int) → int` | Absolute value |
| `greet` | `(string) → string` | Greeting message |
| `repeat_str` | `(string, int) → string` | Repeat string N times |

## Why bother

`add(2, 3)` through an FFI is *slower* than doing it in Python — the ctypes call overhead
dwarfs the work. The case that pays is one call doing a lot of work:

```bash
wyn build --python --release && python3 bench.py
```

Collatz, longest chain below 300,000, identical answers (442) in both languages:

| | time |
|---|---|
| wyn via ctypes | **43 ms** |
| pure Python | 2090 ms |

**48x** — with `--release`. At the default `-O0` it is 15x, so the flag is worth about 3x
and `bench.py` says which build it measured rather than letting you guess.

Honest limits: the wrapper is ctypes, not a CPython extension, so a per-element callback
would be dominated by call overhead — PyO3 is the right tool for that shape. What Wyn has
here is compile time and zero setup.
