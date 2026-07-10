# Gware Engine v0.0.0.2 (Language Expansion)

Welcome to Gware Engine v0.0.0.2! This release transforms Gware from a simple scripting evaluator into a fully-fledged, capable programming language by introducing crucial data structures and custom logic blocks.

## What's New?

### 1. Custom Functions (`def`)
You can now encapsulate your logic into reusable functions using the `def` keyword. Variables within functions have proper lexical scoping, preventing conflicts with the global environment.
```text
def calculate_area(int width, int height) {
    set result = width * height
    return result
}
```

### 2. Arrays
Gware now supports dynamic, mixed-type arrays (unless strictly typed) with zero-based indexing.
```text
set my_arr = [10, 20, "thirty"]
show(my_arr[0])
set my_arr[1] = 99
```
*Note: String characters can also be accessed via indexing!*

### 3. Native File I/O
Interact directly with the file system natively.
```text
set content = read_file("config.txt")
write_file("output.txt", "Execution successful!")
```

## How to Update
Run Gware with the `--update` flag to fetch the latest binary from GitHub!
```powershell
gware --update
```
