// try/catch/finally, including a RUNTIME fault being catchable.
//
// Regression: a `throw` left the remainder of its block unread, and because this is
// a single-pass parse-and-evaluate interpreter those leftover tokens became the next
// thing executed - so the statement after `throw` ran and `catch` bound undefined.
try { throw 1; console.log("must not print"); } catch (e) { console.log("caught " + e); }
console.log("after");

// Reading a property of null/undefined is a catchable TypeError, not undefined.
try { null.x; } catch (e) { console.log(e.name + ": " + e.message); }
try { undefined.y; } catch (e) { console.log(e.name); }
let n = null;
try { n.foo(); } catch (e) { console.log(e.message); }
try { n["bar"]; } catch (e) { console.log(e.message); }

// A runtime fault unwinds out of a function into the caller's handler.
function boom() { let z = null; return z.q; }
try { boom(); } catch (e) { console.log("unwound: " + e.name); }

// finally runs on both paths, and an early exit does not derail the handler.
try { throw new Error("x"); console.log("no"); } catch (e) { console.log("c " + e.message); } finally { console.log("f1"); }
try { console.log("body"); } catch (e) { console.log("never"); } finally { console.log("f2"); }

// Optional chaining still short-circuits instead of throwing.
console.log(n?.a);
let deep = { b: null };
console.log(deep?.b?.c);

// `return` inside try, with statements after it, must not leak either.
function r() { try { return "ret"; console.log("nope"); } finally { console.log("fin"); } }
console.log(r());

// A caught error does not stop later code.
console.log("end");

// `delete o.k` removes the key and leaves the object intact (it used to erase the
// whole binding, which only became visible once undefined.x started throwing).
let d = { a: 1, b: 2 };
console.log(delete d.a, d.a, d.b, "a" in d);
let keep = 5;
console.log(delete keep, typeof keep);

// `arguments` is the real argument list. It used to be bound to undefined, so
// `arguments.length` read as undefined - a silent wrong answer that only became
// visible once property access on undefined started throwing.
function argc() { return arguments.length; }
console.log(argc(1, 2, 3));
function argn() { return arguments[1]; }
console.log(argn("a", "b"));
function argsum() { let s = 0; for (let i = 0; i < arguments.length; i++) s += arguments[i]; return s; }
console.log(argsum(1, 2, 3, 4));

// Deep recursion still works (the arguments object must not be allocated on every
// call, or the array pool is exhausted and this reports a spurious RangeError).
function fib(n) { return n < 2 ? n : fib(n - 1) + fib(n - 2); }
console.log(fib(18));
