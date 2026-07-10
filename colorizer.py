import os
import re

def colorize(filepath):
    with open(filepath, 'r') as f:
        content = f.read()

    # Add include
    if '#include "color.h"' not in content:
        content = content.replace('#include <stdio.h>', '#include <stdio.h>\n#include "color.h"')

    # Replace Runtime Errors
    content = re.sub(r'printf\("Runtime Error: (.*?)\\n"\)', r'printf(ANSI_COLOR_RED "Runtime Error: \1\\n" ANSI_COLOR_RESET)', content)
    content = re.sub(r'printf\("Runtime Error: (.*?)\\n", (.*?)\)', r'printf(ANSI_COLOR_RED "Runtime Error: \1\\n" ANSI_COLOR_RESET, \2)', content)

    # Replace Parse Errors
    content = re.sub(r'printf\("Parse error: (.*?)\\n"\)', r'printf(ANSI_COLOR_RED "Parse error: \1\\n" ANSI_COLOR_RESET)', content)
    content = re.sub(r'printf\("Parse error: (.*?)\\n", (.*?)\)', r'printf(ANSI_COLOR_RED "Parse error: \1\\n" ANSI_COLOR_RESET, \2)', content)

    # Transpiler
    content = content.replace('printf("Error: Could not open output file %s\\n", outputFile);', 'printf(ANSI_COLOR_RED "Error: Could not open output file %s\\n" ANSI_COLOR_RESET, outputFile);')
    content = content.replace('printf("Successfully transpiled to %s\\n", outputFile);', 'printf(ANSI_COLOR_GREEN "Successfully transpiled to %s\\n" ANSI_COLOR_RESET, outputFile);')

    # Updater
    content = content.replace('printf("Checking GitHub for updates...\\n");', 'printf(ANSI_COLOR_YELLOW "Checking GitHub for updates...\\n" ANSI_COLOR_RESET);')
    content = content.replace('printf("Error: Could not connect to GitHub.\\n");', 'printf(ANSI_COLOR_RED "Error: Could not connect to GitHub.\\n" ANSI_COLOR_RESET);')
    content = content.replace('printf("No releases found on the GitHub repository yet.\\n");', 'printf(ANSI_COLOR_YELLOW "No releases found on the GitHub repository yet.\\n" ANSI_COLOR_RESET);')
    content = content.replace('printf("Make sure to publish a Release on GitHub before attempting an update!\\n");', 'printf(ANSI_COLOR_YELLOW "Make sure to publish a Release on GitHub before attempting an update!\\n" ANSI_COLOR_RESET);')
    content = content.replace('printf("You are already on the latest version (%s). No update required!\\n", current_version);', 'printf(ANSI_COLOR_GREEN "You are already on the latest version (%s). No update required!\\n" ANSI_COLOR_RESET, current_version);')
    content = content.replace('printf("New version found: %s (Current: %s)\\n", latest_version, current_version);', 'printf(ANSI_COLOR_CYAN "New version found: %s (Current: %s)\\n" ANSI_COLOR_RESET, latest_version, current_version);')
    content = content.replace('printf("Downloading update...\\n");', 'printf(ANSI_COLOR_YELLOW "Downloading update...\\n" ANSI_COLOR_RESET);')
    content = content.replace('printf("Error: Failed to download the update. Does the release contain a \'gware.exe\' asset?\\n");', 'printf(ANSI_COLOR_RED "Error: Failed to download the update. Does the release contain a \'gware.exe\' asset?\\n" ANSI_COLOR_RESET);')
    content = content.replace('printf("Download complete! Applying update...\\n");', 'printf(ANSI_COLOR_GREEN "Download complete! Applying update...\\n" ANSI_COLOR_RESET);')
    content = content.replace('printf("Error: Could not create update script.\\n");', 'printf(ANSI_COLOR_RED "Error: Could not create update script.\\n" ANSI_COLOR_RESET);')

    with open(filepath, 'w') as f:
        f.write(content)

for root, dirs, files in os.walk('src'):
    for file in files:
        if file.endswith('.c'):
            colorize(os.path.join(root, file))
