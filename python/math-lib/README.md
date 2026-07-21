# Wyn → Python Math Library

A math library written in Wyn, compiled to a shared library, and called from Python.

## Build

```bash
cd sample-apps/python/math-lib
wyn build main.wyn --python
```

This generates:
- `libmain.dylib` (macOS) / `libmain.so` (Linux) / `libmain.dll` (Windows)
- `main.py` - Python wrapper with typed bindings

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
