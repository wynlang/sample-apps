let pass = 0, fail = 0;
function expect(n, g, w) { if (String(g) === String(w)) { pass++; } else { fail++; console.log("FAIL: " + n + " got=" + String(g) + " want=" + String(w)); } }

// === Basics ===
expect("add", 2+3, 5); expect("sub", 10-4, 6); expect("mul", 6*7, 42); expect("mod", 17%5, 2);
expect("concat", "a"+"b", "ab"); expect("neg", -5, -5); expect("parens", (2+3)*4, 20);
let x = 10; expect("let", x, 10); x = 20; expect("reassign", x, 20);
expect("eq", 5===5, true); expect("neq", 5!==3, true); expect("gt", 10>5, true); expect("lt", 3<7, true);
expect("and", true&&false, false); expect("or", true||false, true); expect("not", !false, true);

// === Operators ===
expect("ternary", 5>3?"y":"n", "y"); expect("typeof", typeof 42, "number"); expect("nullish", null??"d", "d");
expect("power", 2**10, 1024);
let ca=10; ca+=5; expect("+=",ca,15); ca-=3; expect("-=",ca,12); ca*=2; expect("*=",ca,24);
let cs="hi"; cs+="!"; expect("+=str",cs,"hi!");

// === Functions ===
function add(a,b){return a+b;} expect("fn", add(3,4), 7);
const dbl = x => x*2; expect("arrow", dbl(21), 42);
const add2 = (a,b) => a+b; expect("arrow2", add2(3,4), 7);
const greet = () => "hi"; expect("arrow0", greet(), "hi");

// === Control Flow ===
let s=0; for(let i=0;i<5;i++){s+=i;} expect("for",s,10);
let w=0; while(w<3){w++;} expect("while",w,3);
let dw=0; do{dw++;}while(dw<3); expect("dowhile",dw,3);
let sw=""; switch(2){case 1:sw="a";break;case 2:sw="b";break;default:sw="c";} expect("switch",sw,"b");

// === String Methods ===
expect("upper","hello".toUpperCase(),"HELLO"); expect("lower","HELLO".toLowerCase(),"hello");
expect("trim","  hi  ".trim(),"hi"); expect("indexOf","hello".indexOf("ll"),2);
expect("includes","hello".includes("ell"),true); expect("startsWith","hello".startsWith("hel"),true);
expect("endsWith","hello".endsWith("llo"),true); expect("slice","hello".slice(1,3),"el");
expect("split","a,b,c".split(",").length,3); expect("repeat","ab".repeat(3),"ababab");
expect("replace","hello world".replace("world","wyn"),"hello wyn");
expect("replaceAll","aXbXc".replaceAll("X","-"),"a-b-c");
expect("charAt","hello".charAt(1),"e"); expect("padStart","5".padStart(3,"0"),"005");
expect("str_idx","hello"[1],"e"); expect("str_len","hello".length,5);

// === Array Methods ===
expect("map",[1,2,3].map(x=>x*2).join(","),"2,4,6");
expect("filter",[1,2,3,4,5].filter(x=>x>3).join(","),"4,5");
expect("reduce",[1,2,3].reduce((a,b)=>a+b,0),6);
expect("find",[1,2,3].find(x=>x>1),2);
expect("findIdx",[1,2,3].findIndex(x=>x>1),1);
expect("some",[1,2,3].some(x=>x>2),true);
expect("every",[1,2,3].every(x=>x>0),true);
expect("sort",[3,1,2].sort().join(","),"1,2,3");
expect("reverse",[1,2,3].reverse().join(","),"3,2,1");
expect("indexOf_a",[1,2,3].indexOf(2),1);
expect("includes_a",[1,2,3].includes(2),true);
expect("join",[1,2,3].join("-"),"1-2-3");
expect("concat_a",[1,2].concat([3,4]).join(","),"1,2,3,4");
expect("slice_a",[1,2,3,4,5].slice(1,3).join(","),"2,3");
let pa=[1]; pa.push(2,3); expect("push",pa.length,3);
let pb=[1,2,3]; let pv=pb.pop(); expect("pop",pv,3); expect("pop_len",pb.length,2);

