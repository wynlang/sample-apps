# WynJS

A JavaScript runtime written in Wyn. 2,322 lines, 157KB binary, 115 tests.

## Project Structure

```
wynjs/
├── wyn.toml           # Project config
├── src/
│   └── main.wyn       # JS runtime (lexer + parser + evaluator)
├── tests/
│   ├── test_main.wyn   # Wyn test runner
│   └── test.js         # JS test suite (115 tests)
└── README.md
```

## Usage

```sh
# Run directly
cd wyn && wyn run ../sample-apps/wynjs/src/main.wyn -- hello.js

# Build binary
cd wyn && wyn build ../sample-apps/wynjs/src/main.wyn -o wynjs
./wynjs hello.js
```

## Testing

```sh
# Run JS test suite via Wyn test runner
cd sample-apps/wynjs && ../../wyn/wyn run tests/test_main.wyn

# Or run JS tests directly
cd wyn && wyn run ../sample-apps/wynjs/src/main.wyn -- ../sample-apps/wynjs/tests/test.js
```

## Architecture

Single-pass tree-walk interpreter: lexer → recursive descent parser → immediate evaluation.

Values are tagged strings (`n:42`, `s:hello`, `b:true`, `o:0`, `a:0`).
Objects use `[HashMap]` arrays — each JS object is a Wyn HashMap.
Scopes use flat arrays with stack marks for push/pop.
Function bodies stored as source strings, re-parsed on call.
Closures capture and restore environment snapshots.
Prototype chains for class inheritance.

## Features — 115/115 tests pass

### Language
let/const/var, if/else, while, for, do-while, switch/case, for-in, for-of,
function declarations, arrow functions, ES6 classes with extends/super,
try/catch/finally/throw, destructuring, spread, template literals,
closures (mutable capture), import/export

### Operators
`+` `-` `*` `/` `%` `**` `===` `!==` `<` `>` `<=` `>=` `&&` `||` `!`
`? :` `??` `?.` `+=` `-=` `*=` `/=` `%=` `++` `--` `typeof` `instanceof` `delete` `void`

### Built-in Objects
Math, JSON, Object, console, Map, Set, Error, RegExp, Promise (sync),
parseInt, parseFloat, Number, String, Boolean, isNaN, Array.isArray, Array.from

### String Methods (20)
toUpperCase, toLowerCase, trim, indexOf, includes, startsWith, endsWith,
slice, split, replace, replaceAll, repeat, padStart, padEnd, charAt, concat, length

### Array Methods (20)
map, filter, reduce, find, findIndex, some, every, forEach, sort, reverse,
indexOf, includes, join, concat, slice, splice, push, pop, shift, unshift

## Wyn Compiler Bugs Found

Building WynJS exposed and fixed 11 Wyn compiler bugs:

1. Array literal codegen — `array_push_str` for string arrays
2. `wyn run` CLI args — args after `--` now passed to program
3. `wyn build` TCC path — MAP_ANONYMOUS + missing source files
4. `test` keyword — now context-sensitive
5. HashMap.keys()/values() — now return `[string]`
6. Global forward references — script mode globals at file scope
7. Global variable type inference — method call results get correct types
8. Unified build source file list — single source of truth
9. HashMaps in arrays — `array.push()` updates element type
10. Module variable exports — `export var` now works
11. Module function codegen — imported functions use `#define` aliases
