# Gware Engine v0.0.0.3 (Networking & Error Handling)

Welcome to Gware Engine v0.0.0.3! We are bringing Gware to the modern web with native fetch capabilities and rock-solid error handling.

## What's New?

### 1. Robust Error Catching (`try`/`catch`)
Previously, runtime errors (like indexing out of bounds or dividing by zero) would cause Gware to crash entirely. Now, these failures can be gracefully handled using `try` and `catch` blocks!
```text
try {
    set result = 10 / 0
} catch (err) {
    show("Whoops! Error:")
    show(err)
}
```

### 2. Native Network Capabilities (`fetch`)
Interact with external APIs seamlessly using the new native `fetch` command, which issues an HTTP GET request to a URL and returns the response string.
```text
try {
    set weather = fetch("http://myweatherapi.com")
    show(weather)
} catch (err) {
    show("Could not get weather...")
}
```

## How to Update
Run Gware with the `--update` flag to fetch the latest binary from GitHub!
```powershell
gware --update
```
