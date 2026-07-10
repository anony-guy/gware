#include "net.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <netdb.h>
    #define SOCKET int
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR -1
    #define closesocket close
#endif

char* fetch_url(const char* url) {
    if (strncmp(url, "http://", 7) != 0) {
        printf("Error: Only http:// is supported natively\n");
        return NULL;
    }

#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return NULL;
#endif

    const char* url_no_protocol = url + 7;
    const char* path_start = strchr(url_no_protocol, '/');
    
    char hostname[256];
    char path[1024];
    
    if (path_start) {
        int host_len = path_start - url_no_protocol;
        if (host_len >= sizeof(hostname)) host_len = sizeof(hostname) - 1;
        strncpy(hostname, url_no_protocol, host_len);
        hostname[host_len] = '\0';
        strncpy(path, path_start, sizeof(path) - 1);
        path[sizeof(path) - 1] = '\0';
    } else {
        strncpy(hostname, url_no_protocol, sizeof(hostname) - 1);
        hostname[sizeof(hostname) - 1] = '\0';
        strcpy(path, "/");
    }

    struct hostent* host = gethostbyname(hostname);
    if (!host) {
#ifdef _WIN32
        WSACleanup();
#endif
        return NULL;
    }

    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
#ifdef _WIN32
        WSACleanup();
#endif
        return NULL;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(80);
    server_addr.sin_addr.s_addr = *(unsigned long*)host->h_addr_list[0];

    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        closesocket(sock);
#ifdef _WIN32
        WSACleanup();
#endif
        return NULL;
    }

    char request[2048];
    snprintf(request, sizeof(request), 
             "GET %s HTTP/1.1\r\n"
             "Host: %s\r\n"
             "Connection: close\r\n"
             "User-Agent: Gware/0.0.0.6\r\n"
             "\r\n", path, hostname);

    send(sock, request, strlen(request), 0);

    int capacity = 4096;
    int size = 0;
    char* buf = (char*)malloc(capacity);
    int bytesRead;

    while ((bytesRead = recv(sock, buf + size, capacity - size - 1, 0)) > 0) {
        size += bytesRead;
        if (size >= capacity - 1) {
            capacity *= 2;
            buf = (char*)realloc(buf, capacity);
        }
    }

    buf[size] = '\0';
    closesocket(sock);

#ifdef _WIN32
    WSACleanup();
#endif

    // Strip HTTP headers
    char* body = strstr(buf, "\r\n\r\n");
    if (body) {
        body += 4;
        char* final_body = strdup(body);
        free(buf);
        return final_body;
    }
    
    return buf;
}
