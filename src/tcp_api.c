#include "tcp_api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "json_api.h"
#include <stdint.h>

#define SHA1_ROTL(bits, word) (((word) << (bits)) | ((word) >> (32-(bits))))

static void sha1_process_block(const uint8_t *block, uint32_t *h) {
    uint32_t w[80], a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
    for (int i = 0; i < 16; i++) {
        w[i] = ((uint32_t)block[i*4]<<24) | ((uint32_t)block[i*4+1]<<16) | ((uint32_t)block[i*4+2]<<8) | ((uint32_t)block[i*4+3]);
    }
    for (int i = 16; i < 80; i++) {
        w[i] = SHA1_ROTL(1, w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16]);
    }
    for (int i = 0; i < 80; i++) {
        uint32_t f, k;
        if (i < 20)      { f = (b & c) | ((~b) & d); k = 0x5A827999; }
        else if (i < 40) { f = b ^ c ^ d;            k = 0x6ED9EBA1; }
        else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDC; }
        else             { f = b ^ c ^ d;            k = 0xCA62C1D6; }
        uint32_t temp = SHA1_ROTL(5, a) + f + e + k + w[i];
        e = d; d = c; c = SHA1_ROTL(30, b); b = a; a = temp;
    }
    h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e;
}

static void sha1_hash(const char *msg, size_t len, uint8_t *out) {
    uint32_t h[5] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0};
    uint8_t block[64];
    size_t i = 0;
    size_t orig_len = len;
    while (len >= 64) {
        sha1_process_block((const uint8_t*)msg + i, h);
        i += 64; len -= 64;
    }
    memcpy(block, msg + i, len);
    block[len++] = 0x80;
    if (len > 56) {
        memset(block + len, 0, 64 - len);
        sha1_process_block(block, h);
        len = 0;
    }
    memset(block + len, 0, 56 - len);
    uint64_t bitlen = (uint64_t)orig_len * 8;
    block[56] = (bitlen >> 56) & 0xFF;
    block[57] = (bitlen >> 48) & 0xFF;
    block[58] = (bitlen >> 40) & 0xFF;
    block[59] = (bitlen >> 32) & 0xFF;
    block[60] = (bitlen >> 24) & 0xFF;
    block[61] = (bitlen >> 16) & 0xFF;
    block[62] = (bitlen >> 8) & 0xFF;
    block[63] = bitlen & 0xFF;
    sha1_process_block(block, h);
    for (int j = 0; j < 5; j++) {
        out[j*4] = (h[j] >> 24) & 0xFF;
        out[j*4+1] = (h[j] >> 16) & 0xFF;
        out[j*4+2] = (h[j] >> 8) & 0xFF;
        out[j*4+3] = h[j] & 0xFF;
    }
}

static const char b64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static void base64_encode(const uint8_t *in, size_t len, char *out) {
    size_t i = 0, j = 0;
    while (i < len) {
        size_t orig_i = i;
        uint32_t octet_a = i < len ? in[i++] : 0;
        uint32_t octet_b = i < len ? in[i++] : 0;
        uint32_t octet_c = i < len ? in[i++] : 0;
        uint32_t triple = (octet_a << 16) + (octet_b << 8) + octet_c;
        out[j++] = b64_table[(triple >> 18) & 0x3F];
        out[j++] = b64_table[(triple >> 12) & 0x3F];
        out[j++] = (orig_i + 1 < len) ? b64_table[(triple >> 6) & 0x3F] : '=';
        out[j++] = (orig_i + 2 < len) ? b64_table[triple & 0x3F] : '=';
    }
    out[j] = '\0';
}

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <sys/select.h>
    #define SOCKET int
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR -1
    #define closesocket close
#endif

static Value native_tcp_listen(int argCount, Value* args) {
    if (argCount != 2 || args[0].type != VAL_INT || (args[1].type != VAL_FUNCTION && args[1].type != VAL_NATIVE_FUNCTION)) {
        throw_error("tcp_listen expects (port, callback_function)");
    }
    int port = args[0].as.int_val;
    Value callback = args[1];

#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        throw_error("WSAStartup failed");
    }