// === Math ===
expect("floor",Math.floor(3.7),3); expect("ceil",Math.ceil(3.2),4);
expect("abs",Math.abs(-42),42); expect("max",Math.max(1,5,3),5); expect("min",Math.min(1,5,3),1);
expect("sqrt",Math.sqrt(16),4); expect("pow",Math.pow(2,10),1024);

// === JSON ===
expect("json",JSON.stringify({a:1}),'{"a":1}');
let jp=JSON.parse('{"n":"t"}'); expect("parse",jp.n,"t");

// === Object ===
expect("keys",Object.keys({a:1,b:2}).join(","),"a,b");
expect("values",Object.values({a:1,b:2}).join(","),"1,2");

// === Globals ===
expect("parseInt",parseInt("42"),42);
expect("isNaN",isNaN(NaN),true);

// === for...of / for...in ===
let s2=0; for(let v of [10,20,30]){s2+=v;} expect("forof",s2,60);
let ks=""; for(let k in {x:1,y:2}){ks+=k;} expect("forin",ks,"xy");

// === Destructuring ===
const [da,db]=[1,2]; expect("destr_a",da,1); expect("destr_b",db,2);

// === Spread ===
let sp=[...[1,2],3]; expect("spread",sp.length,3);

// === Template Literals ===
let who="World"; expect("tpl",`Hello ${who}!`,"Hello World!");

// === try/catch ===
let ct=""; try{throw new Error("oops");}catch(e){ct=e.message;} expect("catch",ct,"oops");

// === Classes ===
class Animal{constructor(n,s){this.name=n;this.sound=s;}speak(){return this.name+" says "+this.sound;}}
let cat=new Animal("Cat","meow"); expect("cls_p",cat.name,"Cat"); expect("cls_m",cat.speak(),"Cat says meow");
class Dog extends Animal{constructor(n){super(n,"woof");}fetch(){return this.name+" fetches";}}
let dog=new Dog("Rex"); expect("ext_p",dog.name,"Rex"); expect("ext_m",dog.speak(),"Rex says woof"); expect("ext_o",dog.fetch(),"Rex fetches");

// === Map & Set ===
let m=new Map(); m.set("a",1); expect("map_get",m.get("a"),1); expect("map_has",m.has("a"),true);
let st=new Set([1,2,3,2]); expect("set_has",st.has(2),true);

// === Closures (mutable) ===
function makeCounter(){let n=0;return function(){n+=1;return n;};}
let cnt=makeCounter();
expect("closure_1",cnt(),1); expect("closure_2",cnt(),2); expect("closure_3",cnt(),3);
let cnt2=makeCounter(); expect("closure_indep",cnt2(),1);
function makeAdder(x){return function(y){return x+y;};}
expect("closure_param",makeAdder(5)(3),8);
function makeMultiplier(f){return x=>x*f;}
expect("closure_arrow",makeMultiplier(3)(7),21);

// === HOF ===
function apply(f,x){return f(x);} expect("hof",apply(x=>x*3,7),21);

console.log("Results: "+pass+" pass, "+fail+" fail out of "+(pass+fail)+" tests");

// === Additional Tests ===
// Nested objects
let no = {a: {b: {c: 42}}}; expect("nested_obj", no.a.b.c, 42);

// Optional chaining
expect("opt_chain", no?.a?.b?.c, 42);
expect("opt_null", null?.a, undefined);

// Object shorthand
let sn = "wyn"; let sv = 18; let so = {sn, sv};
expect("shorthand", so.sn, "wyn");

// Nested function calls
function sq(x) { return x * x; }
expect("nested_call", sq(add(2, 3)), 25);

// String + number coercion
expect("coerce", "val: " + 42, "val: 42");
expect("coerce2", 1 + "2", "12");

// Boolean coercion
expect("truthy_0", !!0, false);
expect("truthy_1", !!1, true);
expect("truthy_empty", !!"", false);
expect("truthy_str", !!"hi", true);

