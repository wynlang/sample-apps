"""Auto-generated Python wrapper for main.wyn — created by Wyn"""
import ctypes, os, sys

_dir = os.path.dirname(os.path.abspath(__file__))
if sys.platform == 'darwin':
    _ext = 'dylib'
elif sys.platform == 'win32':
    _ext = 'dll'
else:
    _ext = 'so'
_lib = ctypes.CDLL(os.path.join(_dir, f'libmain.{_ext}'))

# add(a: int, b: int) -> int
_lib.add.argtypes = [ctypes.c_longlong, ctypes.c_longlong]
_lib.add.restype = ctypes.c_longlong

def add(a: int, b: int) -> int:
    _r = _lib.add(a, b)
    return _r

# subtract(a: int, b: int) -> int
_lib.subtract.argtypes = [ctypes.c_longlong, ctypes.c_longlong]
_lib.subtract.restype = ctypes.c_longlong

def subtract(a: int, b: int) -> int:
    _r = _lib.subtract(a, b)
    return _r

# multiply(a: int, b: int) -> int
_lib.multiply.argtypes = [ctypes.c_longlong, ctypes.c_longlong]
_lib.multiply.restype = ctypes.c_longlong

def multiply(a: int, b: int) -> int:
    _r = _lib.multiply(a, b)
    return _r

# divide(a: int, b: int) -> int
_lib.divide.argtypes = [ctypes.c_longlong, ctypes.c_longlong]
_lib.divide.restype = ctypes.c_longlong

def divide(a: int, b: int) -> int:
    _r = _lib.divide(a, b)
    return _r

# modulo(a: int, b: int) -> int
_lib.modulo.argtypes = [ctypes.c_longlong, ctypes.c_longlong]
_lib.modulo.restype = ctypes.c_longlong

def modulo(a: int, b: int) -> int:
    _r = _lib.modulo(a, b)
    return _r

# power(base: int, exp: int) -> int
_lib.power.argtypes = [ctypes.c_longlong, ctypes.c_longlong]
_lib.power.restype = ctypes.c_longlong

def power(base: int, exp: int) -> int:
    _r = _lib.power(base, exp)
    return _r

# factorial(n: int) -> int
_lib.factorial.argtypes = [ctypes.c_longlong]
_lib.factorial.restype = ctypes.c_longlong

def factorial(n: int) -> int:
    _r = _lib.factorial(n)
    return _r

# fibonacci(n: int) -> int
_lib.fibonacci.argtypes = [ctypes.c_longlong]
_lib.fibonacci.restype = ctypes.c_longlong

def fibonacci(n: int) -> int:
    _r = _lib.fibonacci(n)
    return _r

# calc_gcd(a: int, b: int) -> int
_lib.calc_gcd.argtypes = [ctypes.c_longlong, ctypes.c_longlong]
_lib.calc_gcd.restype = ctypes.c_longlong

def calc_gcd(a: int, b: int) -> int:
    _r = _lib.calc_gcd(a, b)
    return _r

# is_prime(n: int) -> bool
_lib.is_prime.argtypes = [ctypes.c_longlong]
_lib.is_prime.restype = ctypes.c_bool

def is_prime(n: int) -> bool:
    _r = _lib.is_prime(n)
    return _r

# calc_clamp(val: int, lo: int, hi: int) -> int
_lib.calc_clamp.argtypes = [ctypes.c_longlong, ctypes.c_longlong, ctypes.c_longlong]
_lib.calc_clamp.restype = ctypes.c_longlong

def calc_clamp(val: int, lo: int, hi: int) -> int:
    _r = _lib.calc_clamp(val, lo, hi)
    return _r

# calc_abs(n: int) -> int
_lib.calc_abs.argtypes = [ctypes.c_longlong]
_lib.calc_abs.restype = ctypes.c_longlong

def calc_abs(n: int) -> int:
    _r = _lib.calc_abs(n)
    return _r

# greet(name: string) -> string
_lib.greet.argtypes = [ctypes.c_char_p]
_lib.greet.restype = ctypes.c_char_p

def greet(name: str) -> str:
    name = name.encode() if isinstance(name, str) else name
    _r = _lib.greet(name)
    return _r.decode() if _r else ""

# repeat_str(s: string, n: int) -> string
_lib.repeat_str.argtypes = [ctypes.c_char_p, ctypes.c_longlong]
_lib.repeat_str.restype = ctypes.c_char_p

def repeat_str(s: str, n: int) -> str:
    s = s.encode() if isinstance(s, str) else s
    _r = _lib.repeat_str(s, n)
    return _r.decode() if _r else ""

