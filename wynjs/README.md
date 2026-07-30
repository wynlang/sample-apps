# WynJS

A JavaScript interpreter written in Wyn. 3,034 lines of Wyn, one 191 KB binary,
no dependencies.

Everything claimed below is covered by `tests/` and was verified by running it.
The "Not supported" section is equally load-bearing: it lists what this does
*not* do, so nothing here has to be taken on trust.

## Layout

```
wynjs/
├── wyn.toml                # project config
├── src/main.wyn            # the whole interpreter: lexer + parser + evaluator
└── tests/
    ├── test_main.wyn       # test runner (build once, run every case, diff output)
    ├── cases/NAME.js       # a JS program ...
    ├── cases/NAME.out      # ... and its exact expected stdout
    └── test.js             # legacy suite: one file, 217 self-checking assertions
```

Single file on purpose. An earlier attempt split storage into `src/runtime.wyn`
and `src/values.wyn`; those were never imported by anything (`main.wyn` kept its
own private copies of all 12 functions) and were deleted rather than left to rot.

## Usage

```sh
# run a JS file
../../wyn/wyn run src/main.wyn -- hello.js

# or build a standalone interpreter first (much faster per run)
../../wyn/wyn build src/main.wyn -o wynjs
./wynjs hello.js
```

## Tests

```sh
cd sample-apps/wynjs
../../wyn/wyn run tests/test_main.wyn
```

The runner builds the interpreter once, then for each case runs the `.js` and
diffs stdout against the `.out`, printing the case name and the first differing
lines. Current state:

```
=== WynJS test suite ===
  ok   arith_precedence
  ok   string_number_coercion
  ok   declarations
  ok   control_flow
  ok   functions
  ok   closures
  ok   recursion
  ok   short_circuit
  ok   early_return
  ok   objects_arrays
  ok   strings
  ok   classes_errors
  ok   builtins
  ok   modern_syntax
  ok   loops_braceless
  ok   promises_math
  ok   limits
  ok   limits_overflow
  ok   uncaught_throw
  ok   legacy assertions (tests/test.js, 217 asserts)

20 pass, 0 fail
```

The cases deliberately include the things a hand-written interpreter gets wrong:
operator precedence and associativity, `string + number` coercion, short-circuit
evaluation (the untaken branch must not run), closure capture of a mutable
binding, recursion, and early `return`.

## Supported

**Values and operators.** Numbers (including `0xff`, `0b101`, `0o17`, `1_000`,
`1e3`, `1.5e-2`), strings (single, double, and template literals with `${}`),
booleans, `null`, `undefined`, arrays, objects, functions.
`+ - * / % **`, `=== !== == != < > <= >=`, `&& || !`, `? :`, `??`, `?.`,
`+= -= *= /= %=`, `++ --`, `typeof`, `instanceof`, `delete`, `void`.
Precedence and associativity are right, `&&`/`||`/`??`/`? :` short-circuit
properly, and `+` follows JS coercion (`1 + 2 + "x"` is `"3x"`, `"x" + 1 + 2` is
`"x12"`).

**Declarations.** `let`, `const`, `var`, comma-separated declarators
(`let a = 1, b = 2`), declaration without initialiser.

**Control flow.** `if` / `else if` / `else`, `while`, `do-while`, C-style `for`,
`for-in`, `for-of`, `break`, `continue`, `switch`/`case`/`default`. Every loop
form accepts a braceless single-statement body as well as a block.

**Functions.** Declarations, expressions, arrow functions (0, 1, or n params;
expression or block body), default parameters, rest parameters, recursion
(including mutual recursion), higher-order functions, closures that capture and
*mutate* an enclosing binding, IIFEs.

**Objects and arrays.** Literals, dot and bracket access, nesting, property add
and update, shorthand methods with a working `this`, `Object.keys/values/
entries/assign/fromEntries/create/getPrototypeOf`, destructuring (array and
object, with rest), spread of arrays *and* strings.

**Classes.** `class`, `constructor`, methods, `extends`, `super(...)`, `static`
members, `return this` chaining. A method may itself be named `get` or `set`.

**Errors.** `throw`, `try`/`catch`/`finally`, `Error`/`TypeError` with
`.message`. An uncaught throw prints `Uncaught TypeError: msg` and exits
non-zero rather than crashing.

