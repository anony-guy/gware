# Gware Engine v0.0.0.1 (Initial Release)

Welcome to the very first release of the **Gware Engine** and **GwareWeb**! 

Gware is a lightning-fast, zero-dependency scripting language and web transcompiler written entirely in C. It is designed to be incredibly small, blazing fast, and powerful enough to build both desktop scripts and modern web UI components.

## 🌟 Highlights of this Release

### 1. Hybrid Type Safety
Gware gives you the ultimate flexibility of a dynamic scripting language with the iron-clad safety of a compiled one.
* **Dynamic Typing:** Use `set x = 10` for quick, flexible scripting.
* **Static Declarations:** Use `set int age = 30` or `set string name = "Gware"`.
* **Strong Runtime Constraints:** If you declare a variable as an `int`, the engine will lock that type in. Any attempt to reassign it to a string or perform invalid math will result in a loud, explicit Runtime Error.

### 2. GwareWeb: The Single-Language Transcompiler
Stop juggling HTML, CSS, and JavaScript. 
GwareWeb allows you to write `component`, `style`, `action`, and `view` blocks in a single `.gweb` file. When you compile it using the `--web` flag, Gware translates it into a flawless, standalone `index.html` file complete with **Reactive State Binding** that automatically updates the DOM when variables change.

### 3. Built-In GitHub Auto-Updater
Never fall behind on a release! Gware includes a native auto-updater that leverages built-in Windows APIs (via PowerShell) to poll this repository for the latest release.
Just run `gware.exe --update`, and Gware will dynamically download the new binary and hot-swap itself seamlessly.

---

## 🛠️ Usage & Flags

* **Run a script:** `gware.exe script.gw`
* **Transpile to Web:** `gware.exe --web ui.gweb`
* **Check Version:** `gware.exe --version`
* **Check Web Version:** `gware.exe --web-version`
* **Update Engine:** `gware.exe --update`

## 📦 Installation
Download `gware.exe` from the Assets below and drop it anywhere on your machine! No installations or runtimes required.
