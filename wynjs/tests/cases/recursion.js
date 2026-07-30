function fac(n) { return n < 2 ? 1 : n * fac(n - 1); }
console.log(fac(5));
console.log(fac(10));
function fib(n) { if (n < 2) return n; return fib(n - 1) + fib(n - 2); }
console.log(fib(10));
console.log(fib(20));
// repeated calls to the same function must not corrupt its stored body
function dbl(n) { return n * 2; }
console.log(dbl(3) + dbl(4));
console.log(dbl(1));
console.log(dbl(2));
// mutual recursion
function isEven(n) { if (n === 0) return true; return isOdd(n - 1); }
function isOdd(n) { if (n === 0) return false; return isEven(n - 1); }
console.log(isEven(10));
console.log(isOdd(7));
// deep-but-legal recursion
function depth(n) { if (n === 0) return 0; return 1 + depth(n - 1); }
console.log(depth(500));
