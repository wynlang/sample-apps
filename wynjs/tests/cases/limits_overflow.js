// Unbounded recursion must fail cleanly, not segfault.
function forever(n) { return forever(n + 1); }
forever(0);
console.log("this line must never print");
