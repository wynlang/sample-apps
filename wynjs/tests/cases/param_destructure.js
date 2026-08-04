// Destructuring in a FUNCTION PARAMETER. The params string is split on "," when a
// call binds arguments, so a pattern's own commas used to tear it apart and every
// name bound to undefined — silently. `.map(([k,v]) => ...)`, the shape
// Object.entries() feeds, was the common casualty.

// Arrow, array and object patterns, alone and alongside plain parameters.
console.log([[1, 2]].map(([a, b]) => a + b)[0]);
console.log([{ x: 7 }].map(({ x }) => x)[0]);
const three = (x, [a, b], { c }) => x + a + b + c;
console.log(three(1, [2, 3], { c: 4 }));
const patFirst = ([a], y) => a + y;
console.log(patFirst([1], 2));

// A default on a parameter that FOLLOWS a leading pattern, taken and overridden.
const withDef = ([a], y = 5) => a + y;
console.log(withDef([1]));
console.log(withDef([1], 9));
const strDef = ([a], s = "z") => a + s;
console.log(strDef([1]));

// A rest parameter after a pattern.
const withRest = ([a], ...r) => a + r.length;
console.log(withRest([1], 2, 3));

// function declarations and function expressions, both pattern kinds.
function fd([a, b]) { return a + b }
function fo({ a }) { return a }
console.log(fd([1, 2]), fo({ a: 5 }));
const fe = function ([a]) { return a };
console.log(fe([7]));

// Object pattern with renaming, in a parameter.
const renamed = ({ k: v }) => v;
console.log(renamed({ k: 9 }));

// The HOF shapes this unblocks.
console.log(JSON.stringify(Object.entries({ a: 1, b: 2 }).map(([k, v]) => k + v)));
console.log(JSON.stringify([[1, 2], [3, 0]].filter(([a, b]) => b > 0)));
console.log(JSON.stringify([[1, 2], [3, 4]].map(([a, b]) => a * b)));

// Plain parameters, an index argument, and a parenthesised EXPRESSION must all be
// unaffected — the pattern branch has to not swallow them.
console.log([1, 2, 3].map((v, i) => v * i).join(","));
console.log([1, 2, 3, 4].reduce((a, b) => a + b, 0));
console.log((1 + 2) * 3);
const arrLit = ([1, 2]);
console.log(arrLit.length);
