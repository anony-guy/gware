# Gware v0.0.0.7 Release Notes

Welcome to Gware v0.0.0.7! This release massively upgrades GwareWeb by turning it into a fully-fledged Single Page Application (SPA) framework. We have addressed four major limitations and bridged the gap between Gware and modern Javascript frameworks!

## What's New?

### 1. Client-Side Routing (Native SPA)
GwareWeb now has native support for client-side routing. You can declare routes globally using the new `router` block, and Gware will automatically handle `window.history` and prevent page reloads for links!
```gware
router {
    route("/", Home)
    route("/about", About)
}
```

### 2. Native Client-Side Fetching
Gware frontend actions are now `async` by default! You can seamlessly call the built-in `fetch("url")` directly inside your `.gweb` files. It automatically handles async `await` and parsing the JSON response natively!
```gware
action loadData {
    set AppState.data = fetch("https://api.example.com/data")
}
```

### 3. JavaScript Interop (The Escape Hatch)
We have introduced a powerful `js("...")` escape hatch that allows you to evaluate raw JavaScript code whenever Gware's syntax doesn't support a specific browser API (like `window.localStorage` or Canvas APIs).
```gware
action saveToStorage {
    js("localStorage.setItem('user', AppState['user']);")
}
```

### 4. Efficient Virtual-DOM Diffing
We have replaced the old `.innerHTML = html` array rendering engine with a tiny, lightning-fast DOM diffing algorithm injected directly into the transpiler!
- Lists of 10,000+ items now update smoothly without lag.
- Input elements will no longer lose focus when arrays are re-rendered.
- Updates are constrained only to modified text and attribute nodes!

## Bug Fixes
- `TOKEN_LPAREN` is properly configured with `PREC_CALL` so function calls inside expressions are natively parsed without conflict.
- Nested attribute values and `store` variable binding now properly support dot notation!
