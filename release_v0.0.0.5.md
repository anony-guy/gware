# Gware v0.0.0.5 Release Notes

## Summary of Changes
In this major release, we transformed Gware from a simple interpreted language into a full engine with native database and server capabilities, alongside reactive web capabilities!

### 1. Engine Extensibility (`invokeFunction`)
- Refactored `AST_FUNCTION_CALL` to use a global `invokeFunction(funcVal, args)`. This allows C code to execute Gware closures and pass arguments dynamically!
- Added `get_global_env()` so native APIs can read and register functions into the global state.

### 2. Native SQLite Engine
- Bundled the `sqlite3.c` amalgamation directly into Gware!
- Built `sqlite_api.c` exposing `sqlite_open`, `sqlite_close`, `sqlite_exec`, and `sqlite_query`.
- Database queries return a `VAL_ARRAY` of `VAL_OBJECT`s, mapping rows directly into Gware's native JSON structure!
- Successfully tested creating a table, inserting JSON data, and fetching it natively.

### 3. Native TCP Server 
- Linked `Winsock2` (`-lws2_32`).
- Implemented `tcp_listen(port, callback)` in `tcp_api.c`.
- Gware can now act as an Express.js-style web server! It listens on a socket and dynamically invokes your Gware callback on requests.

### 4. GwareWeb Reactivity (`bind`)
- Added two-way input binding! `input(bind: var_name)` generates highly efficient DOM synchronization logic in vanilla JavaScript without a Virtual DOM.
- Fixed lexer constraints to allow digits in tag names (like `h1`), ensuring HTML tags compile without syntax errors!

### 5. GwareWeb Encapsulation Engine (Dynamic Props)
- Modified `transpiler.c` to generate unique `instanceId` tags for Uppercase components (e.g. `<Counter>`).
- State variables and Actions are dynamically mangled per instance in the resulting JavaScript (e.g., `Counter_1_clicks`), allowing you to render the same component multiple times on a page without memory leaks or state collision!
- Props can now be passed via attributes (`<Counter clicks="10">`), which are seamlessly transpiled into state initializations.

### 6. GwareWeb List Rendering (`for` loops)
- Upgraded `parser.c` to support array literals (`[1, 2, 3]`).
- Added a `for(in: items, as: item)` UI element. The transpiler converts its body AST directly into a JavaScript Template String (`html += \`<li>${item}</li>\``) and executes it inside `updateDOM()`. This achieves dynamic reactivity for arrays like SQLite query results without needing a Virtual DOM!

## Validation Results
- `sqlite_test.gw`: Successfully queried DB and printed objects.
- `tcp_test.gw`: Verified curl receives `HTTP/1.1 200 OK` from Gware script.
- `bind_test.gweb`: `index.html` generates proper `oninput` JS handlers for reactive inputs!
- `multi_counter.gweb`: Rendered multiple `<Counter>` components on the same page with isolated reactive states and props!
- `list_test.gweb`: Successfully rendered an array of strings into a `<ul>` loop using the new `for` UI tag!
