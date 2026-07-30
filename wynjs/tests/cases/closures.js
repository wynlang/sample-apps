// Each counter keeps its OWN mutable binding
function makeCounter() {
  let c = 0;
  return function () { c++; return c; };
}
const c1 = makeCounter();
console.log(c1());
console.log(c1());
console.log(c1());
// capture of a parameter
function adder(a) { return function (b) { return a + b; }; }
const add5 = adder(5);
console.log(add5(3));
console.log(add5(10));
// arrow form captures the same way
function adderArrow(a) { return (b) => a + b; }
console.log(adderArrow(100)(1));
// inner shadow does not leak out
let sh = 1;
function shadow() { let sh = 2; return sh; }
console.log(shadow());
console.log(sh);
