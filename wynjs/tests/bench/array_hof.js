// map + filter over a large array: the callfn2 two-argument fast path, which
// re-lexes the callback body once per ELEMENT.
var a = [];
for (var i = 0; i < 20000; i++) { a.push(i); }
var b = a.map(function (x) { return x * 2; });
var c = b.filter(function (x) { return x % 3 === 0; });
console.log(c.length, b[19999]);
