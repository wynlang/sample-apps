function classify(x) {
  if (x < 0) { return "neg"; }
  if (x === 0) { return "zero"; }
  return "pos";
}
console.log(classify(-5));
console.log(classify(0));
console.log(classify(7));
// return exits the loop AND the function
function firstBig(a) {
  for (let i = 0; i < a.length; i++) {
    if (a[i] > 10) { return a[i]; }
  }
  return -1;
}
console.log(firstBig([1, 5, 20, 30]));
console.log(firstBig([1, 2]));
// bare return yields undefined
function bare() { return; }
console.log(bare());
// nothing after a return runs
let side = 0;
function noTail() { return "done"; side = 99; }
console.log(noTail());
console.log(side);
