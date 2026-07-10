#include "net.h"
#include <windows.h>
#include <wininet.h>
#include <stdlib.h>

char* fetch_url(const char* url) {
    HINTERNET hInternet = InternetOpen("Gware/0.0.0.3", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!hInternet) return NULL;
    
    HINTERNET hConnect = InternetOpenUrl(hInternet, url, NULL, 0, INTERNET_FLAG_RELOAD, 0);
    if (!hConnect) {
        InternetCloseHandle(hInternet);
        return NULL;
    }
    
    int capacity = 4096;
    int size = 0;
    char* buf = (char*)malloc(capacity);
    DWORD bytesRead;
    
    while (InternetReadFile(hConnect, buf + size, capacity - size - 1, &bytesRead) && bytesRead > 0) {
        size += bytesRead;
        if (size >= capacity - 1) {
            capacity *= 2;
            buf = (char*)realloc(buf, capacity);
        }
    }
    
    buf[size] = '\0';
    InternetCloseHandle(hConnect);
    InternetCloseHandle(hInternet);
    return buf;
}
