// The same arithmetic as loop_arith, moved into a function called per iteration.
// (cost per iteration here) - (cost per iteration in loop_arith) = cost of a call.
function add(a, b) { return a + b * 2; }
var s = 0;
for (var i = 0; i < 40000; i++) { s = add(s, i); }
console.log(s);