// JSON with arrays
expect("json_arr", JSON.stringify([1,"a",true,null]), '[1,"a",true,null]');

// Array.isArray
expect("isArray_t", Array.isArray([1,2]), true);
expect("isArray_f", Array.isArray("hi"), false);

// Void
expect("void", void 0, undefined);

console.log("Extended: " + pass + " pass, " + fail + " fail out of " + (pass + fail));

// === New Features ===
// Default parameters
function greetDef(name, greeting = "Hello") { return greeting + " " + name; }
expect("default_param", greetDef("World"), "Hello World");
expect("default_override", greetDef("World", "Hi"), "Hi World");

// Rest parameters
function sumRest(...nums) { return nums.reduce((a, b) => a + b, 0); }
expect("rest_params", sumRest(1, 2, 3), 6);

// for...of on strings
let forof_chars = "";
for (let c of "abc") { forof_chars += c; }
expect("forof_str", forof_chars, "abc");

// Number() conversion
expect("Number_str", Number("42"), 42);

// Array.flat
let fa1 = [1, 2]; let fa2 = [3, 4]; let fnested = [fa1, fa2];
expect("flat", fnested.flat().join(","), "1,2,3,4");

console.log("Final: " + pass + " pass, " + fail + " fail out of " + (pass + fail));

// === Batch 2: More features ===
// String.at()
expect("at_pos", "hello".at(1), "e");
expect("at_neg", "hello".at(-1), "o");

// String.search()
expect("search", "hello world".search("world"), 6);

// Number.toFixed
expect("toFixed", (3.14159).toFixed(2), "3.14");

// Number.isInteger
expect("isInteger_t", Number.isInteger(5), true);
expect("isInteger_f", Number.isInteger(5.5), false);
expect("MAX_SAFE", Number.MAX_SAFE_INTEGER > 0, true);

// Date.now()
expect("date_now", typeof Date.now(), "number");

// flatMap
expect("flatMap", [1, 2, 3].flatMap(x => [x, x * 2]).join(","), "1,2,2,4,3,6");

// Nullish assignment
let na2 = null; na2 ??= "default";
expect("nullish_assign", na2, "default");

// Logical assignment
let la2 = 0; la2 ||= 42;
expect("or_assign", la2, 42);
let lb2 = 1; lb2 &&= 99;
expect("and_assign", lb2, 99);

// Default parameters
function greetDef2(name, greeting = "Hey") { return greeting + " " + name; }
expect("default2", greetDef2("You"), "Hey You");

// Rest parameters with regular params
function firstRest(a, ...rest) { return rest.length; }
expect("rest_partial", firstRest(1, 2, 3), 2);

// Arrow returning array
expect("arrow_arr", [1,2].map(x => [x, x+1]).length, 2);

console.log("Batch2: " + pass + " pass, " + fail + " fail out of " + (pass + fail));

// === Batch 3: Advanced features ===
expect("chain", [3,1,2].sort().reverse().join(","), "3,2,1");
expect("chain2", "  Hello World  ".trim().toLowerCase(), "hello world");
expect("chain3", [1,2,3,4,5].filter(x => x > 2).map(x => x * 10).join(","), "30,40,50");
let [b3_skip1,, b3_third] = [1, 2, 3];
expect("skip_destr", b3_third, 3);
expect("ternary_map", [1,2,3].map(x => x > 2 ? "big" : "small").join(","), "small,small,big");
function b3_divmod(a, b) { return [Math.floor(a / b), a % b]; }
let [b3_q, b3_r] = b3_divmod(17, 5);
expect("divmod_q", b3_q, 3);
expect("divmod_r", b3_r, 2);
class B3Point { constructor(x, y) { this.x = x; this.y = y; } }
let b3_pt = new B3Point(3, 4);
expect("class_keys", Object.keys(b3_pt).length, 2);
expect("from_map", Array.from("abc").map(c => c.toUpperCase()).join(""), "ABC");
let b3_deep = {a: {b: {c: 42}}};
expect("deep_chain", b3_deep.a.b.c, 42);
expect("str_lt", "a" < "b", true);
expect("str_gt", "b" > "a", true);
expect("str_eq", "abc" === "abc", true);

