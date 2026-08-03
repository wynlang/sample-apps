# WynJS

A JavaScript interpreter written in Wyn. ~3,500 lines of Wyn, one binary, no
dependencies.

Everything in "Supported" below was verified by running it and diffing against
real `node`. The "Not supported" section is equally load-bearing: it lists what
this does *not* do, so nothing here has to be taken on trust.

**How the claims are checked.** `tests/run_node_parity.sh` runs every case under
real `node` and diffs that against the recorded `.out`. This is the only test
that can catch a wrong *expectation* rather than a wrong result — and it has:
`parseInt("abc")` was once recorded as `0` when node says `NaN`, and the suite
passed 20/20 while the interpreter was wrong. **Never generate a `.out` from
WynJS's own output.** Always:

```sh
node tests/cases/NAME.js > tests/cases/NAME.out
```

## Layout

```
wynjs/
├── wyn.toml                     # project config
├── src/main.wyn                 # the whole interpreter: lexer + parser + evaluator
└── tests/
    ├── test_main.wyn            # Wyn-side runner (diffs each case against its .out)
    ├── run_node_parity.sh       # diffs each case against REAL node — the real gate
    ├── cases/NAME.js            # a JS program ...
    ├── cases/NAME.out           # ... and its exact expected stdout, from node
    └── test.js                  # legacy suite: one file, 217 self-checking assertions
```

Single file on purpose. An earlier attempt split storage into `src/runtime.wyn`
and `src/values.wyn`; those were never imported by anything and were deleted
rather than left to rot.

## Usage

```sh
# run a JS file
../../wyn/wyn run src/main.wyn hello.js

# or build a standalone interpreter first (much faster per run)
../../wyn/wyn build src/main.wyn -o wynjs
./wynjs hello.js
```

## Tests

```sh
cd sample-apps/wynjs
../../wyn/wyn run tests/test_main.wyn      # 27 pass, 0 fail
./tests/run_node_parity.sh                 # 23 identical, 3 known-different, 0 unexplained
```

The three known differences are listed with reasons inside
`run_node_parity.sh`: `uncaught_throw` (node prints a multi-line stack trace with
absolute machine paths; WynJS prints one line), and `promises_math` /
`limits_overflow` (microtask interleaving and float formatting at the extremes).

## Supported

Each item below was run against node and matched.

**Values and operators.** Numbers (`0xff`, `0b101`, `0o17`, `1_000`, `1e3`,
`1.5e-2`), strings (single, double, and template literals with `${}`), booleans,
`null`, `undefined`, arrays, objects, functions.
`+ - * / % **`, `=== !== == != < > <= >=`, `&& || !`, `? :`, `??`, `?.`,
`+= -= *= /= %=`, postfix `++` / `--`, `typeof`, `delete`, `void`, `in`, and the
bitwise set (`& | ^ ~ << >>`). Precedence and associativity are right,
`&&`/`||`/`??`/`? :` short-circuit (the untaken branch is parsed but not
evaluated), and `+` follows JS coercion: `1 + 2 + "x"` is `"3x"`,
`"x" + 1 + 2` is `"x12"`.

**Declarations.** `let`, `const`, `var`, comma-separated declarators,
declaration without initialiser.

**Control flow.** `if` / `else if` / `else`, `while`, `do-while`, C-style `for`,
`for-in`, `for-of`, `break`, `continue`, `switch`/`case`/`default`. Every loop
form accepts a braceless single-statement body as well as a block.

**Functions.** Declarations, expressions, arrow functions (0, 1, or n params;
expression or block body), default parameters, rest parameters, recursion
(including mutual), higher-order functions, IIFEs, `arguments` (`.length` and
indexing), and closures that capture and *mutate* an enclosing binding.

**Objects and arrays.** Literals, dot and bracket access, nesting, property add
and update, shorthand methods with a working `this`, `Object.keys`/`values`/
`assign`/`fromEntries`/`create`/`getPrototypeOf`, `delete obj.key` (removes the
key, leaves the object intact), `hasOwnProperty`, destructuring (array and
object, with rest), spread of arrays *and* strings.

**Getters and setters.** `{ get x() {...}, set x(v) {...} }` and the same on a
`class`, including `this` inside the accessor, accessors inherited through a
prototype, appearing in `Object.keys`, and `JSON.stringify` recording the
*computed* value. A property or method may itself be *named* `get` or `set` —
that is different, and is tested.

