@echo off
IF "%1"=="wasm" (
    f:\Coding\zig\zig-x86_64-windows-0.17.0-dev.1282+c0f9b51d8\zig.exe cc -target wasm32-wasi -DGWARE_WASM src/main.c src/lexer.c src/parser.c src/ast.c src/eval.c src/color.c src/cJSON.c src/json_api.c src/transpiler.c -o gware.wasm
    IF %ERRORLEVEL% NEQ 0 (
        echo WASM Build failed!
        exit /b %ERRORLEVEL%
    )
    echo Successfully compiled to gware.wasm
    exit /b 0
)

f:\Coding\zig\zig-x86_64-windows-0.17.0-dev.1282+c0f9b51d8\zig.exe cc -o gware.exe src\main.c src\lexer.c src\parser.c src\ast.c src\eval.c src\transpiler.c src\updater.c src\color.c src\net.c src\cJSON.c src\json_api.c src\sqlite3.c src\sqlite_api.c src\tcp_api.c src\crypto_api.c src\fs_api.c src\test_api.c src\packager.c src\vfs.c src\formatter.c -lwininet -lws2_32 -ladvapi32 -lcrypt32 -lbcrypt
IF %ERRORLEVEL% NEQ 0 (
    echo Build failed!
    exit /b %ERRORLEVEL%
)

if not exist public mkdir public
.\gware.exe --web counter.gweb
move index.html public\index.html >nul 2>&1
