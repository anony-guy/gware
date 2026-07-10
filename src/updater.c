#include "updater.h"
#include <stdio.h>
#include "color.h"
#include <stdlib.h>
#include <string.h>

void trim_whitespace(char* str) {
    int len = strlen(str);
    while (len > 0 && (str[len - 1] == '\n' || str[len - 1] == '\r' || str[len - 1] == ' ')) {
        str[len - 1] = '\0';
        len--;
    }
}

void check_and_apply_update(const char* current_version) {
    printf(ANSI_COLOR_YELLOW "Checking GitHub for updates...\n" ANSI_COLOR_RESET);
    
    // Fetch latest release tag via PowerShell
    FILE* fp = popen("powershell -noprofile -Command \"try { (Invoke-RestMethod https://api.github.com/repos/anony-guy/gware/releases/latest).tag_name } catch { echo 'NOT_FOUND' }\"", "r");
    if (!fp) {
        printf(ANSI_COLOR_RED "Error: Could not connect to GitHub.\n" ANSI_COLOR_RESET);
        return;
    }
    
    char latest_version[128] = {0};
    fgets(latest_version, sizeof(latest_version), fp);
    pclose(fp);
    trim_whitespace(latest_version);
    
    if (strlen(latest_version) == 0 || strcmp(latest_version, "NOT_FOUND") == 0) {
        printf(ANSI_COLOR_YELLOW "No releases found on the GitHub repository yet.\n" ANSI_COLOR_RESET);
        printf(ANSI_COLOR_YELLOW "Make sure to publish a Release on GitHub before attempting an update!\n" ANSI_COLOR_RESET);
        return;
    }
    
    if (strcmp(latest_version, current_version) == 0) {
        printf(ANSI_COLOR_GREEN "You are already on the latest version (%s). No update required!\n" ANSI_COLOR_RESET, current_version);
        return;
    }
    
    printf(ANSI_COLOR_CYAN "New version found: %s (Current: %s)\n" ANSI_COLOR_RESET, latest_version, current_version);
    printf(ANSI_COLOR_YELLOW "Downloading update...\n" ANSI_COLOR_RESET);
    
    // Download the new executable (assuming the asset is named gware.exe)
    char cmd[512];
    sprintf(cmd, "powershell -noprofile -Command \"Invoke-WebRequest -Uri https://github.com/anony-guy/gware/releases/download/%s/gware.exe -OutFile gware.exe.new\"", latest_version);
    
    int result = system(cmd);
    if (result != 0) {
        printf(ANSI_COLOR_RED "Error: Failed to download the update. Does the release contain a 'gware.exe' asset?\n" ANSI_COLOR_RESET);
        return;
    }
    
    printf(ANSI_COLOR_GREEN "Download complete! Applying update...\n" ANSI_COLOR_RESET);
    
    // Create the batch script for the hot-swap
    FILE* bat = fopen("update.bat", "w");
    if (!bat) {
        printf(ANSI_COLOR_RED "Error: Could not create update script.\n" ANSI_COLOR_RESET);
        return;
    }
    
    fprintf(bat, "@echo off\n");
    fprintf(bat, "echo Swapping executables...\n");
    // Wait for 1 second to ensure gware.exe has fully terminated and released its file lock
    fprintf(bat, "timeout /t 1 /nobreak > NUL\n");
    fprintf(bat, "del gware.exe\n");
    fprintf(bat, "ren gware.exe.new gware.exe\n");
    fprintf(bat, "echo Update successful! You are now running %s.\n", latest_version);
    // Self-destruct the batch file
    fprintf(bat, "del \"%%~f0\"\n");
    fclose(bat);
    
    // Execute the batch script asynchronously and exit immediately
    system("start /b update.bat");
    exit(0);
}
