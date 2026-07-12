# Gware v0.0.0.9 Release Notes

We are thrilled to announce the release of Gware v0.0.0.9, an update that brings major improvements to the standard library, language infrastructure, and the overall developer experience. This release solidifies Gware's tooling ecosystem, making it a robust platform for both backend scripting and modern web UI development.

## 🚀 Key Features

### Standard Library & Engine

* **Garbage Collection (Mark-and-Sweep)**: Implemented an automated Mark-and-Sweep Garbage Collector (GC) directly within the VM. Memory is now tracked during allocations, and unreferenced objects are automatically reclaimed when thresholds are reached, eliminating manual memory management overhead.
* **Abstract Syntax Tree (AST) Refactoring**: Thoroughly decoupled and stabilized the parser nodes, ensuring safe generation and execution of syntax elements like function calls (`AST_CALL_EXPRESSION`), assignments (`AST_SET_STATEMENT`), and logical structures. Fixed deep underlying issues with block execution scoping.

### Developer Experience & Tooling

* **Built-in Formatter (`gware fmt`)**: The Gware language now comes with a zero-config built-in code formatter! Run `gware fmt <script.gw>` to automatically format your source files to standard 4-space indents and K&R bracing conventions.
* **Native Testing Framework (`gware test`)**: Writing tests in Gware is easier than ever. The new `gware test <dir>` command automatically scans for `*.test.gw` files and executes them safely using isolated `jmp_stack` environments. An `assert(condition)` function is now globally available!
* **Packager (`gware build`)**: You can now compile and distribute your Gware applications as a single, self-contained binary! `gware build` bundles your scripts into a custom Virtual File System (VFS) and appends it natively to the `gware.exe` executable payload.
* **VS Code Extension**: Gware now has an official Visual Studio Code Extension! It provides rich syntax highlighting for both `.gw` and `.gweb` files (including variables, operators, UI components, and string interpolation). A `.vsix` package is included and ready for manual installation.

### Web & Scaffolding

* **Hot Module Replacement (HMR)**: Live-reload has been integrated for GwareWeb (`.gweb`) compilation targets. Your UI automatically refreshes in the browser when `gware` detects a source code change.
* **WASM Target Support**: Expanded transpiler capabilities to align with forthcoming WASM deployments for the Gware runtime.
* **CLI Scaffold (`gware create`)**: Scaffolding new Gware projects and apps can now be done out-of-the-box using the new generator tools.
* **Error Stack Traces**: Re-engineered error handling to provide complete execution stack traces instead of raw aborts, allowing for much smoother debugging loops.

## 🛠️ Installation
* Gware binaries and standard library components can be updated directly from the source repository.
* The VS Code Extension can be installed manually using the packaged `gware-0.0.1.vsix` file located in the `vscode-gware/` directory.

Happy Coding!
