// The untaken side of && || ?? ?: must NOT be evaluated
let n = 0;
function bump() { n++; return 1; }
false && bump();
console.log(n);
true && bump();
console.log(n);
true || bump();
console.log(n);
false || bump();
console.log(n);
// ternary picks exactly one arm
let t = 0;
function inc() { t++; return "y"; }
let r = false ? inc() : "n";
console.log(r);
console.log(t);
// ?? only evaluates the right side for null/undefined
let d = 0;
function def() { d++; return "D"; }
console.log(0 ?? def());
console.log(d);
console.log(null ?? def());
console.log(d);
// values, not just booleans
console.log(true && false);
console.log(true || false);
console.log(!false);
console.log(5 > 3 ? "y" : "n");
