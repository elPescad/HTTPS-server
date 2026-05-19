#include <iostream>
#include <string>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "Ws2_32.lib") 

int main() {
    //start up WSA
    WSADATA wsaData;
    int iresult;

    iresult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if(iresult != 0)
    {
        // if WSA failed to startup give error num
        // temp error handler. Check docs for error type.
        std::cout << "WSAStartup failed with error: " << iresult << std::endl;
        return 1;
    }

    // Check the bytes to see if they match the 2.2 version
    // If not return an error and free up space
    if(LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
    {
        std::cout << "Could not find a usable version of winsock.dll" << std::endl;
        WSACleanup();
        return 0;
    }
    else
    {
        std::cout << "The winsock dll 2.2 was found okay" << std::endl;
    }

    // Create SOCKET
    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    // Check for invalid socket
    if(sock == INVALID_SOCKET)
    {
        std::cout << "Socket function failed with error " << WSAGetLastError() << std::endl;
    }
    else
    {
        std::cout << "Socket function succeeded" << std::endl;
    }

    //struct specifically for IPv4 to be added to bind.
    sockaddr_in service;

    //specifies IPv4 protocol
    service.sin_family = AF_INET;

    //Local IP address for now
    inet_pton(AF_INET, "127.0.0.1", &service.sin_addr.s_addr);

    //htons stands for host to network.
    //we will be hosting on localhost:8080
    service.sin_port = htons(8080);

    //Bind socket to local address whoever it may be.
    //Tells computer "hey send or recieve only from this place"
    iresult = bind(sock, (sockaddr*) &service, sizeof(service));
    //Error handling
    if(iresult != 0)
    {
        std::cout << "Bind failed with error " << WSAGetLastError() << std::endl;
        WSACleanup();
        return 0;
    }
    else
    {
        std::cout << "Bind returned succesfully" << std::endl;
    }

    //Puts the socket in a listening state to check for
    //incoming requests
    iresult = listen(sock, SOMAXCONN);
    if(iresult != 0)
    {
        std::cout << "Listen failed with error " << WSAGetLastError() << std::endl;
        WSACleanup();
        return 0;
    }

    std::cout << "Listening on socket" << std::endl;

    sockaddr_in clientService;
    int addrlen = sizeof(clientService);

    SOCKET acceptSock = accept(sock, (sockaddr*) &clientService, &addrlen);
    if(acceptSock == INVALID_SOCKET)
    {
        std::cout << "Accept failed with error " << WSAGetLastError() << std::endl;
        closesocket(sock);
        WSACleanup();
        return 0;
    }

    std::cout << "Client connected" << std::endl;

    iresult = closesocket(sock);
    if(iresult != 0)
    {
        std::cout << "closesocket function failed with error " << WSAGetLastError() << std::endl;
        WSACleanup();
        return 0;
    }

    iresult = closesocket(acceptSock);
    if(iresult != 0)
    {
        std::cout << "closesocket function failed with error " << WSAGetLastError() << std::endl;
        WSACleanup();
        return 0;
    }

    // typically won't be called since server will be run for a while
    // however is necessary to prevent memory leaks.

    std::cout << "Yup we got it" << std::endl;
    WSACleanup();
    return 1;
}