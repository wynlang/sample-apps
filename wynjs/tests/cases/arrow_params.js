// Arrow function DEFAULT parameters.
//
// `(a, b = 1) => ...` did not parse as an arrow at all. The parameter scan
// accepted only `)` or `,` after a name, so an `=` fell through to the
// backtrack, the parenthesised list was re-read as an ordinary expression, and
// the `=>` that followed was then unexpected - which silently truncated the
// REST OF THE PROGRAM. A file using one printed nothing and exited 0.
//
// A plain `function` with default parameters has always worked, so the fix
// encodes arrow defaults into the same `name=default` params string that
// p_func_expr produces and callfn already understands, rather than adding a
// second mechanism.
//
// TWO THINGS ARE LEFT OUT ON PURPOSE, because a test case is a statement about
// what works and both are pre-existing defects that this change does not touch
// (each verified to behave identically before it):
//
//   - a default that is an EXPRESSION rather than a literal, `(n, f = 2 * 3)`,
//     still yields undefined;
//   - a REST parameter in an arrow, `(...xs) => ...`, still truncates.
//
// Semicolons are deliberate throughout. An expression-bodied arrow whose
// statement has no terminating `;` is a third separate pre-existing defect (the
// body capture runs to end-of-input), and every existing case in this directory
// is written with semicolons for the same reason. Mixing any of these in would
// make a failure here ambiguous about which bug it found.

// A string default, omitted then supplied.
const greet = (name, punct = "!") => { return "hi " + name + punct };
console.log(greet("wyn"));
console.log(greet("wyn", "?"));

// A numeric default, expression body.
const add = (a, b = 10) => a + b;
console.log(add(5), add(5, 1));

// Every parameter defaulted, called with nothing, one, then both.
const origin = (x = 1, y = 2) => x * 100 + y;
console.log(origin(), origin(3), origin(3, 4));

// A falsy explicit argument must NOT be replaced by the default - the classic
// bug in hand-rolled `d = d || 10` defaulting.
const keepZero = (n, d = 10) => n + d;
console.log(keepZero(1, 0));

// A default alongside a block body and an early return.
const clamp = (n, lo = 0) => {
    if (n < lo) { return lo }
    return n
};
console.log(clamp(-5), clamp(5), clamp(-5, -10));

// A defaulted parameter on an arrow stored in an object, called as a method.
const obj = { f: (a, b = 5) => a * b };
console.log(obj.f(3), obj.f(3, 2));
