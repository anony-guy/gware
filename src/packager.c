#include "packager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"
#include "lexer.h"
#include "parser.h"
#include "color.h"

#ifdef _WIN32
#include <windows.h>
#endif

typedef struct BundledFile {
    char* filename;
    char* content;
    int size;
    struct BundledFile* next;
} BundledFile;

static BundledFile* bundle_head = NULL;

static int is_bundled(const char* filename) {
    BundledFile* curr = bundle_head;
    while (curr) {
        if (strcmp(curr->filename, filename) == 0) return 1;
        curr = curr->next;
    }
    return 0;
}

static void bundle_file(const char* filename, const char* content, int size) {
    if (is_bundled(filename)) return;
    BundledFile* file = (BundledFile*)malloc(sizeof(BundledFile));
    file->filename = strdup(filename);
    file->content = (char*)malloc(size + 1);
    memcpy(file->content, content, size);
    file->content[size] = '\0';
    file->size = size;
    file->next = bundle_head;
    bundle_head = file;
}

static void scan_ast_for_imports(ASTNode* node) {
    if (!node) return;
    
    if (node->type == AST_IMPORT_STATEMENT && node->value) {
        if (!is_bundled(node->value)) {
            // Load and parse
            FILE* fp = fopen(node->value, "rb");
            if (!fp) {
                printf(ANSI_COLOR_RED "Error: Packager could not find imported file '%s'\n" ANSI_COLOR_RESET, node->value);
                exit(1);
            }
            fseek(fp, 0, SEEK_END);
            long size = ftell(fp);
            fseek(fp, 0, SEEK_SET);
            char* buf = malloc(size + 1);
            fread(buf, 1, size, fp);
            buf[size] = '\0';
            fclose(fp);
            
            bundle_file(node->value, buf, size);
            
            Lexer* l = Lexer_create(buf, node->value);
            Parser* p = Parser_create(l);
            ASTNode* ast = Parser_parseProgram(p);
            if (ast) scan_ast_for_imports(ast);
            ASTNode_destroy(ast);
            Parser_destroy(p);
            Lexer_destroy(l);
            free(buf);
        }
    }
    
    scan_ast_for_imports(node->left);
    scan_ast_for_imports(node->right);
    
    for (int i=0; i<node->statementCount; i++) scan_ast_for_imports(node->statements[i]);
    for (int i=0; i<node->parameterCount; i++) scan_ast_for_imports(node->parameters[i]);
}

void build_executable(const char* main_file) {
    printf(ANSI_COLOR_CYAN "Packaging %s...\n" ANSI_COLOR_RESET, main_file);
    
    FILE* fp = fopen(main_file, "rb");
    if (!fp) {
        printf(ANSI_COLOR_RED "Error: Could not open main script %s\n" ANSI_COLOR_RESET, main_file);
        return;
    }
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    char* buf = malloc(size + 1);
    fread(buf, 1, size, fp);
    buf[size] = '\0';
    fclose(fp);
    
    bundle_file("main.gw", buf, size);
    
    Lexer* l = Lexer_create(buf, "main.gw");
    Parser* p = Parser_create(l);
    ASTNode* ast = Parser_parseProgram(p);
    if (ast) scan_ast_for_imports(ast);
    ASTNode_destroy(ast);
    Parser_destroy(p);
    Lexer_destroy(l);
    free(buf);
    
    // Calculate total bundle size
    int total_bundle_size = 0;
    BundledFile* curr = bundle_head;
    while (curr) {
        total_bundle_size += 4 + strlen(curr->filename) + 4 + curr->size;
        curr = curr->next;
    }
    
    char* bundle_data = malloc(total_bundle_size);
    int offset = 0;
    curr = bundle_head;
    while (curr) {
        int fn_len = strlen(curr->filename);
        memcpy(bundle_data + offset, &fn_len, 4); offset += 4;
        memcpy(bundle_data + offset, curr->filename, fn_len); offset += fn_len;
        
        memcpy(bundle_data + offset, &curr->size, 4); offset += 4;
        memcpy(bundle_data + offset, curr->content, curr->size); offset += curr->size;
        
        curr = curr->next;
    }
    
#ifdef _WIN32
    const char* out_file = "app.exe";
#else
    const char* out_file = "app";
#endif
    
#ifdef _WIN32
    char exe_path[MAX_PATH];
    GetModuleFileName(NULL, exe_path, MAX_PATH);
#else
    char exe_path[1024];
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path)-1);
    if (len != -1) exe_path[len] = '\0';
    else strcpy(exe_path, "gware");
#endif
    
    FILE* src = fopen(exe_path, "rb");
    FILE* dst = fopen(out_file, "wb");
    
    if (!src || !dst) {
        printf(ANSI_COLOR_RED "Error: Could not copy executable.\n" ANSI_COLOR_RESET);
        if (src) fclose(src);
        if (dst) fclose(dst);
        free(bundle_data);
        return;
    }
    
    char copy_buf[8192];
    size_t bytes;
    while ((bytes = fread(copy_buf, 1, sizeof(copy_buf), src)) > 0) {
        fwrite(copy_buf, 1, bytes, dst);
    }
    fclose(src);
    
    // Append bundle
    fwrite(bundle_data, 1, total_bundle_size, dst);
    
    // Write 8-byte magic marker and 4-byte size
    const char* magic = "GWAREPKG";
    fwrite(magic, 1, 8, dst);
    fwrite(&total_bundle_size, 1, 4, dst);
    
    fclose(dst);
    free(bundle_data);
    
    printf(ANSI_COLOR_GREEN "Successfully packaged into %s!\n" ANSI_COLOR_RESET, out_file);
}
