// if / else if / else
function sign(x) { if (x > 0) { return "+"; } else if (x < 0) { return "-"; } else { return "0"; } }
console.log(sign(3) + sign(-3) + sign(0));
// while
let w = 0; while (w < 3) { w++; } console.log(w);
// do-while runs at least once
let dw = 0; do { dw++; } while (dw < 3); console.log(dw);
let once = 0; do { once++; } while (false); console.log(once);
// for with continue and break
let s = ""; for (let i = 0; i < 5; i++) { if (i === 2) continue; s += i; } console.log(s);
let t = ""; for (let i = 0; i < 9; i++) { if (i === 3) break; t += i; } console.log(t);
// nested loops
let g = "";
for (let i = 0; i < 3; i++) { for (let j = 0; j < 2; j++) { g += i + "" + j + " "; } }
console.log(g.trim());
// switch with fallthrough-free breaks and a default
function sw(x) { switch (x) { case 1: return "one"; case 2: return "two"; default: return "many"; } }
console.log(sw(1) + "," + sw(2) + "," + sw(9));
// for-of and for-in
for (const v of [9, 8]) { console.log(v); }
let obj = { k1: 1, k2: 2 };
for (let k in obj) { console.log(k); }
