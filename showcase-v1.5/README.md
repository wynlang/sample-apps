# Wyn v1.5.0 Feature Showcase

**The most comprehensive demonstration of Wyn's modern type system and elegant syntax.**

## ⚠️ Known Syntax Limitations in v1.5.0

v1.5.0 has verbose syntax that is improved in v1.6.0:

**v1.5.0 syntax:**
```wyn
var result = Result_Ok(42)           // Underscore required
var map: HashMap<string, int> = {}   // Type annotation required
match result {
    Result_Ok(x) => x,               // Full enum name required
    Result_None() => 0
}
```

**v1.6.0 improvements:**
```wyn
var result = Result::Ok(42)          // Namespace operator
var map = HashMap<string, int>{}     // Type in initializer (planned)
match result {
    Result::Ok(x) => x,              // Namespace operator
    Result::None() => 0
}
```

✅ **Namespace operator (::) available in v1.6.0!**

---

## Features Demonstrated

### 1. **Enums with Associated Data** 🎯
```wyn
enum Result {
    Ok(int),
    Err(string)
}

enum Option {
    Some(int),
    None
}

enum Status {
    Pending,
    Processing,
    Complete(int),
    Failed(string)
}
```

- Tagged unions with type-safe data
- Mixed variants (with and without data)
- Auto-generated constructors

### 2. **Pattern Matching with Destructuring** 🔍
```wyn
var value = match result {
    Result_Ok(score) => {
        print("Success!")
        score
    },
    Result_Err(msg) => {
        print(msg)
        0
    }
}
```

- Exhaustive pattern matching
- Value extraction from enum variants
- Block expressions in match arms

### 3. **Generic Collections** 📦
```wyn
fn create_scores() -> HashMap<string, int> {
    var scores: HashMap<string, int> = {}
    scores.insert("alice", 95)
    return scores
}
```

- Generic type syntax: `HashMap<K, V>`
- Type inference through function returns
- Beautiful collection APIs

### 4. **HashMap with Variable Keys** 🗝️
```wyn
var key = "bob"
scores[key] = 95  // Variable key assignment!
var value = scores[key]
```

- Index syntax with variables
- Type-safe key/value access
- Ergonomic collection manipulation

### 5. **Option Type Methods** ✨
```wyn
if opt.is_some() {
    return opt.unwrap()
}
return opt.unwrap_or(0)
```

- Safe null handling
- `is_some()`, `is_none()`
- `unwrap()`, `unwrap_or()`

### 6. **Result Type Methods** ✅
```wyn
if result.is_ok() {
    return result.unwrap()
}
return result.unwrap_or(0)
```

- Elegant error handling
- `is_ok()`, `is_err()`
- No exceptions needed

### 7. **String Indexing** 📝
```wyn
var text = "Hello"
var first = text[0]  // "H"
var last = text[4]   // "o"
```

- Array-style string access
- Returns single-character strings
- Clean, intuitive syntax

### 8. **Array Transformations** 🔄
```wyn
var doubled = nums.map(fn(x: int) -> int {
    return x * 2
})

var total = nums.reduce(0, fn(acc: int, x: int) -> int {
    return acc + x
})
```

- Functional programming patterns
- Higher-order functions
- Trailing commas supported

### 9. **Bool Methods** 🔢
```wyn
var flag = true
print(flag.to_string())  // "true"
print(flag.to_int())     // 1
```

- Type conversions
- Consistent API across types

### 10. **Type Inference** 🧠
```wyn
fn create() -> HashMap<string, int> { ... }

var map = create()  // Type inferred!
map.insert("key", 42)  // Methods work!
```

- Automatic type propagation
- No redundant annotations
- Full type safety maintained

## Running the Showcase

```bash
cd wyn
./wyn ../sample-apps/showcase-v1.5/main.wyn
../sample-apps/showcase-v1.5/main.wyn.out
```

## Expected Output

```
=== Wyn v1.5.0 Feature Showcase ===

1. Creating user scores (HashMap<string, int>)...
   Users: alice, bob, charlie

2. Analyzing scores with Result type...
Success! Average score: 91

3. Looking up individual scores (Option type)...
   Alice's score: 95
   Unknown user score: 0

4. Checking task status (mixed enum variants)...
   Status 1: Finished successfully
   Failure reason: Network timeout
   Status 2: Task failed

5. Processing text with string indexing...
First character: H
   Word count: 3

6. Transforming scores with map/reduce...
Total with bonus: 458

7. Bool methods demonstration...
   Is passing? true
   As int: 1

8. Variable keys in HashMap...
   Updated Bob's score to: 95

=== All v1.5.0 features working! ===
```

## Why This Matters

This showcase demonstrates that Wyn combines:

- **Modern type system** (generics, enums, pattern matching)
- **Functional features** (type inference, tagged unions, functional programming)
- **Production-ready** (type-safe, no runtime errors, predictable behavior)

**Note:** Some syntax is verbose in v1.5.0 (enum constructors use underscores, HashMap needs type annotations). This is a known limitation being addressed in v1.6.0. The type system itself is solid and production-ready.

## v1.6.0 Syntax Improvements

Available now in v1.6.0:

1. **Namespace syntax for enums:**
   ```wyn
   // v1.5.0
   var result = Result_Ok(42)
   
   // v1.6.0 ✅
   var result = Result::Ok(42)
   ```

2. **Enhanced error messages:**
   - Color-coded output
   - Detailed type information
   - Conversion suggestions

3. **More string methods:**
   - `string.replace(old, new)`
   - `string.pad_left(width, char)`
   - `string.pad_right(width, char)`

Planned for future v1.6.x releases:

4. **Type inference for HashMap:**
   ```wyn
   // Current
   var map: HashMap<string, int> = {}
   
   // Planned
   var map = HashMap<string, int>{}
   ```

5. **Short constructor names in patterns:**
   ```wyn
   // Current
   match result {
       Result::Ok(x) => x,
       Result::Err(e) => 0
   }
   
   // Planned
   match result {
       Ok(x) => x,
       Err(e) => 0
   }
   ```

These improvements make the syntax cleaner while maintaining the powerful type system.

## Complexity Hidden Behind Simplicity

Under the hood, this code generates:
- Tagged union structs with discriminated unions
- Type-safe enum constructors
- Generic collection implementations
- Pattern matching with exhaustive checks
- Zero-cost abstractions

But you write code that looks like Python or TypeScript!

## v1.5.0 Achievement

This release proves Wyn is ready for:
- ✅ Real-world applications
- ✅ Complex data structures
- ✅ Type-safe error handling
- ✅ Functional programming patterns
- ✅ Self-hosting compiler development

**Wyn v1.5.0: Where elegance meets power.** 🚀