#endif

    SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSocket == INVALID_SOCKET) {
#ifdef _WIN32
        WSACleanup();
#endif
        throw_error("Error creating socket");
    }

    struct sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);

    if (bind(listenSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        closesocket(listenSocket);
#ifdef _WIN32
        WSACleanup();
#endif
        throw_error("Bind failed");
    }

    if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) {
        closesocket(listenSocket);
#ifdef _WIN32
        WSACleanup();
#endif
        throw_error("Listen failed");
    }

    fd_set master_set, read_set;
    FD_ZERO(&master_set);
    FD_SET(listenSocket, &master_set);
    SOCKET max_sd = listenSocket;

    printf("Listening on port %d (non-blocking)...\n", port);
    fflush(stdout);

    while (1) {
        read_set = master_set;
        // Wait indefinitely for an event
        if (select(max_sd + 1, &read_set, NULL, NULL, NULL) == SOCKET_ERROR) {
            continue;
        }

        for (SOCKET i = 0; i <= max_sd; i++) {
            if (FD_ISSET(i, &read_set)) {
                if (i == listenSocket) {
                    // New connection
                    SOCKET clientSocket = accept(listenSocket, NULL, NULL);
                    if (clientSocket != INVALID_SOCKET) {
                        FD_SET(clientSocket, &master_set);
                        if (clientSocket > max_sd) max_sd = clientSocket;
                    }
                } else {
                    // Existing connection has data
                    char buffer[4096];
                    int bytesReceived = recv(i, buffer, sizeof(buffer) - 1, 0);
                    if (bytesReceived <= 0) {
                        // Connection closed or error
                        closesocket(i);
                        FD_CLR(i, &master_set);
                    } else {
                        buffer[bytesReceived] = '\0';
                        
                        // Invoke Gware callback
                        Value argsArr[1];
                        argsArr[0] = createString(buffer);
                        
                        Environment* call_env = Environment_create(get_global_env());
                        Value response = invokeFunction(callback, 1, argsArr, call_env, NULL);
                        
                        if (response.type == VAL_STRING && response.as.str_val) {
                            send(i, response.as.str_val, strlen(response.as.str_val), 0);
                        }
                        Environment_destroy(call_env);
                        gc_collect();
                        
                        // HTTP is stateless in this simple implementation, close after response
                        closesocket(i);
                        FD_CLR(i, &master_set);
                    }
                }
            }
        }
    }

    closesocket(listenSocket);
#ifdef _WIN32
    WSACleanup();
#endif
    return createNull();
}

void ws_send_frame(SOCKET sock, const char* msg) {
    size_t len = strlen(msg);
    uint8_t header[10];
    int hlen = 0;
    header[hlen++] = 0x81;
    if (len <= 125) {
        header[hlen++] = len;
    } else if (len <= 65535) {
        header[hlen++] = 126;
        header[hlen++] = (len >> 8) & 0xFF;
        header[hlen++] = len & 0xFF;
    } else {
        header[hlen++] = 127;
        for (int i=7; i>=0; i--) {
            header[hlen++] = (len >> (i*8)) & 0xFF;
        }
    }
    send(sock, (const char*)header, hlen, 0);
    send(sock, msg, len, 0);
}

static Value native_ws_send(int argCount, Value* args) {
    if (argCount != 2 || args[0].type != VAL_INT || args[1].type != VAL_STRING) {
        throw_error("ws_send expects (socket_id, message)");
    }
    SOCKET sock = args[0].as.int_val;
    const char* msg = args[1].as.str_val;
    ws_send_frame(sock, msg);
    return createNull();
}

static Value native_sse_send(int argCount, Value* args) {
    if (argCount < 3 || args[0].type != VAL_INT || args[1].type != VAL_STRING || args[2].type != VAL_STRING) return createNull();
    char buffer[4096];
    snprintf(buffer, sizeof(buffer), "event: %s\ndata: %s\n\n", args[1].as.str_val, args[2].as.str_val);
    send(args[0].as.int_val, buffer, strlen(buffer), 0);
    return createNull();
}

