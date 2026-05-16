#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>

// Tell the compiler to link the Winsock library (works in MSVC)
#pragma comment(lib, "Ws2_32.lib") 

int main() {
    // 0. INITIALIZE WINSOCK (Windows Specific!)
    WSADATA wsaData;
    int wsaerr = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (wsaerr != 0) {
        std::cerr << "The Winsock dll not found!" << std::endl;
        return 1;
    }

    // 1. CREATE THE SOCKET
    // Notice Windows uses 'SOCKET' instead of a standard 'int'
    SOCKET server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_fd == INVALID_SOCKET) {
        std::cerr << "Failed to create socket! Error: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return 1;
    }

    // 2. CONFIGURE THE ADDRESS (Port and IP)
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY; 
    address.sin_port = htons(8080); 

    // 3. BIND THE SOCKET
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) == SOCKET_ERROR) {
        std::cerr << "Bind failed! Error: " << WSAGetLastError() << std::endl;
        closesocket(server_fd);
        WSACleanup();
        return 1;
    }

    // 4. LISTEN FOR CONNECTIONS
    if (listen(server_fd, 3) == SOCKET_ERROR) {
        std::cerr << "Listen failed! Error: " << WSAGetLastError() << std::endl;
        closesocket(server_fd);
        WSACleanup();
        return 1;
    }
    
    std::cout << "Server is officially listening on port 8080..." << std::endl;

    // 5. ACCEPT A CONNECTION
    int addrlen = sizeof(address);
    std::cout << "Waiting for a browser to connect..." << std::endl;
    
    SOCKET user_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen);
    if (user_socket == INVALID_SOCKET) {
        std::cerr << "Failed to accept connection! Error: " << WSAGetLastError() << std::endl;
        closesocket(server_fd);
        WSACleanup();
        return 1;
    }

    std::cout << "\nSUCCESS! A client connected!" << std::endl;

    // 6. HANG UP (Windows uses closesocket instead of close)
    closesocket(user_socket);
    closesocket(server_fd);
    WSACleanup(); // Shut down the Winsock library

    std::cout << "Connection closed. Shutting down server." << std::endl;

    return 0;
}