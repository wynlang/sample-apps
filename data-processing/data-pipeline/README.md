# Data Pipeline - Wyn v1.4.0 Feature Showcase

A comprehensive demonstration of all Wyn v1.4.0 features in a real-world data processing pipeline.

## Features Demonstrated

### 1. Functional Programming
- `.map()` - Transform array elements
- `.filter()` - Select matching elements
- `.reduce()` - Aggregate values
- Higher-order functions with function parameters

### 2. Async/Await
- `async fn` - Asynchronous functions
- `await` - Non-blocking operations
- Future-based execution

### 3. String Methods (40+)
- `.trim()`, `.upper()`, `.lower()`
- `.contains()`, `.lines()`, `.parse_int()`
- Character classification and manipulation

### 4. Array Methods (21+)
- Basic: `.len()`, `.first()`, `.last()`
- Functional: `.map()`, `.filter()`, `.reduce()`
- Aggregation: `.sum()`, `.min()`, `.max()`

### 5. File I/O
- `File::read()`, `File::write()`
- `File::exists()`, `File::delete()`
- Path operations

### 6. Integer & Float Methods
- `.to_binary()`, `.to_hex()`
- `.is_even()`, `.is_odd()`
- `.round()`, `.floor()`, `.ceil()`

### 7. System Operations
- `System::args()` - Command-line arguments
- `File::get_cwd()` - Current directory
- `Time::now()` - Timestamps

## Usage

```bash
cd sample-apps/showcase/data-pipeline
../../../wyn/wyn main.wyn
./main.wyn.out
```

## Code Highlights

**Functional Pipeline:**
```wyn
var result = numbers
    .filter(is_valid)
    .map(square)
    .reduce(sum, 0);
```

**Higher-Order Functions:**
```wyn
fn apply_pipeline(arr: array, transform: fn(int) -> int) -> array {
    return arr.map(transform);
}
```

**Async Operations:**
```wyn
async fn process_async(data: int) -> int {
    Time::sleep(1);
    return data * 2;
}

var result = await process_async(50);
```

## Output

The app demonstrates:
- String processing and manipulation
- Functional array transformations
- File I/O operations
- Async/await execution
- Type conversions
- System information

All features work together seamlessly in a real-world scenario.