static int ws_clients[4096] = {0};
static char ws_paths[4096][256] = {{0}};

static int match_route(const char* pattern, const char* actualPath, ValueObject* params) {
    const char* p1 = pattern;
    const char* p2 = actualPath;
    while (*p1 && *p2) {
        if (*p1 == ':') {
            p1++;
            const char* nameStart = p1;
            while (*p1 && *p1 != '/' && *p1 != ' ') p1++;
            char name[128] = {0};
            strncpy(name, nameStart, p1 - nameStart);
            
            const char* valStart = p2;
            while (*p2 && *p2 != '/' && *p2 != ' ') p2++;
            char val[512] = {0};
            strncpy(val, valStart, p2 - valStart);
            
            Object_set_value(params, name, createString(val));
        } else if (*p1 == *p2) {
            p1++; p2++;
        } else {
            return 0;
        }
    }
    return (*p1 == '\0' && *p2 == '\0');
}

#include <sys/stat.h>
#include <pthread.h>

static SOCKET hmr_sockets[1024];
static int hmr_socket_count = 0;
static pthread_mutex_t hmr_mutex = PTHREAD_MUTEX_INITIALIZER;

void* hmr_watcher_thread(void* arg) {
    char* watch_path = (char*)arg;
    struct stat st;
    time_t last_mtime = 0;
    if (stat(watch_path, &st) == 0) {
        last_mtime = st.st_mtime;
    }
    
    while(1) {
#ifdef _WIN32
        Sleep(300);
#else
        usleep(300000);
#endif
        if (stat(watch_path, &st) == 0) {
            if (st.st_mtime != last_mtime) {
                last_mtime = st.st_mtime;
                pthread_mutex_lock(&hmr_mutex);
                for (int i = 0; i < hmr_socket_count; i++) {
                    ws_send_frame(hmr_sockets[i], "RELOAD");
                }
                pthread_mutex_unlock(&hmr_mutex);
            }
        }
    }
    return NULL;
}

Value native_http_serve(int argCount, Value* args) {
    if (argCount < 2 || args[0].type != VAL_INT || args[1].type != VAL_OBJECT) return createNull();
    int port = args[0].as.int_val;
    ValueObject* routes = args[1].as.obj_val;
    char* static_dir = NULL;
    if (argCount >= 3 && args[2].type == VAL_STRING) {
        static_dir = args[2].as.str_val;
    }

#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        throw_error("WSAStartup failed");
    }
