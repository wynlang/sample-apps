"""Test the Wyn-compiled math library from Python"""
from math_lib import (
    add, subtract, multiply, divide, modulo,
    power, factorial, fibonacci, calc_gcd, is_prime,
    calc_clamp, calc_abs, greet, repeat_str
)

passed = 0
failed = 0

def check(name, got, expected):
    global passed, failed
    if got == expected:
        print(f"  ✓ {name}")
        passed += 1
    else:
        print(f"  ✗ {name}: expected {expected}, got {got}")
        failed += 1

print("=== Wyn → Python Library Test ===\n")

# Arithmetic
check("add(2, 3)", add(2, 3), 5)
check("add(-10, 10)", add(-10, 10), 0)
check("subtract(10, 3)", subtract(10, 3), 7)
check("multiply(6, 7)", multiply(6, 7), 42)
check("divide(10, 3)", divide(10, 3), 3)
check("divide(10, 0)", divide(10, 0), 0)
check("modulo(10, 3)", modulo(10, 3), 1)
check("power(2, 10)", power(2, 10), 1024)
check("power(5, 0)", power(5, 0), 1)

# Recursive / iterative
check("factorial(0)", factorial(0), 1)
check("factorial(10)", factorial(10), 3628800)
check("factorial(20)", factorial(20), 2432902008176640000)
check("fibonacci(0)", fibonacci(0), 0)
check("fibonacci(1)", fibonacci(1), 1)
check("fibonacci(10)", fibonacci(10), 55)
check("fibonacci(30)", fibonacci(30), 832040)

# Number theory
check("calc_gcd(12, 8)", calc_gcd(12, 8), 4)
check("calc_gcd(100, 75)", calc_gcd(100, 75), 25)
check("is_prime(2)", is_prime(2), True)
check("is_prime(17)", is_prime(17), True)
check("is_prime(4)", is_prime(4), False)
check("is_prime(1)", is_prime(1), False)

# Utility
check("calc_clamp(50, 0, 10)", calc_clamp(50, 0, 10), 10)
check("calc_clamp(-5, 0, 10)", calc_clamp(-5, 0, 10), 0)
check("calc_clamp(5, 0, 10)", calc_clamp(5, 0, 10), 5)
check("calc_abs(-42)", calc_abs(-42), 42)
check("calc_abs(42)", calc_abs(42), 42)

# Strings
check("greet('World')", greet("World"), "Hello from Wyn, World!")
check("greet('Python')", greet("Python"), "Hello from Wyn, Python!")
check("repeat_str('ab', 3)", repeat_str("ab", 3), "ababab")
check("repeat_str('x', 0)", repeat_str("x", 0), "")

print(f"\n=== Results: {passed} passed, {failed} failed ===")
if failed == 0:
    print("✓ All tests passed!")
else:
    exit(1)
