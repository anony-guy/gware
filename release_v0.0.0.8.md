# Gware v0.0.0.8 Release Notes

Gware v0.0.0.8 brings two massive architectural additions to the engine, cementing it as a production-ready and fully native powerhouse.

### 1. Mark-and-Sweep Garbage Collector (GC)
We completely rewrote the internal memory allocation pipeline to feature a tracing Mark-and-Sweep Garbage Collector. 
- Replaced all explicit `Value_free()` and manual memory tracking with GC-backed linked lists.
- Dynamically traverses the AST evaluation environments (`firstObject`) during GC cycles, keeping memory usage clean and flat even on large Web requests or complex `.gweb` reactivity.
- Solved the persistent memory leak/segfault issues associated with cyclical objects and array freeing in a single shot!

### 2. Native WebSockets Engine
Gware can now act as a standalone zero-dependency real-time WebSocket backend.
- We implemented a **zero-dependency `SHA-1` hashing** algorithm and **`Base64` encoder** entirely in native C to manually satisfy the WebSocket handshake (`101 Switching Protocols`).
- Implemented **Frame Decoding/Encoding**: `tcp_api.c` can now strip XOR-masks from WS payloads and pack responses into RFC 6455 compliant TCP frames.
- Exposed `ws_send(socket_id, message)` directly to Gware scripts to send asynchronous data to any connected client!

### 3. Parameterized SQLite Security Fixes
We also audited the SQLite engine (`sqlite_api.c`) and ensured that `sqlite_exec` and `sqlite_query` actively leverage `sqlite3_bind_*` to inject arguments as parameters, completely shielding Gware backend scripts from SQL injection attacks.

### 4. WebAssembly (WASM) Compilation Efforts
We actively attempted building the engine via `zig cc -target wasm32-wasi`. While we made significant progress, `sqlite3`, TCP Sockets, and `setjmp.h` (for error handling) conflict with the strict WASI freestanding environment. We decided to defer WASM compilation to a future release in order to develop an explicit JavaScript interop abstraction layer for the WASM bridge.
