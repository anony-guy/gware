#include "tcp_api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "json_api.h"

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
                        Value response = invokeFunction(callback, 1, argsArr, call_env);
                        
                        if (response.type == VAL_STRING && response.as.str_val) {
                            send(i, response.as.str_val, strlen(response.as.str_val), 0);
                        }
                        Value_free(response);
                        Environment_destroy(call_env);
                        Value_free(argsArr[0]);
                        
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

static Value native_http_serve(int argCount, Value* args) {
    if (argCount != 2 || args[0].type != VAL_INT || args[1].type != VAL_OBJECT) {
        throw_error("http_serve expects (port, routes_object)");
    }
    int port = args[0].as.int_val;
    ValueObject* routes = args[1].as.obj_val;

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
                    } else {
                        buffer[bytesReceived] = '\0';
                        
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
                        
                        char routeKey[1050];
                        sprintf(routeKey, "%s %s", method, path);
                        
                        Value callback;
                        callback.type = VAL_INT; // invalid type to indicate not found
                        for (int k = 0; k < routes->count; k++) {
                            if (strcmp(routes->keys[k], routeKey) == 0) {
                                callback = routes->values[k];
                                break;
                            }
                        }
                        
                        if (callback.type == VAL_FUNCTION || callback.type == VAL_NATIVE_FUNCTION) {
                            char* bodyStr = strstr(buffer, "\r\n\r\n");
                            if (bodyStr) bodyStr += 4;
                            else bodyStr = "";
                            
                            Value reqObj = createObject(5);
                            reqObj.as.obj_val->keys[0] = strdup("method");
                            reqObj.as.obj_val->values[0] = createString(method);
                            reqObj.as.obj_val->keys[1] = strdup("path");
                            reqObj.as.obj_val->values[1] = createString(path);
                            reqObj.as.obj_val->keys[2] = strdup("body");
                            reqObj.as.obj_val->values[2] = createString(bodyStr);
                            reqObj.as.obj_val->count = 3;
                            
                            Value argsArr[1];
                            argsArr[0] = reqObj;
                            
                            Environment* call_env = Environment_create(get_global_env());
                            Value response = invokeFunction(callback, 1, argsArr, call_env);
                            
                            if (response.type == VAL_OBJECT || response.type == VAL_ARRAY) {
                                Value jsonStr = native_json_stringify(1, &response);
                                if (jsonStr.type == VAL_STRING && jsonStr.as.str_val) {
                                    char* resBuffer = malloc(strlen(jsonStr.as.str_val) + 256);
                                    sprintf(resBuffer, "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: %zu\r\n\r\n%s", strlen(jsonStr.as.str_val), jsonStr.as.str_val);
                                    send(i, resBuffer, strlen(resBuffer), 0);
                                    free(resBuffer);
                                }
                                Value_free(jsonStr);
                            } else if (response.type == VAL_STRING && response.as.str_val) {
                                char* resBuffer = malloc(strlen(response.as.str_val) + 256);
                                sprintf(resBuffer, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: %zu\r\n\r\n%s", strlen(response.as.str_val), response.as.str_val);
                                send(i, resBuffer, strlen(resBuffer), 0);
                                free(resBuffer);
                            } else {
                                const char* okResponse = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nOK";
                                send(i, okResponse, strlen(okResponse), 0);
                            }
                            
                            Value_free(response);
                            Environment_destroy(call_env);
                            Value_free(reqObj);
                        } else {
                            const char* notFound = "HTTP/1.1 404 Not Found\r\nContent-Length: 9\r\n\r\nNot Found";
                            send(i, notFound, strlen(notFound), 0);
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
}

