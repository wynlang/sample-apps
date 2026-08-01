// `==` and `!=`, which are NOT `===` with a different spelling.
//
// Both operators were implemented with the strict comparison, so `1 == "1"` was
// false and `null == undefined` was false - two of the handful of facts every
// JavaScript programmer knows, and the second is the idiom for "absent" that a
// great deal of real code is built on.
//
// The order of the rules is what makes this correct, so the cases are grouped by
// rule and each group contains the case that a wrong order would break.
//
// Semicolons throughout, per the note in arrow_params.js.

// ---- null and undefined: equal to each other, and to NOTHING else ----------
// This exception comes FIRST. A numeric coercion would get it wrong, because
// Number(null) is 0 - so `0 == null` would become true, and `x == null` would
// stop meaning "absent".
console.log(null == undefined);
console.log(undefined == null);
console.log(null == null);
console.log(undefined == undefined);
console.log(0 == null);
console.log(0 == undefined);
console.log("" == null);
console.log(false == null);
console.log(null != undefined);

// ---- same type: compare strictly, nothing to coerce ------------------------
console.log(1 == 1);
console.log(1 == 2);
console.log("a" == "a");
console.log("a" == "b");
console.log(true == true);
console.log(true == false);

// ---- number and string: compare NUMERICALLY --------------------------------
console.log(1 == "1");
console.log("1" == 1);
console.log(1 == "1.0");
console.log(1 == " 1 ");
console.log(1 == "2");
console.log(0 == "");
console.log(0 == " ");

// A STRING THAT IS NOT A NUMBER IS NaN, AND NaN EQUALS NOTHING. This is the case
// a naive coercion gets wrong: turning "abc" into 0 would make it equal to 0,
// which is the single most surprising thing loose equality does NOT do.
console.log("abc" == 0);
console.log("abc" == "abc");
console.log("12abc" == 12);

// ---- boolean against number or string --------------------------------------
// A boolean becomes 1 or 0, which is why only exactly-1 is loosely true.
console.log(1 == true);
console.log(0 == false);
console.log(2 == true);
console.log("1" == true);
console.log("0" == false);
console.log("" == false);
console.log("abc" == true);

// ---- strict comparison is untouched ----------------------------------------
// The whole point is that the two operators now DIFFER. Every line here is the
// strict counterpart of a true line above.
console.log(1 === "1");
console.log(null === undefined);
console.log(0 === false);
console.log("" === 0);
console.log(1 === 1);
console.log(1 !== "1");
console.log(1 !== 1);

// ---- != is the exact negation of == ----------------------------------------
console.log(1 != "1");
console.log(1 != "2");
console.log("abc" != 0);
console.log(0 != false);

// ---- two objects compare by identity ---------------------------------------
const a = { x: 1 };
const b = { x: 1 };
const c = a;
console.log(a == b);
console.log(a == c);
console.log(a === c);

// A practical use: the "absent" check that motivates the whole first rule.
function label(v) {
    if (v == null) { return "missing" }
    return "present"
}
console.log(label(null), label(undefined), label(0), label(""), label("x"));
