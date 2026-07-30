// * binds tighter than +; parens override; unary minus binds tighter than **
console.log(2 + 3 * 4);
console.log((2 + 3) * 4);
console.log(10 - 2 - 3);
console.log(2 ** 3 ** 2);
console.log(-(2 ** 2));
console.log(1 + 2 * 3 - 4 / 2);
console.log(17 % 5);
console.log(-7 % 3);
console.log(10 / 4);
console.log(1 < 2 === true);
