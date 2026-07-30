// An uncaught throw reports name: message and stops the program.
console.log("before");
throw new TypeError("no good");
console.log("after must not print");
