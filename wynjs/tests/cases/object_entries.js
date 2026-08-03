// Object.entries built two arrays at once into a BUMP-allocated store, so the outer
// array's slots overlapped the pairs: e[0][0] came back as the pair itself (typeof
// "object", length 2) instead of the key, and printing or concatenating it recursed
// until the stack overflowed. Reading it was fine, which made it look like a printing
// bug. Fixed by completing every pair before allocating the outer array.
var e = Object.entries({ a: 1 });
console.log(e.length);
console.log(e[0].length);
console.log(e[0][0]);
console.log(e[0][1]);
console.log(JSON.stringify(e));
console.log(typeof e[0][0]);
console.log(e[0][0].length);

// A string value as well as a numeric one, and the pair read back in a loop - the
// shape real code uses.
var f = Object.entries({ x: "s", y: 2 });
for (var i = 0; i < f.length; i++) console.log(f[i][0] + "=" + f[i][1]);

// Round trip through fromEntries.
console.log(JSON.stringify(Object.fromEntries(Object.entries({ a: 1, b: 2 }))));