**Classes.** `class`, `constructor`, methods, `extends`, `static` members
(fields and methods), `return this` chaining.

**Errors.** `throw`, `try`/`catch`/`finally`, catch binding, nested try, and
rethrow. `Error`/`TypeError`/`RangeError` with `.message` and `.name`.
`finally` runs on the normal path, on `throw`, and on `return`/`break`/`continue`
out of the `try`; a `try`/`finally` with no `catch` lets the throw keep
propagating. **Runtime faults are catchable**: reading a property of `null` or
`undefined` raises a real `TypeError` whose message matches node's
(`Cannot read properties of null (reading 'x')`) rather than silently yielding
`undefined`. An uncaught throw prints `Uncaught TypeError: msg` and exits
non-zero rather than crashing.

**Built-ins.** `console.log` (multiple args, space-joined), `Math` (`floor`,
`ceil`, `round`, `abs`, `max`, `min`, `PI`, ...), `JSON.stringify`/`parse`,
`Map`, `Set` (but see the `new Map([...])` limitation below), `Promise`
(synchronous — see below), `parseInt`/`parseFloat`
(numeric prefix, so `parseInt("42px")` is `42` and `parseInt("abc")` is `NaN`),
`Number`, `String`, `Boolean`, `isNaN`, `Array.isArray`, `Array.from`,
`Array.of`, `String.fromCharCode`, `.toFixed` (rounds, not truncates), ~20 string
methods and ~20 array methods including `map`/`filter`/`reduce`/`sort`/`splice`/
`flat`/`flatMap`/`find`/`some`/`every`. Ordinary float arithmetic matches node,
including `0.1 + 0.2` and `1/3`.

## Not supported

Verified gaps, each reproduced against node — not guesses.

- **Regular expressions.** Regex literals are lexed as ordinary text, so
  `"abc".replace(/b/,"X")` yields `abc X` where node gives `aXc`, and
  `/ab/.test(s)` does not work. `RegExp` is a stub carrying `source`/`flags`;
  `.match()` returns `null` and `.search()` does a plain substring search.
- **Generators.** `function*` and `yield` parse but do not suspend:
  `function* g(){yield 1}; [...g()]` gives `undefined` where node gives `[ 1 ]`.
- **`async` / `await` and the event loop.** `async function f(){return 1};
  f().then(...)` prints nothing. `Promise` resolution is **synchronous** —
  `.then` runs immediately, there is no microtask queue, and `setTimeout` invokes
  its callback at once, ignoring the delay (so a `setTimeout` callback runs
  *before* the following synchronous line, the opposite of node).
- **`instanceof`.** Always `false`, including `e instanceof TypeError`. Use
  `e.name` to discriminate errors.
- **Labelled `break` / `continue`.** The label is ignored, so `break outer`
  breaks only the innermost loop.
- **Prefix `++` / `--`.** `++n` and `--n` are no-ops that also yield the wrong
  value; postfix `n++` / `n--` work. Write `n = n + 1`.
- **Default values in destructuring.** `const {a, b = 9} = {a: 1}` leaves `b` as
  `undefined`. Default *parameters* in a function signature do work.
- **`Object.entries`** returns an array of the right length whose elements are
  unusable (`Object.entries({a:1})[0][0]` prints nothing).
- **`new Map([[k, v]])`** — seeding a `Map` from an iterable yields `undefined`,
  so the following `.get` throws. `new Set([1, 2])` *does* work. Build a map
  empty and fill it: `const m = new Map(); m.set(k, v)`.
- **`String.raw`** is unimplemented.
- **`this` at top level** is `undefined` rather than an object.
- **`WeakMap`** is an object stub with no weak semantics; `Symbol` exists only
  far enough for `typeof`.
- **`Object.keys` ordering** can differ from node's insertion order (the backing
  store is a hash map).
- Number formatting differs at the extremes (`1e21`, `-0`).
- **Recursion depth is capped at 900 frames.** The interpreter recurses on the
  host C stack (measured: it dies near 1350), so the cap converts a segfault into
  `RangeError: Maximum call stack size exceeded`.
- **Fixed-capacity pools.** 65536 scope slots / 8192 arrays / 65536 array
  elements. Exhausting one reports the same `RangeError` instead of panicking.
- No modules at runtime beyond simple `import`/`export`.

## How it works