**Built-ins.** `console.log` (multiple args, space-joined), `Math` (`floor`,
`ceil`, `round`, `abs`, `max`, `min`, `PI`, ...), `JSON.stringify`/`parse`,
`Map`, `Set`, `Promise` (synchronous — see below), `parseInt`, `parseFloat`
(both take a numeric prefix, so `parseInt("42px")` is `42`), `Number`, `String`,
`Boolean`, `isNaN`, `Array.isArray`, `Array.from`, `String.fromCharCode`,
`.toFixed` (which rounds, not truncates), ~20 string methods and ~20 array
methods including `map`/`filter`/`reduce`/`sort`/`splice`.

## Not supported

Verified gaps, not guesses:

- **Getter/setter accessors.** `get n() { return 5; }` returns the function
  object instead of invoking it. (A method *named* `get` works — that is
  different and is tested.)
- **Generators.** `function*` parses, but `it.next().value` is `undefined`;
  `yield` does not suspend.
- **Real async.** `async`/`await` parse and `Promise` exists, but resolution is
  **synchronous**: `.then` runs immediately, there is no microtask queue or
  event loop. `setTimeout` invokes its callback at once, ignoring the delay.
- **Regular expressions.** `/ab/.test(s)` does not work; `RegExp` is a stub.
- **Recursion depth is capped at 900 frames.** The interpreter recurses on the
  host C stack (measured: it dies near 1350 frames), so the cap converts a
  segfault into `RangeError: Maximum call stack size exceeded`.
- **Fixed-capacity pools.** 65536 scope slots / 8192 arrays / 65536 array
  elements. Exhausting one reports the same `RangeError` instead of panicking.
- No modules at runtime beyond simple `import`/`export`, no labelled
  `continue`, no `Symbol` beyond `typeof`, no getters on prototypes, no
  `WeakMap` semantics (it is an object stub).

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
return, which is what makes mutable capture work.

Untaken branches are still *parsed* (the interpreter must advance `pos`) but a
`noeval` depth counter suppresses every state mutation and all output while
inside them — that is how short-circuiting works without an AST.

## Wyn compiler bugs found and worked around

Reviving this exposed six compiler bugs. None are patched here; each is worked
around in the interpreter with a comment at the site, and each has a minimal
repro. The workarounds are the reason `sdup()` appears throughout: it forces a
fresh string allocation to defeat a premature release.

1. **Storing a string parameter into a global array element loses it.**
   `fn set(v: string) { arr[0] = v }` emits `{ const char* t = mk(); set(t);
   wyn_rc_release(t); }` — the callee stored the pointer, the caller freed it.
   Workaround: `arr[0] = v + ""`.
2. **A local aliasing a global string is released at function exit, freeing the
   global.** `var s1 = src; ... ; src = s1` emits `wyn_rc_release(s1)` before
   returning, leaving `src` dangling. Broke every second call of any JS function
   (the stored body was freed). Workaround: copy on both save and restore.
3. **A `HashMap` bound inside a loop or `if` body gets a spurious
   `hashmap_free`.** `while c >= 0 { var m = maps[c]; ... }` frees the map the
   global array still owns — use-after-free, segfault on the next lookup.
   Binding at function-top scope does not. Workaround: index inline, or read
   through a top-level-scoped accessor.
4. **`return [a, b]` of two local strings yields two empty elements.**
   Workaround: build the array with `push()`.
5. **`for i in 0..IDENT { }` misparses** as a struct literal ("Expected ':'
   after field name"). Workaround: `for i in 0..(IDENT)`.
6. **`fn str_to_int` collides with a `wyn_runtime.h` symbol** and fails the C
   compile with "redefinition". Workaround: pick another name.

## Interpreter bugs fixed

For the record, since the survey reported this as "builds-but-wrong-output" —
it produced *zero* output on its own test suite. Beyond the six workarounds
above: `console.log` emitted a doubled newline; the ternary eagerly evaluated
*both* arms (so every recursive function recursed forever); `&&`/`||`/`??` did
not short-circuit; `switch` with a `return` in a non-final case looped forever;
a top-level `throw` followed by more statements hung; a class method named `get`
was parsed as an accessor and skipped; `parseInt("42px")` and `vtonum("-1.0")`
panicked; `Math.ceil` and `Math.round` both called `floor`; `.toFixed`
truncated instead of rounding; `[1,2] + ""` gave `"[array]"`; `Promise` methods
panicked on a missing internal slot; string spread and every braceless loop body
silently did nothing.
