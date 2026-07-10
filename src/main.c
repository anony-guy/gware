#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lexer.h"
#include "parser.h"
#include "eval.h"
#include "transpiler.h"
#include "updater.h"
#include "color.h"

#define GWARE_VERSION "v0.0.0.4"

void run_script(char* input, int isWeb) {
    Lexer* l = Lexer_create(input);
    Parser* p = Parser_create(l);
    
    ASTNode* ast = Parser_parseProgram(p);
    
    if (isWeb) {
        Transpile_to_html(ast, "index.html");
    } else {
        Environment* env = Environment_create(NULL);
        Eval_node(ast, env);
        Environment_destroy(env);
    }
    
    ASTNode_destroy(ast);
    Parser_destroy(p);
    Lexer_destroy(l);
}

int main(int argc, char** argv) {
    enableColors();
    if (argc < 2) {
        printf("Usage: gware.exe <script.gw>\n");
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
    
    if (strcmp(argv[1], "--web-version") == 0) {
        printf(ANSI_COLOR_CYAN "GwareWeb Transcompiler %s\n" ANSI_COLOR_RESET, GWARE_VERSION);
        return 0;
    }
    
    if (strcmp(argv[1], "--update") == 0) {
        check_and_apply_update(GWARE_VERSION);
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
    
    run_script(buffer, isWeb);
    free(buffer);
    
    return 0;
}
