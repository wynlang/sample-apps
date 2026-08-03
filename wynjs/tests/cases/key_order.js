// JS GUARANTEES that string keys enumerate in insertion order. WynJS stored
// properties in a HashMap and enumerated in BUCKET order, so Object.keys,
// for-in and JSON.stringify all came out scrambled — visibly so once an object
// had more than a couple of keys.
const o = { alice: 3, bob: 5, carol: 7 };
console.log(Object.keys(o).join(","));
console.log(Object.values(o).join(","));
console.log(JSON.stringify(o));
console.log(JSON.stringify(Object.entries(o)));

let seen = "";
for (const k in o) seen += k + " ";
console.log(seen.trim());

// Enough keys that bucket order and insertion order cannot coincide by luck.
const many = {};
for (let i = 0; i < 12; i++) many["k" + i] = i;
console.log(Object.keys(many).join(","));

// Rewriting a value keeps the key at its ORIGINAL position...
const r = { a: 1, b: 2, c: 3 };
r.b = 99;
console.log(JSON.stringify(r));

// ...but deleting and re-adding moves it to the END.
delete r.a;
r.a = 42;
console.log(JSON.stringify(r));

// A getter is an enumerable own property and keeps its declared position.
const g = { x: 1, get mid() { return 2 }, z: 3 };
console.log(Object.keys(g).join(","));
console.log(JSON.stringify(g));

// Insertion order holds for nested objects and through class instances.
console.log(JSON.stringify({ outer: { z: 1, y: 2 }, after: 3 }));
class P { first = 1; second = 2; third = 3 }
console.log(Object.keys(new P()).join(","));
console.log(JSON.stringify(new P()));
