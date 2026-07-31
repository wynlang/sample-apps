// Bitwise and shift operators.
//
// These were not merely missing - they were WORSE than missing. `&` and `|`
// tokenized but no precedence level consumed them, so `5 & 3` printed
// "5 undefined 3" (console.log's arguments split at the stranded token). And `^`,
// `<<`, `>>`, `>>>` did not lex at all, which set tt = 0 - the EOF token - so a
// single `~5` or `1 ^ 2` silently truncated the rest of the file and exited 0.
//
// So this file is as much about the truncation as the arithmetic: every check
// after the first would have vanished.
//
// Semicolons throughout, per the note in arrow_params.js: an expression-bodied
// arrow with no terminating semicolon is a separate pre-existing defect.

// The four bitwise operators, on small positive values where the answer is
// derivable by hand: 5 = 101, 3 = 011.
console.log(5 & 3);
console.log(5 | 3);
console.log(5 ^ 3);
console.log(~5);

// Shifts.
console.log(1 << 4);
console.log(256 >> 4);
console.log(-8 >> 1);

// A LOGICAL shift differs from an arithmetic one only on negatives, which is the
// whole reason both exist.
//
// `-1 >>> 0` is THE discriminating case and is here deliberately: a shift of zero
// still has to reinterpret the operand as unsigned 32-bit, so the answer is
// 4294967295 rather than -1. Without it, an implementation that skipped the
// masking entirely still matched every other line in this group - measured by
// mutation, not assumed.
console.log(-8 >>> 28);
console.log(8 >>> 1);
console.log(-1 >>> 0);
console.log(-1 >>> 28);
console.log(-16 >>> 2);

// Operands PAST 32 bits, which is where the two halves of the >>> conversion are
// told apart. Wyn's ints are 64-bit and JS coerces to 32, so a value above 2^32
// must be truncated before it is reinterpreted as unsigned - the wrap-negatives
// step alone is not enough, and each of these two lines is caught only by the
// group it belongs to (mutation-checked both ways).
console.log(4294967296 >>> 0);
console.log(8589934592 >>> 1);
console.log(-4294967297 >>> 0);

// PRECEDENCE. JavaScript binds & tighter than ^ tighter than |, so this is
// 1 | (2 ^ (3 & 4)) = 1 | (2 ^ 0) = 3. One flat level of "bitwise" would make
// them left-associative peers and give a different answer.
console.log(1 | 2 ^ 3 & 4);
console.log(5 & 3 | 8);

// Shifts bind tighter than a comparison and looser than +, so this is 1 << 5.
console.log(1 << 2 + 3);
console.log((1 << 2) + 3);
console.log(1 << 2 < 8);

// JavaScript's own famous wart: & is LOOSER than ===, so this is 5 & (1 === 1),
// which is 5 & true = 1. Reproduced rather than silently "fixed", because an
// implementation that is better than the language is the harder thing to debug.
console.log(5 & 1 === 1);

// Only the low 5 bits of a shift count are used, so a count of 32 is a no-op
// rather than zero - and rather than the undefined behaviour a raw C shift by 32
// would be.
console.log(1 << 32);
console.log(1 << 33);

// The ~~ idiom for truncation toward zero, which is why ~ has to be a unary
// operator and not just a token.
console.log(~~3.7);
console.log(~~-3.7);

// Hex literals through the bitwise path, since masking is what these are for.
console.log(0xff & 0x0f);
console.log(0xf0 | 0x0f);

// Chained, to prove left-associativity within one level.
console.log(1 | 2 | 4 | 8);
console.log(15 & 7 & 3);
console.log(1 ^ 3 ^ 1);

// Bitwise on a value that arrives as a string coerces it, as JS does.
console.log("6" & 3);

// A bit-flag pattern - the reason a language needs these at all.
const READ = 1;
const WRITE = 2;
const EXEC = 4;
let perms = READ | WRITE;
console.log(perms);
console.log((perms & WRITE) !== 0);
console.log((perms & EXEC) !== 0);
perms = perms | EXEC;
console.log(perms);
perms = perms & ~WRITE;
console.log(perms);

// If the file got this far, nothing truncated.
console.log("reached the end");
