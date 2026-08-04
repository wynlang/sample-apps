// Recursion: call overhead compounds with depth, and each frame re-lexes the body.
function fib(n) { if (n < 2) return n; return fib(n - 1) + fib(n - 2); }
console.log(fib(21));
