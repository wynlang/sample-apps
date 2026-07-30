// + is left-associative: numbers add until a string appears, then it concatenates
console.log(1 + 2 + "x");
console.log("x" + 1 + 2);
console.log("a" + "b");
console.log("5" * 2);
console.log(2 + 3 + "");
console.log(String(null));
console.log(String(undefined));
console.log(String(true));
console.log([1, 2] + "");
console.log("" + [1, [2, 3]]);
console.log(Number("42"));
console.log(parseInt("42px"));
console.log(parseInt("abc"));
console.log(parseFloat("3.5rem"));
