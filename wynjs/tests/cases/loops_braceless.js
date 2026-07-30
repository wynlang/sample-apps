// Every loop form must run a BRACELESS single-statement body, not just a block.
let s = 0;
for (let i = 0; i < 3; i++) s += i;
console.log(s);
let b = [];
for (let i = 0; i < 500; i++) b.push(i);
console.log(b.length);
let w = 0;
while (w < 3) w++;
console.log(w);
let d = 0;
do d++; while (d < 3);
console.log(d);
let ofSum = 0;
for (const v of [1, 2, 3]) ofSum += v;
console.log(ofSum);
let inKeys = "";
for (let k in { a: 1, b: 2 }) inKeys += k;
console.log(inKeys);
// braced forms still work, with continue and break
let cont = "";
for (let i = 0; i < 5; i++) { if (i === 2) continue; cont += i; }
console.log(cont);
let brk = "";
for (let i = 0; i < 9; i++) { if (i === 3) break; brk += i; }
console.log(brk);
// toFixed rounds, it does not truncate
console.log((123.456).toFixed(2));
console.log((-1.567).toFixed(1));
console.log((3).toFixed(2));
console.log((2.5).toFixed(0));
