#include "test_api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include "color.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#endif

extern void run_script(char* input, const char* filename, int isWeb);
extern jmp_buf jmp_stack[64];
extern int jmp_stack_ptr;
extern char last_error_msg[1024];

static void run_test_file(const char* filename, int* passed, int* failed) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        printf(ANSI_COLOR_RED "[FAIL] %s: Could not open file\n" ANSI_COLOR_RESET, filename);
        (*failed)++;
        return;
    }
    
    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    char* buffer = (char*)malloc(length + 1);
    fread(buffer, 1, length, file);
    buffer[length] = '\0';
    fclose(file);
    
    jmp_stack_ptr++;
    if (setjmp(jmp_stack[jmp_stack_ptr]) == 0) {
        run_script(buffer, filename, 0);
        printf(ANSI_COLOR_GREEN "[PASS] %s\n" ANSI_COLOR_RESET, filename);
        (*passed)++;
    } else {
        printf(ANSI_COLOR_RED "[FAIL] %s: %s\n" ANSI_COLOR_RESET, filename, last_error_msg);
        (*failed)++;
    }
    jmp_stack_ptr--;
    
    free(buffer);
}

void run_tests() {
    int passed = 0;
    int failed = 0;
    
    printf(ANSI_COLOR_CYAN "Running Gware Test Suite...\n" ANSI_COLOR_RESET);
    
#ifdef _WIN32
    WIN32_FIND_DATA fdFile;
    HANDLE hFind = NULL;
    char sPath[2048];
    sprintf(sPath, "*_test.gw");
    
    if ((hFind = FindFirstFile(sPath, &fdFile)) == INVALID_HANDLE_VALUE) {
        printf("No test files found.\n");
        return;
    }
    
    do {
        if (strcmp(fdFile.cFileName, ".") != 0 && strcmp(fdFile.cFileName, "..") != 0) {
            run_test_file(fdFile.cFileName, &passed, &failed);
        }
    } while (FindNextFile(hFind, &fdFile));
    FindClose(hFind);
#else
    DIR *d;
    struct dirent *dir;
    d = opendir(".");
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            int len = strlen(dir->d_name);
            if (len > 8 && strcmp(dir->d_name + len - 8, "_test.gw") == 0) {
                run_test_file(dir->d_name, &passed, &failed);
            }
        }
        closedir(d);
    }
#endif
    
    printf("\nTest Summary:\n");
    if (failed > 0) {
        printf(ANSI_COLOR_RED "Failed: %d" ANSI_COLOR_RESET "\n", failed);
    }
    printf(ANSI_COLOR_GREEN "Passed: %d" ANSI_COLOR_RESET "\n", passed);
    printf("Total:  %d\n", passed + failed);
}
