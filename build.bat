@echo off
f:\Coding\zig\zig-x86_64-windows-0.17.0-dev.1282+c0f9b51d8\zig.exe cc -o gware.exe src\main.c src\lexer.c src\parser.c src\ast.c src\eval.c src\transpiler.c src\updater.c src\color.c src\net.c -lwininet
if %ERRORLEVEL% equ 0 (
    echo Build successful. Running counter.gweb...
    gware.exe --web counter.gweb
) else (
    echo Build failed!
)
