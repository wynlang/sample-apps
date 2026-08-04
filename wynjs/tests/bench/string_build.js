// Repeated concatenation: exercises the runtime's string handling rather than
// the parser.
var s = "";
for (var i = 0; i < 20000; i++) { s = s + "ab"; }
console.log(s.length);
