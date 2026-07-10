#include "tcp_api.h"
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

void register_tcp_api(Environment* env) {
    Value tcp_val; tcp_val.type = VAL_NATIVE_FUNCTION; tcp_val.is_return = 0; tcp_val.as.native_fn = native_tcp_listen;
    Environment_set(env, "tcp_listen", tcp_val, NULL);
}
