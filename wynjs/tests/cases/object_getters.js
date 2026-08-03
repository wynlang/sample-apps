// Object literal accessors: `get x() {}` / `set x(v) {}`.
//
// Regression: `get` was stored as an ordinary key and `x()` was then parsed as a
// method, so `{ get x() { return 7 } }.x` read back as the function, not 7.
const o = { get x() { return 7 } };
console.log(o.x);

// A getter sees `this`.
const p = { v: 10, get double() { return this.v * 2 } };
console.log(p.double);

// Setter runs on assignment; getter reads the stored value back.
const q = { _n: 0, get n() { return this._n }, set n(val) { this._n = val * 3 } };
q.n = 5;
console.log(q.n, q._n);

// A property may legitimately be NAMED get or set - those are not accessors.
const r = { get() { return "method named get" }, set: 42 };
console.log(r.get(), r.set);

// Accessors mixed with plain properties, in any order.
const s = { get a() { return 1 }, b: 2, get c() { return 3 } };
console.log(s.a, s.b, s.c);

// An accessor is an enumerable own property, and JSON.stringify records the
// COMPUTED value.
const t = { a: 1, get b() { return 2 } };
console.log(Object.keys(t).join(","));
console.log(JSON.stringify(t));

// A getter can be computed from other properties each time it is read.
const u = { n: 1, get sq() { return this.n * this.n } };
console.log(u.sq);
u.n = 4;
console.log(u.sq);

// Class accessors have the same root cause and the same fix.
class C { get v() { return 9 } }
console.log(new C().v);
class D { constructor() { this._n = 2 } get n() { return this._n * 5 } set n(x) { this._n = x } }
const d = new D();
console.log(d.n);
d.n = 10;
console.log(d.n);
// A class method may also be named get.
class E { get() { return "method" } }
console.log(new E().get());
// A getter coexists with `return this` chaining.
class F { constructor() { this.v = 0 } add(n) { this.v += n; return this } get result() { return this.v } }
console.log(new F().add(3).add(4).result);
