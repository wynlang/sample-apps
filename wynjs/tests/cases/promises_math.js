// Math.round is round-half-toward-+Infinity, not floor
console.log(Math.round(2.5));
console.log(Math.round(2.4));
console.log(Math.round(-2.5));
console.log(Math.round(-2.6));
console.log(Math.floor(-2.5));
console.log(Math.ceil(-2.5));
// Promises are resolved SYNCHRONOUSLY here: .then runs immediately.
let p = new Promise((resolve) => resolve(5));
p.then((v) => console.log("then " + v));
Promise.resolve(9).then((v) => console.log("resolved " + v));
Promise.reject("bad").catch((e) => console.log("caught " + e));
// Map / Set still work (they use a different internal slot than Promise)
let m = new Map();
m.set("k", 1);
console.log(m.get("k"));
console.log(m.size);
let st = new Set();
st.add(1); st.add(1);
console.log(st.size);