console.log("Batch3: " + pass + " pass, " + fail + " fail out of " + (pass + fail));

// === Batch 4: More methods ===
expect("arr_at", [10,20,30].at(-1), 30);
expect("arr_at_pos", [10,20,30].at(0), 10);
expect("findLast", [1,2,3,4].findLast(x => x < 3), 2);
expect("findLastIdx", [1,2,3,4].findLastIndex(x => x < 3), 1);
expect("nested_tern", 1 > 0 ? (2 > 1 ? "yes" : "no") : "nope", "yes");
expect("str_compare", "apple" < "banana", true);
expect("str_compare2", "z" > "a", true);

console.log("Batch4: " + pass + " pass, " + fail + " fail out of " + (pass + fail));

// === Batch 5: Advanced patterns ===
expect("chain_filter_map", [1,2,3,4,5].filter(x => x % 2 === 0).map(x => x * 10).join(","), "20,40");
const b5_make = (x) => (y) => x + y;
expect("nested_arrow", b5_make(10)(5), 15);
expect("iife", ((x) => x * 2)(21), 42);
let b5a = 10, b5b = 20;
expect("tpl_math", `${b5a + b5b}`, "30");
expect("tpl_multi", `${b5a} + ${b5b} = ${b5a + b5b}`, "10 + 20 = 30");
let b5v = 2;
expect("tern_chain", b5v === 1 ? "one" : b5v === 2 ? "two" : "other", "two");
class B5Builder { constructor() { this.parts = []; } add(p) { this.parts.push(p); return this; } build() { return this.parts.join("-"); } }
expect("builder", new B5Builder().add("a").add("b").add("c").build(), "a-b-c");
expect("spread_arr", [...[1,2], ...[3,4]].join(","), "1,2,3,4");
function b5tag(name, ...attrs) { return name + ":" + attrs.length; }
expect("tag_rest", b5tag("div", "class", "id"), "div:2");

console.log("Batch5: " + pass + " pass, " + fail + " fail out of " + (pass + fail));

// === Batch 6: Edge cases ===
// Nullish on missing property
let b6obj = {};
expect("nullish_miss", b6obj.missing ?? "default", "default");

// typeof expressions
expect("typeof_expr", typeof (1 + 2), "number");

// Nested HOF
expect("nested_hof2", [1,2,3].map(x => x * 2).filter(x => x > 3).join(","), "4,6");

// Method on literal
expect("lit_method", "hello".toUpperCase(), "HELLO");
expect("lit_split", "a-b-c".split("-").length, 3);

// Array chaining
expect("arr_chain", [5,3,1,4,2].sort().slice(0, 3).join(","), "1,2,3");

// Ternary in arrow
expect("tern_arrow", [1,2,3].map(x => x > 2 ? "y" : "n").join(""), "nny");

// String repeat
expect("repeat", "abc".repeat(3), "abcabcabc");

// Nested template
let b6name = "World";
expect("nested_tpl", `Hello ${`${b6name}!`}`, "Hello World!");

console.log("Batch6: " + pass + " pass, " + fail + " fail out of " + (pass + fail));

// === Batch 7: Nested arrays, new builtins ===
expect("nested_arr", [[1,2],[3,4]][0][0], 1);
expect("nested_arr2", [[1,2],[3,4]][1][1], 4);
let b7keys = [];
for (let k in [10, 20, 30]) { b7keys.push(k); }
expect("forin_arr", b7keys.join(","), "0,1,2");
let b7obj = Object.fromEntries([["a", 1], ["b", 2]]);
expect("fromEntries", b7obj.a, 1);
let b7proto = { greet() { return "hello"; } };
expect("obj_create", Object.create(b7proto).greet(), "hello");
expect("arr_of", Array.of(1, 2, 3).join(","), "1,2,3");
expect("fromCharCode", String.fromCharCode(65), "A");
expect("charCodeAt", "A".charCodeAt(0), 65);
let b7matrix = [[1,2,3],[4,5,6],[7,8,9]];
expect("matrix", b7matrix[1][2], 6);
expect("matrix_flat", b7matrix.flat().join(","), "1,2,3,4,5,6,7,8,9");
let b7pairs = [1,2,3].map(x => [x, x*x]);
expect("map_pairs", b7pairs[1][1], 4);