#endif

    SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSocket == INVALID_SOCKET) {
#ifdef _WIN32
        WSACleanup();
#endif
        throw_error("Error creating socket");
    }

    // Allow port reuse
    int opt = 1;
    setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    struct sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);

    if (bind(listenSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        closesocket(listenSocket);
#ifdef _WIN32
        WSACleanup();
#endif
        throw_error("Bind failed");
    }

    if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) {
        closesocket(listenSocket);
#ifdef _WIN32
        WSACleanup();
#endif
        throw_error("Listen failed");
    }

    fd_set master_set, read_set;
    FD_ZERO(&master_set);
    FD_SET(listenSocket, &master_set);
    SOCKET max_sd = listenSocket;

    int isDev = 0;
    if (argCount >= 4 && args[3].type == VAL_INT && args[3].as.int_val == 1) {
        isDev = 1;
    }
    
    if (isDev && static_dir) {
        char* watch_path = malloc(1024);
        snprintf(watch_path, 1024, "%s/index.html", static_dir);
        pthread_t hmr_thread;
        pthread_create(&hmr_thread, NULL, hmr_watcher_thread, watch_path);
        pthread_detach(hmr_thread);
        printf("HMR Watcher started on %s\n", watch_path);
    }

    printf("HTTP Router listening on port %d...\n", port);
    fflush(stdout);

    while (1) {
        read_set = master_set;
        if (select(max_sd + 1, &read_set, NULL, NULL, NULL) == SOCKET_ERROR) {
            continue;
        }

        for (SOCKET i = 0; i <= max_sd; i++) {
            if (FD_ISSET(i, &read_set)) {
                if (i == listenSocket) {
                    SOCKET clientSocket = accept(listenSocket, NULL, NULL);
                    if (clientSocket != INVALID_SOCKET) {
                        FD_SET(clientSocket, &master_set);
                        if (clientSocket > max_sd) max_sd = clientSocket;
                    }
                } else {
                    char buffer[8192];
                    int bytesReceived = recv(i, buffer, sizeof(buffer) - 1, 0);
                    if (bytesReceived <= 0) {
                        closesocket(i);
                        FD_CLR(i, &master_set);
                        if (i < 4096) ws_clients[i] = 0;
                    } else {
                        buffer[bytesReceived] = '\0';
                        
                        if (i < 4096 && ws_clients[i]) {
                            uint8_t* ubuf = (uint8_t*)buffer;
                            int opcode = ubuf[0] & 0x0F;
                            if (opcode == 8) { // Close frame
                                closesocket(i);
                                FD_CLR(i, &master_set);
                                ws_clients[i] = 0;
                                continue;
                            }
                            int has_mask = (ubuf[1] & 0x80) != 0;
                            uint64_t payload_len = ubuf[1] & 0x7F;
                            int offset = 2;
                            if (payload_len == 126) {
                                payload_len = (ubuf[2] << 8) | ubuf[3];
                                offset = 4;
                            }
                            uint8_t mask[4] = {0};
                            if (has_mask) {
                                mask[0] = ubuf[offset++]; mask[1] = ubuf[offset++]; mask[2] = ubuf[offset++]; mask[3] = ubuf[offset++];
                            }
                            char* payload = malloc(payload_len + 1);
                            for (uint64_t k = 0; k < payload_len; k++) {
                                payload[k] = ubuf[offset + k] ^ (has_mask ? mask[k % 4] : 0);
                            }
                            payload[payload_len] = '\0';
                            
                            char routeKey[1050];
                            sprintf(routeKey, "WS %s", ws_paths[i]);
                            Value callback;
                            callback.type = VAL_INT;
                            for (int k = 0; k < routes->count; k++) {
                                if (strcmp(routes->keys[k], routeKey) == 0) {
                                    callback = routes->values[k];
                                    break;
                                }
                            }
                            if (callback.type == VAL_FUNCTION || callback.type == VAL_NATIVE_FUNCTION) {
                                Value reqObj = createObject(5);
                                reqObj.as.obj_val->keys[0] = strdup("body");
                                reqObj.as.obj_val->values[0] = createString(payload);
                                reqObj.as.obj_val->keys[1] = strdup("socket_id");
                                reqObj.as.obj_val->values[1] = createInt(i);
                                reqObj.as.obj_val->count = 2;
                                Value argsArr[1]; argsArr[0] = reqObj;
                                Environment* call_env = Environment_create(get_global_env());
                                Value response = invokeFunction(callback, 1, argsArr, call_env, NULL);
                                if (response.type == VAL_STRING && response.as.str_val) {
                                    ws_send_frame(i, response.as.str_val);
                                }
                                Environment_destroy(call_env);
                                gc_collect();
                            }
                            free(payload);
                            continue;
                        }
                        
                        char method[16] = {0};
                        char path[1024] = {0};
                        char* space1 = strchr(buffer, ' ');
                        if (space1) {
                            int methodLen = space1 - buffer;
                            if (methodLen < 15) {
                                strncpy(method, buffer, methodLen);
                            }
                            char* space2 = strchr(space1 + 1, ' ');
                            if (space2) {
                                int pathLen = space2 - (space1 + 1);
                                if (pathLen < 1023) {
                                    strncpy(path, space1 + 1, pathLen);
                                }
                            }
                        }
                        
                        char* upgrade = strstr(buffer, "Upgrade: websocket");
                        if (upgrade) {
                            char* key = strstr(buffer, "Sec-WebSocket-Key: ");
                            if (key) {
                                key += 19;
                                char* keyEnd = strstr(key, "\r\n");
                                if (keyEnd) {
                                    char clientKey[256] = {0};
                                    strncpy(clientKey, key, keyEnd - key);
                                    char concatKey[512];
                                    sprintf(concatKey, "%s258EAFA5-E914-47DA-95CA-C5AB0DC85B11", clientKey);
                                    uint8_t sha1_out[20];
                                    sha1_hash(concatKey, strlen(concatKey), sha1_out);
                                    char b64_out[128];
                                    base64_encode(sha1_out, 20, b64_out);
                                    char response[512];
                                    sprintf(response,
                                        "HTTP/1.1 101 Switching Protocols\r\n"
                                        "Upgrade: websocket\r\n"
                                        "Connection: Upgrade\r\n"
                                        "Sec-WebSocket-Accept: %s\r\n\r\n", b64_out);
                                    send(i, response, strlen(response), 0);
                                    if (i < 4096) {
                                        ws_clients[i] = 1;
                                        strncpy(ws_paths[i], path, 255);
                                    }
                                    if (strcmp(path, "/__hmr") == 0) {
                                        pthread_mutex_lock(&hmr_mutex);
                                        if (hmr_socket_count < 1024) hmr_sockets[hmr_socket_count++] = i;
                                        pthread_mutex_unlock(&hmr_mutex);
                                    }
                                    continue;
                                }
                            }
                        }
                        
                        char routeKey[1050];
                        sprintf(routeKey, "%s %s", method, path);
                        
                        Value callback;
                        callback.type = VAL_INT; // invalid type to indicate not found
                        Value paramsObj = createObject(4);
                        
                        for (int k = 0; k < routes->count; k++) {
                            paramsObj.as.obj_val->count = 0; // reset for each attempt
                            if (match_route(routes->keys[k], routeKey, paramsObj.as.obj_val)) {
                                callback = routes->values[k];
                                break;
                            }
                        }
                        
                        if (callback.type == VAL_FUNCTION || callback.type == VAL_NATIVE_FUNCTION) {
                            char* bodyStr = strstr(buffer, "\r\n\r\n");
                            if (bodyStr) bodyStr += 4;
                            else bodyStr = "";
                            
                            Value cookiesObj = createObject(5);
                            char* cookieStr = strstr(buffer, "Cookie: ");
                            if (cookieStr) {
                                cookieStr += 8;
                                char* end = strstr(cookieStr, "\r\n");
                                if (end) {
                                    char cstr[1024] = {0};
                                    strncpy(cstr, cookieStr, end - cookieStr);
                                    char* token = strtok(cstr, "; ");
                                    int cidx = 0;
                                    while (token && cidx < cookiesObj.as.obj_val->capacity) {
                                        char* eq = strchr(token, '=');
                                        if (eq) {
                                            *eq = 0;
                                            cookiesObj.as.obj_val->keys[cidx] = strdup(token);
                                            cookiesObj.as.obj_val->values[cidx] = createString(eq + 1);
                                            cidx++;
                                        }
                                        token = strtok(NULL, "; ");
                                    }
                                    cookiesObj.as.obj_val->count = cidx;
                                }
                            }
                            
                            Value reqObj = createObject(6);
                            reqObj.as.obj_val->keys[0] = strdup("method");
                            reqObj.as.obj_val->values[0] = createString(method);
                            reqObj.as.obj_val->keys[1] = strdup("path");
                            reqObj.as.obj_val->values[1] = createString(path);
                            reqObj.as.obj_val->keys[2] = strdup("body");
                            reqObj.as.obj_val->values[2] = createString(bodyStr);
                            reqObj.as.obj_val->keys[3] = strdup("params");
                            reqObj.as.obj_val->values[3] = paramsObj;
                            reqObj.as.obj_val->keys[4] = strdup("fd");
                            reqObj.as.obj_val->values[4] = createInt(i);
                            reqObj.as.obj_val->keys[5] = strdup("cookies");
                            reqObj.as.obj_val->values[5] = cookiesObj;
                            reqObj.as.obj_val->count = 6;
                            
                            Value argsArr[1];
                            argsArr[0] = reqObj;
                            
                            Environment* call_env = Environment_create(get_global_env());
                            
                            Value response;
                            response.type = VAL_STRING;
                            response.as.str_val = NULL; // null
                            int middleware_intercepted = 0;
                            for (int m = 0; m < routes->count; m++) {
                                if (strcmp(routes->keys[m], "MIDDLEWARE") == 0) {
                                    Value mw = routes->values[m];
                                    if (mw.type == VAL_ARRAY) {
                                        for (int j = 0; j < mw.as.arr_val->count; j++) {
                                            Value mw_func = mw.as.arr_val->elements[j];
                                            if (mw_func.type == VAL_FUNCTION || mw_func.type == VAL_NATIVE_FUNCTION) {
                                                Value mw_res = invokeFunction(mw_func, 1, argsArr, call_env, NULL);
                                                int is_null = (mw_res.type == VAL_STRING && mw_res.as.str_val == NULL);
                                                if (!is_null) {
                                                    response = mw_res;
                                                    middleware_intercepted = 1;
                                                    break;
                                                }
                                            }
                                        }
                                    }
                                    break;
                                }
                            }
                            
                            if (!middleware_intercepted) {
                                response = invokeFunction(callback, 1, argsArr, call_env, NULL);
                            }

                            
                            int is_sse = 0;
                            if (response.type == VAL_OBJECT) {
                                for (int m = 0; m < response.as.obj_val->count; m++) {
                                    if (strcmp(response.as.obj_val->keys[m], "sse") == 0) {
                                        is_sse = 1;
                                        break;
                                    }
                                }
                            }
                            
                            if (is_sse) {
                                const char* sse_headers = "HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nCache-Control: no-cache\r\nConnection: keep-alive\r\n\r\n";
                                send(i, sse_headers, strlen(sse_headers), 0);
                                Environment_destroy(call_env);
                                gc_collect();
                                continue;
                            }
                            
                            int is_advanced = 0;
                            Value headers_obj = createNull();
                            Value body_val = createNull();
                            if (response.type == VAL_OBJECT) {
                                for (int m = 0; m < response.as.obj_val->count; m++) {
                                    if (strcmp(response.as.obj_val->keys[m], "body") == 0) {
                                        is_advanced = 1;
                                        body_val = response.as.obj_val->values[m];
                                    } else if (strcmp(response.as.obj_val->keys[m], "headers") == 0) {
                                        headers_obj = response.as.obj_val->values[m];
                                    }
                                }
                            }
                            
                            if (is_advanced) {
                                char resBuffer[8192] = {0};
                                sprintf(resBuffer, "HTTP/1.1 200 OK\r\n");
                                
                                int has_content_type = 0;
                                if (headers_obj.type == VAL_OBJECT) {
                                    for (int m = 0; m < headers_obj.as.obj_val->count; m++) {
                                        Value hval = headers_obj.as.obj_val->values[m];
                                        if (hval.type == VAL_STRING) {
                                            char hline[512];
                                            snprintf(hline, sizeof(hline), "%s: %s\r\n", headers_obj.as.obj_val->keys[m], hval.as.str_val);
                                            strcat(resBuffer, hline);
                                            if (strcmp(headers_obj.as.obj_val->keys[m], "Content-Type") == 0) has_content_type = 1;
                                        }
                                    }
                                }
                                
                                Value jsonStr = createNull();
                                const char* final_body = "";
                                if (body_val.type == VAL_STRING && body_val.as.str_val) {
                                    final_body = body_val.as.str_val;
                                    if (!has_content_type) strcat(resBuffer, "Content-Type: text/html\r\n");
                                } else if (body_val.type == VAL_OBJECT || body_val.type == VAL_ARRAY) {
                                    jsonStr = native_json_stringify(1, &body_val);
                                    if (jsonStr.type == VAL_STRING && jsonStr.as.str_val) {
                                        final_body = jsonStr.as.str_val;
                                        if (!has_content_type) strcat(resBuffer, "Content-Type: application/json\r\n");
                                    }
                                }
                                
                                char clen[64];
                                snprintf(clen, sizeof(clen), "Content-Length: %zu\r\n\r\n", strlen(final_body));
                                strcat(resBuffer, clen);
                                
                                send(i, resBuffer, strlen(resBuffer), 0);
                                send(i, final_body, strlen(final_body), 0);
                                
                            } else if (response.type == VAL_OBJECT || response.type == VAL_ARRAY) {
                                Value jsonStr = native_json_stringify(1, &response);
                                if (jsonStr.type == VAL_STRING && jsonStr.as.str_val) {
                                    char* resBuffer = malloc(strlen(jsonStr.as.str_val) + 256);
                                    sprintf(resBuffer, "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: %zu\r\n\r\n%s", strlen(jsonStr.as.str_val), jsonStr.as.str_val);
                                    send(i, resBuffer, strlen(resBuffer), 0);
                                    free(resBuffer);
                                }
                            } else if (response.type == VAL_STRING && response.as.str_val) {
                                char* resBuffer = malloc(strlen(response.as.str_val) + 256);
                                sprintf(resBuffer, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: %zu\r\n\r\n%s", strlen(response.as.str_val), response.as.str_val);
                                send(i, resBuffer, strlen(resBuffer), 0);
                                free(resBuffer);
                            } else {
                                const char* okResponse = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nOK";
                                send(i, okResponse, strlen(okResponse), 0);
                            }

                            
                            Environment_destroy(call_env);
                            gc_collect();
                        } else {
                            int served = 0;
                            if (static_dir && strcmp(method, "GET") == 0) {
                                char filePath[1024];
                                snprintf(filePath, sizeof(filePath), "%s%s", static_dir, strcmp(path, "/") == 0 ? "/index.html" : path);
                                FILE* f = fopen(filePath, "rb");
                                if (f) {
                                    fseek(f, 0, SEEK_END);
                                    long fsize = ftell(f);
                                    fseek(f, 0, SEEK_SET);
                                    char* fbuf = malloc(fsize + 1);
                                    fread(fbuf, 1, fsize, f);
                                    fclose(f);
                                    fbuf[fsize] = 0;
                                    
                                    const char* mime = "text/plain";
                                    if (strstr(filePath, ".html")) mime = "text/html";
                                    else if (strstr(filePath, ".css")) mime = "text/css";
                                    else if (strstr(filePath, ".js")) mime = "application/javascript";
                                    else if (strstr(filePath, ".png")) mime = "image/png";
                                    else if (strstr(filePath, ".jpg")) mime = "image/jpeg";
                                    
                                    char* resBuffer = malloc(fsize + 512);
                                    sprintf(resBuffer, "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %ld\r\n\r\n", mime, fsize);
                                    send(i, resBuffer, strlen(resBuffer), 0);
                                    send(i, fbuf, fsize, 0);
                                    free(resBuffer);
                                    free(fbuf);
                                    served = 1;
                                }
                            }
                            
                            if (!served) {
                                const char* notFound = "HTTP/1.1 404 Not Found\r\nContent-Length: 9\r\n\r\nNot Found";
                                send(i, notFound, strlen(notFound), 0);
                            }
                        }
                        
                        closesocket(i);
                        FD_CLR(i, &master_set);
                    }
                }
            }
        }
    }

    closesocket(listenSocket);
#ifdef _WIN32
    WSACleanup();
#endif
    return createNull();
}

void register_tcp_api(Environment* env) {
    Value tcp_val; tcp_val.type = VAL_NATIVE_FUNCTION; tcp_val.is_return = 0; tcp_val.as.native_fn = native_tcp_listen;
    Environment_set(env, "tcp_listen", tcp_val, NULL);
    
    Value http_val; http_val.type = VAL_NATIVE_FUNCTION; http_val.is_return = 0; http_val.as.native_fn = native_http_serve;
    Environment_set(env, "http_serve", http_val, NULL);
    
    Value ws_val; ws_val.type = VAL_NATIVE_FUNCTION; ws_val.is_return = 0; ws_val.as.native_fn = native_ws_send;
    Environment_set(env, "ws_send", ws_val, NULL);

    Value sse_val; sse_val.type = VAL_NATIVE_FUNCTION; sse_val.is_return = 0; sse_val.as.native_fn = native_sse_send;
    Environment_set(env, "sse_send", sse_val, NULL);
}

