let a = 1;
const b = 2;
var c = 3;
console.log(a + b + c);
a = 10;
console.log(a);
// comma-separated declarations in one statement
let p = 0, q = 5, r = "s";
console.log(p);
console.log(q);
console.log(r);
// declaration without an initialiser
let u;
console.log(u);
// compound assignment
let ca = 10;
ca += 5; console.log(ca);
ca -= 3; console.log(ca);
ca *= 2; console.log(ca);
ca /= 4; console.log(ca);
ca %= 4; console.log(ca);
let cs = "hi"; cs += "!"; console.log(cs);
// ++ / -- are postfix: they yield the OLD value
let i = 5;
console.log(i++);
console.log(i);
console.log(i--);
console.log(i);
