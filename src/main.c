#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lexer.h"
#include "parser.h"
#include "eval.h"
#include "transpiler.h"
#include "updater.h"
#include "color.h"
#ifndef GWARE_WASM
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif
#endif

#include "fs_api.h"
#include "test_api.h"
#include "packager.h"
#include "vfs.h"
#include "formatter.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#define GWARE_VERSION "v0.0.0.4"

void run_script(char* input, const char* filename, int isWeb) {
    Lexer* l = Lexer_create(input, filename);
    Parser* p = Parser_create(l);
    
    ASTNode* ast = Parser_parseProgram(p);
    
    if (isWeb) {
        Transpiler_transpileToWeb(ast, "index.html");
    } else {
        Environment* env = Environment_create(NULL);
        register_fs_api(env);
        Eval_node(ast, env);
        Environment_destroy(env);
    }
    
    ASTNode_destroy(ast);
    Parser_destroy(p);
    Lexer_destroy(l);
}

int main(int argc, char** argv) {
    enableColors();
    
    // Check for VFS bundle at end of executable
#ifdef _WIN32
    char exe_path[MAX_PATH];
    GetModuleFileName(NULL, exe_path, MAX_PATH);
#else
    char exe_path[1024];
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path)-1);
    if (len != -1) exe_path[len] = '\0';
    else strcpy(exe_path, argv[0]);
#endif

    FILE* exe_fp = fopen(exe_path, "rb");
    if (exe_fp) {
        fseek(exe_fp, -12, SEEK_END);
        char magic[9] = {0};
        fread(magic, 1, 8, exe_fp);
        if (strcmp(magic, "GWAREPKG") == 0) {
            int bundle_size = 0;
            fread(&bundle_size, 1, 4, exe_fp);
            fseek(exe_fp, -(12 + bundle_size), SEEK_END);
            char* bundle_data = malloc(bundle_size);
            fread(bundle_data, 1, bundle_size, exe_fp);
            fclose(exe_fp);
            
            vfs_init();
            vfs_load_bundle(bundle_data, bundle_size);
            free(bundle_data);
            
            char* main_src = vfs_get_file("main.gw");
            if (main_src) {
                run_script(main_src, "main.gw", 0);
                return 0;
            }
        } else {
            fclose(exe_fp);
        }
    }
    
    if (argc < 2) {
        printf("Usage: gware.exe <script.gw>\n");
        printf("       gware.exe build <script.gw>\n");
        printf("       gware.exe --web <script.gweb>\n");
        printf("       gware.exe --version\n");
        printf("       gware.exe --web-version\n");
        printf("       gware.exe --update\n");
        return 1;
    }

    if (strcmp(argv[1], "-v") == 0 || strcmp(argv[1], "--version") == 0) {
        printf(ANSI_COLOR_CYAN "Gware Engine %s\n" ANSI_COLOR_RESET, GWARE_VERSION);
        return 0;
    }
    
    if (strcmp(argv[1], "test") == 0) {
        run_tests();
        return 0;
    }
    
    if (strcmp(argv[1], "build") == 0) {
        if (argc < 3) {
            printf("Usage: gware build <main.gw>\n");
            return 1;
        }
        build_executable(argv[2]);
        return 0;
    }
    
    if (strcmp(argv[1], "fmt") == 0) {
        if (argc < 3) {
            printf("Usage: gware fmt <script.gw>\n");
            return 1;
        }
        format_file(argv[2]);
        return 0;
    }
    
    if (strcmp(argv[1], "--web-version") == 0) {
        printf(ANSI_COLOR_CYAN "GwareWeb Transcompiler %s\n" ANSI_COLOR_RESET, GWARE_VERSION);
        return 0;
    }
    
#ifndef GWARE_WASM
    if (strcmp(argv[1], "--update") == 0) {
        check_and_apply_update(GWARE_VERSION);
        return 0;
    }
#endif
    
    if (strcmp(argv[1], "init") == 0) {
#ifndef GWARE_WASM
#ifdef _WIN32
        _mkdir("public");
#else
        mkdir("public", 0777);
#endif
#endif
        FILE* cfg = fopen("gware.json", "w");
        if (cfg) {
            fputs("{\n  \"name\": \"gware-project\",\n  \"version\": \"1.0.0\",\n  \"main\": \"main.gw\"\n}\n", cfg);
            fclose(cfg);
        }
        FILE* mn = fopen("main.gw", "w");
        if (mn) {
            fputs("show(\"Hello from Gware!\")\n", mn);
            fclose(mn);
        }
        FILE* web = fopen("public/index.gweb", "w");
        if (web) {
            fputs("fn App() {\n    return <div>Hello GwareWeb!</div>\n}\nrender(<App />)\n", web);
            fclose(web);
        }
        printf(ANSI_COLOR_GREEN "Gware project initialized successfully!\n" ANSI_COLOR_RESET);
        return 0;
    }
    
    int isWeb = 0;
    char* filename = argv[1];
    
    if (strcmp(argv[1], "--web") == 0) {
        if (argc < 3) {
            printf(ANSI_COLOR_RED "Error: Expected filename after --web\n" ANSI_COLOR_RESET);
            return 1;
        }
        isWeb = 1;
        filename = argv[2];
    }
    
    FILE* file = fopen(filename, "rb");
    if (!file) {
        printf(ANSI_COLOR_RED "Error: Could not open file %s\n" ANSI_COLOR_RESET, filename);
        return 1;
    }
    
    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    char* buffer = (char*)malloc(length + 1);
    fread(buffer, 1, length, file);
    buffer[length] = '\0';
    fclose(file);
    
    run_script(buffer, filename, isWeb);
    free(buffer);
    
    return 0;
}
