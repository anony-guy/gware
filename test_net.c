#include <stdio.h>
#include <windows.h>
#include <wininet.h>

int main() {
    HINTERNET hInternet = InternetOpen("Gware/1.0", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!hInternet) {
        printf("InternetOpen failed\n");
        return 1;
    }
    
    HINTERNET hConnect = InternetOpenUrl(hInternet, "http://example.com", NULL, 0, INTERNET_FLAG_RELOAD, 0);
    if (!hConnect) {
        printf("InternetOpenUrl failed\n");
        InternetCloseHandle(hInternet);
        return 1;
    }
    
    char buffer[1024];
    DWORD bytesRead;
    if (InternetReadFile(hConnect, buffer, sizeof(buffer) - 1, &bytesRead)) {
        buffer[bytesRead] = '\0';
        printf("Read %lu bytes: %s\n", bytesRead, buffer);
    } else {
        printf("InternetReadFile failed\n");
    }
    
    InternetCloseHandle(hConnect);
    InternetCloseHandle(hInternet);
    return 0;
}
