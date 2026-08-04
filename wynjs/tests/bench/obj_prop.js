// Object property get and set in a loop.
var o = { a: 1, b: 2, c: 3 };
var s = 0;
for (var i = 0; i < 50000; i++) { o.a = o.a + 1; s = s + o.a + o.b + o.c; }
console.log(s, o.a);
