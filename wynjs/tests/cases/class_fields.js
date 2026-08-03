// Class field declarations, public and private (#).
// The field must work as the LAST member of the class body too: the parser
// used to fall into the method-parameter loop and consume the rest of the
// PROGRAM hunting for a ")", so everything after the class silently vanished.
class Counter {
  n = 0;
  step = 2;
  bump() { this.n = this.n + this.step; return this.n }
}
const c = new Counter();
console.log(c.n, c.step);
console.log(c.bump());
console.log(c.bump());

// Private fields are not visible as properties, but are readable from inside.
class Secret {
  #p = 41;
  pub = "open";
  reveal() { return this.#p + 1 }
  bumpPriv() { this.#p = this.#p + 1; return this.#p }
}
const s = new Secret();
console.log(s.reveal());
console.log(s.bumpPriv());
console.log(s.pub);
console.log(Object.keys(s).join(","));
console.log(JSON.stringify(s));

// A field initializer sees other fields via `this`, and a constructor still
// runs AFTER the field initializers.
class WithCtor {
  a = 5;
  constructor(b) { this.b = this.a + b }
}
console.log(new WithCtor(1).b);

// A field declared last, with no trailing semicolon, then more program.
class Last {
  v = 7
}
console.log(new Last().v);
console.log("reached the end");
