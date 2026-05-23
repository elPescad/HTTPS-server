#include <iostream>
#include <string>
#include <vector>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#pragma comment(lib, "Ws2_32.lib") 

//function to run server
void runServer(SSL_CTX* ctx)
{
    int iresult;

    /*!!!!!!IMPORTANT!!!!!!*
     * You absolutely have to pass both certificates
     * to the SSL_CTX object since this will be used to
     * create an SSL object for each individual socket
     * so that encryption between server and client can 
     * occur. Do not forget to do this ever
     */

    //load the certificate onto ctx
    if(SSL_CTX_use_certificate_file(ctx, "cert.pem", SSL_FILETYPE_PEM) != 1)
    {
        std::cout << "Failed to load cert.pem" << std::endl;
        SSL_CTX_free(ctx);
        WSACleanup();
        return;
    }

    //load the certificate onto the ctx
    if(SSL_CTX_use_PrivateKey_file(ctx, "key.pem", SSL_FILETYPE_PEM) != 1)
    {
        std::cout << "Failed to load key.pem" << std::endl;
        SSL_CTX_free(ctx);
        WSACleanup();
        return;
    }

    std::cout << "OpenSSL Context loaded succesfully with localhost certs" << std::endl;

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
        closesocket(sock);
        WSACleanup();
        return;
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
        closesocket(sock);
        WSACleanup();
        return;
    }

    std::cout << "Listening on socket" << std::endl;

    //struct for IPv4 but for accepting socket
    sockaddr_in clientService;
    int addrlen = sizeof(clientService);


    struct ClientConnection {
        SOCKET clientSock;
        SSL* clientSSL;
    };

    //Master list of all sockets
    std::vector<ClientConnection> clients;
    while(true)
    {

        //The set of sockets
        fd_set set;

        //resets the list
        FD_ZERO(&set);
        //adds the server socket
        FD_SET(sock, &set);

        //Adds all the current master list of sockets into
        //set
        for(ClientConnection c: clients)
        {
            FD_SET(c.clientSock, &set);
        }

        //checks which sockets are currently active
        //and filters down set to just those
        iresult = select(0, &set, nullptr, nullptr, nullptr);
        if(iresult > 0)
        {
            std::cout << "Select succesful" << std::endl;
        }
        else if(iresult == 0)
        {
            std::cout << "Time expired" << std::endl;
            continue;
        }
        else if(iresult == SOCKET_ERROR)
        {
            std::cout << "Select failed with error " << WSAGetLastError() << std::endl;
            return;
        }

        //adds new clients to masterlist
        if(FD_ISSET(sock, &set))
        {

            //The socket we are accepting a connection from
            SOCKET acceptSock = accept(sock, (sockaddr*) &clientService, &addrlen);
            if(acceptSock == INVALID_SOCKET)
            {
                std::cout << "Accept failed with error " << WSAGetLastError() << std::endl;
                WSACleanup();
                return;
            }

            //Creates ssl object
            SSL *ssl = SSL_new(ctx);

            //Here we link the plaintext socket
            //with the SSL object
            if(SSL_set_fd(ssl, acceptSock) != 1)
            {
                std::cout << "SSL_set_fd failed " << std::endl;
                closesocket(acceptSock);
                SSL_free(ssl);
                SSL_CTX_free(ctx);
                WSACleanup();
                return;
            }

            //Establishes secure encryption layer before
            //we send any bytes
            if(SSL_accept(ssl) != 1)
            {
                std::cout << "SSL Handshake failed" << std::endl;
            }

            std::cout << "SSL Handshake successful" << std::endl;
            std::cout << "Client connected" << std::endl;

            clients.push_back({acceptSock, ssl});
        }

        //handles existing clients
        for(auto it = clients.begin(); it != clients.end();)
        {
            if(FD_ISSET(it->clientSock, &set))
            {
                //buffer so that ram isn't overloaded
                //essentially makes it so we can only 
                //recieve 1024 bytes at a time
                //if more needed we can loop
                //This prevents crashes or from using
                //too much memory.
                char buffer[1024] = {0};

                //recieves the incoming data from the client
                iresult = SSL_read(it->clientSSL, buffer, sizeof(buffer));
                //data is recieved in bytes so if multiple bytes
                //there is data
                if(iresult > 0)
                {
                    std::cout << "Bytes recieved " << iresult << std::endl;
                    std::cout << "Here is what the browser said " << std::endl;
                    std::cout << buffer << std::endl;
                }
                //If no bytes data closed
                else if (iresult <= 0)
                {
                    int err = SSL_get_error(it->clientSSL, iresult);

                    switch (err)
                    {
                        case SSL_ERROR_ZERO_RETURN:
                            std::cout << "Connection closed cleanly by peer\n";
                            break;

                        //Added since we might need to read
                        //TLS records from peer first
                        //either way retry needed
                        case SSL_ERROR_WANT_WRITE:
                        case SSL_ERROR_WANT_READ:
                            std::cout << "SSL_read needs retry\n";
                            break;

                        default:
                            std::cout << "SSL_read failed\n";

                            ERR_print_errors_fp(stderr);
                            break;
                    }

                    SSL_shutdown(it->clientSSL);
                    SSL_free(it->clientSSL);
                    closesocket(it->clientSock);
                    it = clients.erase(it);
                    continue;
                }

                //Create a read only char pointer
                //make a const so it cannot be change accidentally
                //this is the data we will send to be displayed to the browser
                //temporary
                const char *data = "HTTP/1.1 200 OK\r\n"
                    "Content-Type: text/html\r\n"
                    "Connection: close\r\n"
                    "\r\n"
                    "<h1>Hello from the bare metal!</h1><p>You successfully built a socket server.</p>";

                //send data over to client side
                iresult = SSL_write(it->clientSSL, data, (int)strlen(data));
                if (iresult <= 0)
                {
                    int err = SSL_get_error(it->clientSSL, iresult);

                    switch (err)
                    {
                        case SSL_ERROR_ZERO_RETURN:
                            std::cout << "Connection closed cleanly by peer\n";
                            break;

                        //Added since we might need to read
                        //TLS records from peer first
                        //either way retry needed
                        case SSL_ERROR_WANT_READ:
                        case SSL_ERROR_WANT_WRITE:
                            std::cout << "SSL_write needs retry\n";
                            break;

                        default:
                            std::cout << "SSL_write failed\n";

                            ERR_print_errors_fp(stderr);
                            break;
                    }
                    SSL_shutdown(it->clientSSL);
                    SSL_free(it->clientSSL);
                    closesocket(it->clientSock);
                    it = clients.erase(it);
                    continue;
                }

                std::cout << "Bytes sent: " << iresult << std::endl; 

                //Close and free SSL objects and close sockets
                SSL_shutdown(it->clientSSL);
                SSL_free(it->clientSSL);
                iresult = closesocket(it->clientSock);
                if(iresult != 0)
                {
                    std::cout << "closesocket function failed with error " << WSAGetLastError() << std::endl;
                }
                //delete the current client from the masterlist
                it = clients.erase(it);
            }
            else
            {
                //iterate to the next client because this one was unresponsive
                ++it;
            }
        }
    }
    iresult = closesocket(sock);
    if(iresult != 0)
    {
        std::cout << "closesocket function failed with error " << WSAGetLastError() << std::endl;
        WSACleanup();
        return;
    }
}

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

    //Using SSL_METHOD create SSL_CTX object
    //with which we will create all other
    //SSL objects
    const SSL_METHOD *method = TLS_server_method();
    SSL_CTX *ctx = SSL_CTX_new(method);
    if(!ctx)
    {
        std::cout << "Failed to create SSL_CTX" << std::endl;
        WSACleanup();
        return 0;
    }

    std::cout << "Starting server" << std::endl;
    runServer(ctx);

    //Close SSL_CTX global var
    SSL_CTX_free(ctx);

    // typically won't be called since server will be run for a while
    // however is necessary to prevent memory leaks.
    std::cout << "Yup we got it" << std::endl;
    WSACleanup();
    return 1;
}