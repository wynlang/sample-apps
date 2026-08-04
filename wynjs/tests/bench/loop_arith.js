// Pure interpreter overhead: a bare loop, no function call anywhere.
// Isolates the cost of re-lexing the loop's condition/update/body per iteration.
var s = 0;
for (var i = 0; i < 200000; i++) { s = s + i * 2 - 1; }
console.log(s);
