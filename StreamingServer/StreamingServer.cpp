// StreamingTest.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <thread>
#include <atomic>

#pragma comment (lib, "Ws2_32.lib")

#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "27015"

#define ERR(e) \
        printf("%s:%s failed: %d [%s@%ld]\n",__FUNCTION__,e,WSAGetLastError(),__FILE__,__LINE__)

#define DEFAULT_WAIT    30000

int main()
{
    WSADATA wsaData;
    int iResult;

    SOCKET ListenSocket = INVALID_SOCKET;
    SOCKET ClientSocket = INVALID_SOCKET;

    struct addrinfo* result = nullptr;
    struct addrinfo hints;

    int iSendResult;
    char recvbuf[DEFAULT_BUFLEN];
    int recvbuflen = DEFAULT_BUFLEN;

    // Initialize Winsock
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        printf("WSAStartup failed with error: %d\n", iResult);
        return 1;
    }

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    // Resolve the server address and port
    iResult = getaddrinfo(nullptr, DEFAULT_PORT, &hints, &result);
    if (iResult != 0) {
        printf("getaddrinfo failed with error: %d\n", iResult);
        WSACleanup();
        return 1;
    }

    // Create a SOCKET for connecting to server
    ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (ListenSocket == INVALID_SOCKET) {
        printf("socket failed with error: %ld\n", WSAGetLastError());
        freeaddrinfo(result);
        WSACleanup();
        return 1;
    }

    // Setup the TCP listening socket
    iResult = bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen);
    if (iResult == SOCKET_ERROR) {
        printf("bind failed with error: %d\n", WSAGetLastError());
        freeaddrinfo(result);
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }

    freeaddrinfo(result);

    iResult = listen(ListenSocket, SOMAXCONN);
    if (iResult == SOCKET_ERROR) {
        printf("listen failed with error: %d\n", WSAGetLastError());
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }

    // Accept a client socket
    ClientSocket = accept(ListenSocket, nullptr, nullptr);
    if (ClientSocket == INVALID_SOCKET) {
        printf("accept failed with error: %d\n", WSAGetLastError());
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }

    printf("establish connection\n");

    // No longer need server socket
    closesocket(ListenSocket);

    const BOOL keepalive = TRUE;
    setsockopt(ClientSocket, SOL_SOCKET, SO_KEEPALIVE, reinterpret_cast<const char*>(&keepalive), sizeof(keepalive));
    const DWORD num_keepalive_strobes = 1;
    setsockopt(ClientSocket, IPPROTO_TCP, TCP_KEEPCNT, reinterpret_cast<const char*>(&num_keepalive_strobes), sizeof(num_keepalive_strobes));
    const DWORD keepalive_idle_time_secs = 1;
    setsockopt(ClientSocket, IPPROTO_TCP, TCP_KEEPIDLE, reinterpret_cast<const char*>(&keepalive_idle_time_secs), sizeof(keepalive_idle_time_secs));
    const DWORD keepalive_strobe_interval_secs = 1;
    setsockopt(ClientSocket, IPPROTO_TCP, TCP_KEEPINTVL, reinterpret_cast<const char*>(&keepalive_strobe_interval_secs), sizeof(keepalive_strobe_interval_secs));

    WSAEVENT NewEvent = WSACreateEvent();
    WSAEventSelect(ClientSocket, NewEvent, FD_CLOSE);

    std::atomic<bool> connected{ true };
    std::thread th([&]() {
        DWORD Event;
        if ((Event = WSAWaitForMultipleEvents(1, &NewEvent, FALSE, WSA_INFINITE, FALSE)) == WSA_WAIT_FAILED)
            printf("WSAWaitForMultipleEvents() failed with error %d\n", WSAGetLastError());
        else
            printf("WSAWaitForMultipleEvents() close event!\n");

        connected = false;
    });

    WSAPOLLFD fdarray = { 0 };
    // Receive until the peer shuts down the connection
    do {
        printf("loop to send and receive data\n");
        Sleep(1000);
    } while (connected);

    th.join();

    // shutdown the connection since we're done
    iResult = shutdown(ClientSocket, SD_SEND);
    if (iResult == SOCKET_ERROR) {
        printf("shutdown failed with error: %d\n", WSAGetLastError());
        closesocket(ClientSocket);
        WSACleanup();
        return 1;
    }

    printf("shutdown\n");

    // cleanup
    closesocket(ClientSocket);
    WSACleanup();

    return 0;
}
