# Calculator with Modules

Demonstrates Wyn's module system with a simple calculator.

## Features

- **Module imports** - `import calculator`
- **Namespaced calls** - `calculator::add(5, 3)`
- **Multiple modules** - Import and use multiple modules
- **Type-safe** - Full type checking across modules

## Files

- `calculator.wyn` - Calculator module with math operations
- `main.wyn` - Main program that imports and uses the module

## Usage

```bash
cd wyn
./wyn ../sample-apps/calculator-modules/main.wyn
../sample-apps/calculator-modules/main.wyn.out
```

## Output

```
=== CALCULATOR WITH MODULES ===
Demonstrates: Module System (import)

15 + 3 = 18
15 * 3 = 45
5² = 25

=== MODULE SYSTEM WORKING! ===
```

## Module System

Wyn's module system allows you to:

1. **Create modules** - Any `.wyn` file can be a module
2. **Import modules** - `import module_name`
3. **Use functions** - `module_name::function()`
4. **Type safety** - Full type checking across module boundaries

### Creating a Module

```wyn
// calculator.wyn
fn add(a: int, b: int) -> int {
    return a + b;
}
```

### Using a Module

```wyn
// main.wyn
import calculator

fn main() -> int {
    var result = calculator::add(5, 3);
    print("Result: ${result}");
    return 0;
}
```

## Requirements

- **Type annotations required** - Module functions must have type annotations
- **Same directory** - Modules must be in the same directory as the main file
- **Int types only** - Currently only `int` and `bool` types work reliably in modules

## See Also

- [Wyn Language Guide](../../wyn/docs/language-guide.md)
- [Other Sample Apps](../README.md)
