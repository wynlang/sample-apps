class Point {
  constructor(x, y) { this.x = x; this.y = y; }
  sum() { return this.x + this.y; }
}
let p = new Point(2, 3);
console.log(p.x);
console.log(p.sum());
// inheritance with super
class Point3 extends Point {
  constructor(x, y, z) { super(x, y); this.z = z; }
  total() { return this.sum() + this.z; }
}
let q = new Point3(1, 2, 3);
console.log(q.sum());
console.log(q.total());
// method chaining via `return this`
class Chain {
  constructor() { this.v = 0; }
  add(n) { this.v += n; return this; }
  get() { return this.v; }
}
console.log(new Chain().add(1).add(2).add(3).get());
// try / catch / finally ordering
try { throw new Error("boom"); } catch (e) { console.log("caught " + e.message); } finally { console.log("finally"); }
// a thrown value unwinds out of a function
function thrower() { throw new Error("deep"); }
try { thrower(); } catch (e) { console.log("outer " + e.message); }
// typed errors keep their message
try { throw new TypeError("bad type"); } catch (e) { console.log(e.message); }
// no throw -> catch is skipped, finally still runs
try { console.log("body"); } catch (e) { console.log("never"); } finally { console.log("fin2"); }
