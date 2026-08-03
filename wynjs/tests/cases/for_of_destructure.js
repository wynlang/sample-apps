// for-of with a destructuring pattern. The for-in/for-of paths were gated on the
// loop variable being a plain identifier, so a `[` or `{` pattern fell through to
// the C-style for(;;) path — whose captured condition source was empty, read as
// truthy, and spun to the million-iteration cap. It presented as a hang.
for (const [a, b] of [[1, 2], [3, 4]]) console.log(a, b);

for (const { k, v } of [{ k: "x", v: 1 }, { k: "y", v: 2 }]) console.log(k, v);

// Braced body, `let`, and a nested/short pattern.
for (let [a] of [[9], [8]]) { console.log(a) }

// Object.entries is the common shape this blocks. One key only: WynJS enumerates in
// hashmap order rather than JS's guaranteed insertion order, and that is a separate
// defect (ROADMAP OPEN-B2) — this case is about the destructuring, so it must not
// depend on, or silently enshrine, the ordering.
const scores = { alice: 3 };
for (const [name, n] of Object.entries(scores)) console.log(name, n);

// A rest element inside the pattern, and renaming in an object pattern.
for (const [head, ...tail] of [[1, 2, 3]]) console.log(head, JSON.stringify(tail));
for (const { k: key } of [{ k: "renamed" }]) console.log(key);

// break and continue still work through the destructuring path.
for (const [a, b] of [[1, 1], [2, 2], [3, 3]]) {
  if (a === 1) continue;
  if (a === 3) break;
  console.log("body", a, b);
}

// The loop variable must not leak, and the program must continue after the loop.
let total = 0;
for (const [, n] of [["a", 1], ["b", 2]]) total += n;
console.log(total);
console.log("reached the end");
