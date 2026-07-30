// declaration, expression, arrow (0/1/n params), expression-bodied arrow
function decl(a, b) { return a + b; }
console.log(decl(3, 4));
const expr = function (a) { return a * 2; };
console.log(expr(5));
const arrow0 = () => "hi";
console.log(arrow0());
const arrow1 = (x) => x * 2;
console.log(arrow1(21));
const arrowN = (a, b) => a + b;
console.log(arrowN(3, 4));
const arrowBare = x => x + 1;
console.log(arrowBare(1));
// default parameters
function withDefault(a, b = 10) { return a + b; }
console.log(withDefault(1));
console.log(withDefault(1, 2));
// rest parameters
function countArgs(...xs) { return xs.length; }
console.log(countArgs(1, 2, 3));
// higher-order: a function taking and returning a function
function twice(f) { return (x) => f(f(x)); }
console.log(twice(arrowBare)(5));
// callbacks through array methods
console.log([1, 2, 3].map((x) => x * 2).join(","));
console.log([1, 2, 3, 4].filter((x) => x % 2 === 0).join(","));
console.log([1, 2, 3].reduce((a, b) => a + b, 0));
// immediately-invoked
console.log((function () { return "iife"; })());
