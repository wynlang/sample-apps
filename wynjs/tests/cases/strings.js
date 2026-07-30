console.log("abc".length);
console.log("hello".toUpperCase());
console.log("HELLO".toLowerCase());
console.log("  hi  ".trim());
console.log("hello".indexOf("ll"));
console.log("hello".includes("ell"));
console.log("hello".startsWith("hel"));
console.log("hello".endsWith("llo"));
console.log("hello".slice(1, 3));
console.log("hello".charAt(1));
console.log("hello"[1]);
console.log("ab".repeat(3));
console.log("5".padStart(3, "0"));
console.log("hello world".replace("world", "wyn"));
console.log("aXbXc".replaceAll("X", "-"));
console.log("a,b,c".split(",").length);
console.log("a,b,c".split(",")[1]);
// single quotes and escapes
console.log('single');
console.log("tab\there");
console.log("quote\"inside");
// template literals, including an expression
console.log(`plain`);
console.log(`sum ${1 + 1} end`);
let nm = "Ada";
console.log(`hi ${nm}`);
