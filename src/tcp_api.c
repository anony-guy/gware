#include "tcp_api.h"
#include <winsock2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#pragma comment(lib, "ws2_32.lib") // Required for MSVC but Zig uses -lws2_32

static Value native_tcp_listen(int argCount, Value* args) {
    if (argCount != 2 || args[0].type != VAL_INT || (args[1].type != VAL_FUNCTION && args[1].type != VAL_NATIVE_FUNCTION)) {
        throw_error("tcp_listen expects (port, callback_function)");
    }
    int port = args[0].as.int_val;
    Value callback = args[1];

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        throw_error("WSAStartup failed");
    }

    SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSocket == INVALID_SOCKET) {
        WSACleanup();
        throw_error("Error creating socket");
    }

    struct sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);

    if (bind(listenSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        closesocket(listenSocket);
        WSACleanup();
        throw_error("Bind failed");
    }

    if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) {
        closesocket(listenSocket);
        WSACleanup();
        throw_error("Listen failed");
    }

    printf("Listening on port %d...\n", port);
    fflush(stdout);

    while (1) {
        SOCKET clientSocket = accept(listenSocket, NULL, NULL);
        if (clientSocket == INVALID_SOCKET) {
            continue;
        }

        char buffer[4096];
        int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        if (bytesReceived > 0) {
            buffer[bytesReceived] = '\0';
            
            // Invoke Gware callback
            Value argsArr[1];
            argsArr[0] = createString(buffer);
            
            Environment* call_env = Environment_create(get_global_env());
            Value response = invokeFunction(callback, 1, argsArr, call_env);
            
            if (response.type == VAL_STRING && response.as.str_val) {
                send(clientSocket, response.as.str_val, strlen(response.as.str_val), 0);
            }
            Value_free(response);
            Environment_destroy(call_env);
            Value_free(argsArr[0]);
        }
        closesocket(clientSocket);
    }

    closesocket(listenSocket);
    WSACleanup();
    return createNull();
}

void register_tcp_api(Environment* env) {
    Value tcp_val; tcp_val.type = VAL_NATIVE_FUNCTION; tcp_val.is_return = 0; tcp_val.as.native_fn = native_tcp_listen;
    Environment_set(env, "tcp_listen", tcp_val, NULL);
}
