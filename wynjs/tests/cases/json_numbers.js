// JSON.stringify returned the raw number payload, and a whole number produced by
// ARITHMETIC carries a ".0" internally — so [1,2].map(x => x*2) serialized as
// [2.0,4.0] where JS, having one number type, prints [2,4]. A literal 2 was
// already correct, which is why this only showed on computed values.
console.log(JSON.stringify([1, 2].map(x => x * 2)));
console.log(JSON.stringify([2 * 2, 4 / 2, 10 - 7]));
console.log(JSON.stringify({ v: 2 * 3 }));
console.log(JSON.stringify({ a: [3 * 3], b: { c: 8 / 4 } }));
console.log(JSON.stringify([1e3, 2 ** 10]));
console.log(JSON.stringify([-4 / 2, 0]));

// Genuine fractions must still round-trip as fractions, not be truncated.
console.log(JSON.stringify([1.5, 2.25, 0.1, -0.5]));
console.log(JSON.stringify({ half: 1 / 2 }));

// Literals were always fine; keep them covered so a fix here cannot regress them.
console.log(JSON.stringify([2, 4]));
console.log(JSON.stringify({ n: 7, s: "7", b: true, z: null }));

// Numbers reached through filter/reduce, the other common computed shapes.
// NOT covered here: `.map(([a,b]) => a*b)` — destructuring in a FUNCTION PARAMETER
// is unimplemented and yields undefined (ROADMAP OPEN-B2). This case is about number
// formatting, so it must not depend on that.
console.log(JSON.stringify([1, 2, 3, 4].filter(n => n % 2 === 0)));
console.log(JSON.stringify([1, 2, 3, 4].reduce((a, b) => a + b, 0)));
