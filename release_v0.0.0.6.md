# Gware v0.0.0.6 Release Notes

In this major release, we are adding several advanced engine capabilities to make Gware a fully-fledged, production-ready, cross-platform microservice language and reactive frontend framework.

## 1. Parameterized SQLite Queries (Security Fix)
- Updated `native_sqlite_query` and `native_sqlite_exec` to accept a `VAL_ARRAY` of parameters for parameterized queries.
- Prevents SQL injection when building web servers in Gware!

## 2. Cross-Platform Portability (Linux/macOS Support)
- Rewrote the raw networking implementation using POSIX standard sockets for cross-platform support.
- `#ifdef _WIN32` used for `winsock2.h` compatibility on Windows. Gware can now be cross-compiled natively with `zig cc` for any OS!

## 3. Non-Blocking I/O
- Replaced the blocking `accept()` loop with a `select()` event loop in the TCP server.
- Handles concurrent web connections efficiently on a single thread.

## 4. Modules and Imports
- Added an `import "file.gw"` statement. 
- You can now separate logic into multiple files to maintain a clean codebase!

## 5. String Manipulation & Standard Library Expansion
- Expanded the Gware global environment with new string utilities:
  - `string_split(str, delim)` -> `VAL_ARRAY`
  - `string_length(str)` -> `VAL_INT`
  - `string_replace(str, search, replace)` -> `VAL_STRING`

## 6. GwareWeb Conditional Rendering (`if` blocks in UI)
- You can now use `if` blocks directly inside a `view` declaration in a GwareWeb (`.gweb`) component!
- Toggling state variables in an action dynamically injects or removes the DOM elements conditionally, maintaining true reactivity without a Virtual DOM.
