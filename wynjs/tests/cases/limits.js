// Recursion below the interpreter's cap works ...
function depth(n) { if (n === 0) return 0; return 1 + depth(n - 1); }
console.log(depth(800));
// ... and past it the interpreter reports a JS-style RangeError instead of
// crashing. That message is the LAST line of output, so it is checked by
// limits_overflow.js rather than here.
console.log("ok");