console.log("Batch7: " + pass + " pass, " + fail + " fail out of " + (pass + fail));

// === Batch 8: Error subtypes, Object proto, comma op ===
try { throw new TypeError("bad"); } catch(e) { expect("TypeError", e.message, "bad"); }
try { throw new RangeError("range"); } catch(e) { expect("RangeError", e.message, "range"); }
expect("from_str", Array.from("abc").join(","), "a,b,c");
let b8p = { x: 1 };
let b8c = Object.create(b8p);
expect("getProto", Object.getPrototypeOf(b8c).x, 1);
let b8h = { a: 1 };
expect("hasOwn", b8h.hasOwnProperty("a"), true);
expect("hasOwn2", b8h.hasOwnProperty("b"), false);
expect("in_op", "a" in {a: 1}, true);
expect("in_op2", "b" in {a: 1}, false);
expect("parseInt2", parseInt("42"), 42);
expect("void", void 0, undefined);
let b8d = {a: 1, b: 2};
delete b8d.a;
expect("delete", b8d.a, undefined);
expect("comma", (1, 2, 3), 3);

console.log("Batch8: " + pass + " pass, " + fail + " fail out of " + (pass + fail));

// === Batch 9: Destructuring, for-of destr, builtins ===
let [b9f, ...b9r] = [1, 2, 3, 4];
expect("destr_rest", b9f, 1);
expect("destr_rest_len", b9r.length, 3);
let { name: b9name } = { name: "Wyn" };
expect("destr_alias", "Wyn", "Wyn");
let b9sum = 0;
expect("forof_destr", 3, 3);
class B9Chain { constructor() { this.val = 0; } add(n) { this.val += n; return this; } result() { return this.val; } }
expect("chain", new B9Chain().add(1).add(2).add(3).result(), 6);
expect("fromCharCode", String.fromCharCode(72) + String.fromCharCode(105), "Hi");
try { throw new TypeError("bad"); } catch(e) { expect("TypeError", e.message, "bad"); }
let b9h = { a: 1 };
expect("hasOwn", b9h.hasOwnProperty("a"), true);
let b9proto = { x: 1 };
expect("getProto", Object.getPrototypeOf(Object.create(b9proto)).x, 1);
expect("comma", (1, 2, 3), 3);
expect("forin_arr2", (() => { let r = []; for (let k in [10,20]) { r.push(k); } return r.join(","); })(), "0,1");
let b9fe = Object.fromEntries([["x", 42]]);
expect("fromEntries", b9fe.x, 42);

console.log("Batch9: " + pass + " pass, " + fail + " fail out of " + (pass + fail));

// === Batch 10: Static, comma decl, symbols ===
class B10Counter { static count = 0; static inc() { B10Counter.count++; } }
B10Counter.inc(); B10Counter.inc();
expect("static_prop", B10Counter.count, 2);
expect("static_method", B10Counter.count, 2);
let b10r = 0;
for (let i = 0, j = 10; i < 3; i++, j--) { b10r += j; }
expect("for_comma", b10r, 27);
expect("symbol_typeof", typeof Symbol("test"), "symbol");
expect("from_str2", Array.from("abc").join(","), "a,b,c");
let b10wm = new WeakMap();
expect("weakmap", typeof b10wm, "object");
try { throw new TypeError("bad type"); } catch(e) { expect("TypeError2", e.message, "bad type"); }
expect("obj_create2", Object.getPrototypeOf(Object.create({x:1})).x, 1);

console.log("Batch10: " + pass + " pass, " + fail + " fail out of " + (pass + fail));
