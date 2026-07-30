// numeric literal forms
console.log(0x10);
console.log(0xff);
console.log(0b101);
console.log(0o17);
console.log(1_000);
console.log(1e3);
console.log(1.5e-2);
// optional chaining short-circuits the WHOLE chain
let nul = null;
console.log(nul?.foo);
console.log(nul?.a?.b);
console.log(nul?.f());
let deep = { a: { b: 42 } };
console.log(deep?.a?.b);
// nullish coalescing keeps 0 and "" (unlike ||)
console.log(0 ?? "fallback");
console.log("" ?? "fallback");
console.log(null ?? "fallback");
console.log(undefined ?? "fallback");
// spread: arrays AND strings
console.log([...[1, 2], 3].join(","));
console.log([..."abc"].join("-"));
function count(...xs) { return xs.length; }
console.log(count(..."xy"));
// destructuring, incl. rest
const [d1, d2] = [7, 8];
console.log(d1 + "," + d2);
const [head, ...tail] = [1, 2, 3];
console.log(head);
console.log(tail.length);
const { q1, q2 } = { q1: 1, q2: 2 };
console.log(q1 + "," + q2);
// braceless single-statement loop bodies
let sum = 0;
for (const v of [1, 2, 3]) sum += v;
console.log(sum);
let keys = "";
for (let k in { a: 1, b: 2 }) keys += k;
console.log(keys);
// a method may be NAMED get or set
class Box {
  constructor() { this.v = 0; }
  add(n) { this.v += n; return this; }
  get() { return this.v; }
  set(n) { this.v = n; return this; }
}
console.log(new Box().add(1).add(2).get());
console.log(new Box().set(9).get());
