// object literal, dot and bracket access, nesting
let o = { x: 1, y: "two" };
console.log(o.x);
console.log(o.y);
console.log(o["x"]);
let nested = { p: { q: { r: 7 } } };
console.log(nested.p.q.r);
let quoted = { "k-1": 5 };
console.log(quoted["k-1"]);
// adding and updating properties
let e = {};
e.a = 1;
e.a = 2;
e.b = 3;
console.log(e.a + "," + e.b);
console.log(Object.keys(e).join(","));
console.log(Object.values(e).join(","));
// a method sees `this`
let counter = { n: 3, dbl() { return this.n * 2; } };
console.log(counter.dbl());
// missing property is undefined
console.log(o.nope);
// arrays: literal, index, assign, length
let a = [1, 2, 3];
console.log(a[0]);
console.log(a.length);
a[1] = 9;
console.log(a.join("|"));
console.log([].length);
// nested arrays
let m = [[1, 2], [3]];
console.log(m.length);
console.log(m[0][1]);
// mutation methods
let p = [1];
p.push(2, 3);
console.log(p.length);
console.log(p.pop());
console.log(p.join(","));
// order-sensitive: sort() is LEXICOGRAPHIC by default in JS
console.log([3, 20, 100].sort().join(","));
console.log([3, 20, 100].sort((x, y) => x - y).join(","));
console.log([1, 2, 3].reverse().join(","));
console.log([1, 2, 3].includes(2));
console.log([1, 2, 3].indexOf(3));
