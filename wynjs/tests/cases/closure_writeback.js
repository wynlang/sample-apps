// A closure can MUTATE what it closes over.
//
// It could not. A capture here is a snapshot: the captured values are re-defined in
// the callee's fresh scope and copied back into the function's own slot, so a counter
// worked from INSIDE the closure and the outer variable never changed:
//
//     let c = 0
//     const bump = () => { c = c + 1; return c }
//     bump(); bump()          // returned 1 then 2
//     c                       // still 0, node says 2
//
// Not arrow-specific - a function expression assigned to a variable behaved the same
// way. A closure that cannot mutate its environment is not a closure, and this is one
// of the first things anyone writes.
//
// THE SECOND HALF IS WHY THIS FILE IS LONG. Writing every captured value back is
// wrong, because capture_env captures EVERY binding in scope rather than the ones a
// closure mentions - so one closure's stale snapshot of an unrelated variable would
// clobber a change made by someone else. That is exactly what happened to this
// suite's own pass counter, which came back 1 after two successful assertions. The
// write-back therefore only applies values the callee actually CHANGED, and the cases
// below are built to catch a regression in either direction.
//
// Semicolons throughout, per the note in arrow_params.js.

// ---- the basic mutation, both function forms --------------------------------
let c = 0;
const bumpArrow = () => { c = c + 1; return c };
console.log(bumpArrow(), bumpArrow(), c);

let d = 0;
const bumpExpr = function() { d = d + 1; return d };
console.log(bumpExpr(), bumpExpr(), d);

let e = 0;
function bumpDecl() { e = e + 1; return e }
console.log(bumpDecl(), bumpDecl(), e);

// Compound assignment and ++ through a closure, which take different code paths
// from a plain `x = x + 1`.
let f = 0;
const plusEq = () => { f += 2; return f };
console.log(plusEq(), plusEq(), f);

let g = 0;
const inc = () => { g++; return g };
console.log(inc(), inc(), g);

// ---- a factory: each closure keeps its OWN state ---------------------------
// The write-back must not leak one instance's counter into another's.
function makeCounter() {
    let n = 0;
    return () => { n = n + 1; return n }
}
const a1 = makeCounter();
const a2 = makeCounter();
console.log(a1(), a1(), a1());
console.log(a2());
console.log(a1());

// ---- NOTHING ELSE IS DISTURBED ---------------------------------------------
// The case that broke the suite's counter. `total` is incremented by one closure
// while ANOTHER closure has it captured; the second must not put its stale copy
// back.
let total = 0;
function tally(v) { if (v > 0) { total = total + 1 } }
function makeIgnorer() {
    let unused = 0;
    return () => { unused = unused + 1; return unused }
}
const ignorer = makeIgnorer();
tally(1);
ignorer();
tally(1);
ignorer();
tally(1);
console.log(total);

// A closure that reads an outer variable without writing it must leave it alone.
let readonly = 7;
const peek = () => { return readonly };
console.log(peek());

// LEFT OUT ON PURPOSE, and this is the honest boundary of what was fixed. A capture
// is still taken when the closure is CREATED, so a closure does not see a later
// write to the outer variable made by anyone else:
//
//     let ro = 7; const peek = () => ro
//     ro = 9; peek()        // 7 here, 9 in node
//
// Verified to behave identically before this change, so it is pre-existing and
// separate: this commit makes a closure's own writes escape, not its reads refresh.
// Fixing that means capture-by-reference, which is a different design.
//
// For the same reason two closures over one variable do not see each other's writes,
// and that case is not asserted here either.

// ---- closures in the array HOFs, where they mostly live --------------------
let sum = 0;
[1, 2, 3, 4].forEach(x => { sum = sum + x });
console.log(sum);

let seen = 0;
[1, 2, 3].map(x => { seen = seen + 1; return x * 2 });
console.log(seen);

// A closure mutating an outer ARRAY, which is a reference rather than a value.
const collected = [];
[5, 6, 7].forEach(x => { collected.push(x * 2) });
console.log(collected.join(","));

// ---- nesting ---------------------------------------------------------------
// An inner closure's write must reach the outermost variable, through two levels.
let deep = 0;
const outer = () => {
    const inner = () => { deep = deep + 1; return deep };
    inner();
    inner();
    return deep
};
console.log(outer(), deep);

// A parameter shadows an outer name, so the OUTER one must not change.
let shadowed = 100;
const shadow = (shadowed) => { shadowed = shadowed + 1; return shadowed };
console.log(shadow(1), shadowed);