A single-pass tree-walking interpreter with no AST: the lexer, parser, and
evaluator are one recursive descent that evaluates as it parses. Function bodies
are stored as **source strings** and re-parsed on each call, with the parser
state (`src`, `pos`, `tt`, `tv`) saved and restored around the call.

Values are tagged strings: `n:42`, `s:hello`, `b:true`, `N:` (null), `u:`
(undefined), `f:3` (function id), `a:1` (array id), `o:0` (object id). Objects
are Wyn `HashMap`s in a global array indexed by id; prototype chains are a
parallel array of parent ids. Scopes are flat parallel arrays with a stack of
marks for push/pop. Closures snapshot their environment and write it back on
return, which is what makes mutable capture work. Getters and setters are stored
under mangled keys (`__get_x` / `__set_x`) and invoked on property access.

Untaken branches are still *parsed* (the interpreter must advance `pos`) but a
`noeval` depth counter suppresses every state mutation and all output while
inside them — that is how short-circuiting works without an AST.

**The consequence to know about.** Because there is no AST, control flow that
exits a block early cannot just set a flag and unwind — the *remaining tokens of
that block are still in the stream*, and whatever reads next would execute them.
`p_block_body` therefore skips to its own closing brace on an early exit. Getting
this wrong is not a subtle bug: `try { throw 1; console.log("x") } catch (e) {}`
printed `x` and then reached `catch` one statement out of step, so the handler saw
no throw and bound `e` to `undefined`.

## Wyn compiler bugs found and worked around

Reviving this exposed six compiler bugs. None are patched here; each is worked
around in the interpreter with a comment at the site. The workarounds are the
reason `sdup()` appears throughout: it forces a fresh string allocation to defeat
a premature release.

1. **Storing a string parameter into a global array element loses it.**
   `fn set(v: string) { arr[0] = v }` emits `{ const char* t = mk(); set(t);
   wyn_rc_release(t); }` — the callee stored the pointer, the caller freed it.
   Workaround: `arr[0] = v + ""`.
2. **A local aliasing a global string is released at function exit, freeing the
   global.** `var s1 = src; ...; src = s1` emits `wyn_rc_release(s1)` before
   returning, leaving `src` dangling. Broke every second call of any JS function
   (the stored body was freed). Workaround: copy on both save and restore.
3. **A `HashMap` bound inside a loop or `if` body gets a spurious
   `hashmap_free`.** `while c >= 0 { var m = maps[c]; ... }` frees the map the
   global array still owns — use-after-free, segfault on the next lookup. Binding
   at function-top scope does not. Workaround: index inline, or read through a
   top-level-scoped accessor.
4. **`return [a, b]` of two local strings yields two empty elements.**
   Workaround: build the array with `push()`.
5. **`for i in 0..IDENT { }` misparses** as a struct literal ("Expected ':'
   after field name"). Workaround: `for i in 0..(IDENT)`.
6. **`fn str_to_int` collides with a `wyn_runtime.h` symbol** and fails the C
   compile with "redefinition". Workaround: pick another name.

## Interpreter bugs fixed

For the record, since the survey once reported this as "builds-but-wrong-output"
— it produced *zero* output on its own test suite.

Most recently: a `throw` (or `return`) left the rest of its block unread, so the
statement after it executed and `catch` bound `undefined` — meaning
`try { null.x } catch (e) { ... }` printed nothing at all; property access on
`null`/`undefined` returned `undefined` instead of raising a catchable
`TypeError`; `finally` was skipped when the `try` exited via
`return`/`break`/`continue`, and a `try`/`finally` with no `catch` swallowed the
throw; `{ get x() {...} }` returned the function object instead of invoking it
(same for class accessors); `delete o.k` erased the entire object binding rather
than the one key; and `arguments` was bound to `undefined`.

Earlier: `console.log` emitted a doubled newline; the ternary eagerly evaluated
*both* arms (so every recursive function recursed forever); `&&`/`||`/`??` did
not short-circuit; `switch` with a `return` in a non-final case looped forever; a
class method named `get` was parsed as an accessor and skipped;
`parseInt("42px")` and `vtonum("-1.0")` panicked; `Math.ceil` and `Math.round`
both called `floor`; `.toFixed` truncated instead of rounding; `[1,2] + ""` gave
`"[array]"`; `Promise` methods panicked on a missing internal slot; string spread
and every braceless loop body silently did nothing.
