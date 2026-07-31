// Arrow functions: the BLOCK body, which is the form the suite never covered.
//
// The existing `functions` case exercises arrows with EXPRESSION bodies
// (`x => x * 2`) and passed 19/19 while every BLOCK-bodied arrow
// (`x => { return x * 2 }`) silently evaluated to undefined. One token of body
// was being dropped, so the leading `return` never made it into the stored
// source. That is the most common arrow shape in real code and the interpreter
// got it wrong at exit 0, which is the worst failure mode there is.
//
// Every assertion below is a case where the dropped-token bug produced
// `undefined` instead of a value. Two ADJACENT arrow defects found while writing
// this file are deliberately NOT here, because they are still open and a test
// case is a statement about what works: an arrow with a DEFAULT PARAMETER
// (`(a, b = 1) => {}`) truncates the program silently, and an arrow does not
// write a captured binding back to the enclosing scope (a plain `function`
// does). Both are logged in the findings doc.

// The minimal shape.
const one = () => { return 1 }
console.log(one())

// A parameter, and arithmetic - the shape that appears inside every .map().
const dbl = (x) => { return x * 2 }
console.log(dbl(21))

// No parens around a single parameter.
const trbl = x => { return x * 3 }
console.log(trbl(5))

// As a callback, which is where it matters most.
console.log([1, 2, 3].map(x => { return x * 2 }).join(","))
console.log([1, 2, 3, 4].filter(n => { return n % 2 === 0 }).join(","))
console.log([1, 2, 3, 4].reduce((a, b) => { return a + b }, 0))

// Several statements: the return is not the first statement, so this case
// passes even with the bug - it is here so the contrast is recorded.
const sum3 = (a, b, c) => {
    const partial = a + b
    return partial + c
}
console.log(sum3(1, 2, 3))

// An early return inside a block-bodied arrow.
const sign = n => {
    if (n < 0) { return "neg" }
    if (n > 0) { return "pos" }
    return "zero"
}
console.log(sign(-5), sign(5), sign(0))

// A block body that returns nothing is undefined - the one case where
// `undefined` is the CORRECT answer, so the fix must not overshoot.
const nothing = () => { }
console.log(nothing())

// Nested braces inside the body must not end the capture early.
const pick = flag => {
    if (flag) { return { a: 1 } }
    return { a: 2 }
}
console.log(pick(true).a, pick(false).a)

// An arrow returning an arrow, both with block bodies.
const adder = a => { return b => { return a + b } }
console.log(adder(2)(3))

// A block-bodied arrow stored on an object and called as a method.
const obj = { go: () => { return "went" } }
console.log(obj.go())

// A block-bodied arrow used in a conditional expression.
const parity = n => { return n % 2 === 0 ? "even" : "odd" }
console.log(parity(4), parity(7))
