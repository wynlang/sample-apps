console.log(Math.floor(3.7));
console.log(Math.ceil(3.2));
console.log(Math.abs(-42));
console.log(Math.max(1, 5, 3));
console.log(Math.min(1, 5, 3));
console.log(typeof 42);
console.log(typeof "s");
console.log(typeof true);
console.log(typeof undefined);
console.log(typeof null);
console.log(typeof []);
console.log(typeof {});
console.log(typeof function () {});
console.log(JSON.stringify({ a: 1, b: "x" }));
console.log(JSON.stringify([1, 2]));
console.log(Array.isArray([]));
console.log(Array.isArray(1));
console.log(Array.from("abc").join(","));
// Map and Set
let mp = new Map();
mp.set("k", 1);
console.log(mp.get("k"));
console.log(mp.size);
console.log(mp.has("k"));
let st = new Set();
st.add(1); st.add(1); st.add(2);
console.log(st.size);
// console.log with several arguments joins them with a space
console.log("a", "b", 1);
// truthiness
console.log(!!"");
console.log(!!"a");
console.log(!!0);
console.log(!!1);
