#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <unordered_map>
#include <iomanip>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#pragma comment(lib, "Ws2_32.lib") 

namespace fs = std::filesystem;


//trims white space from both sides of string
std::string trim(const std::string& str)
{
    size_t first = str.find_first_not_of(" \t\r\n");
    if(first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

//Parses HTTP headers into an unordered_map
std::unordered_map<std::string, std::string> parseHeaders(const std::string& rawRequest)
{
    std::unordered_map<std::string, std::string> headers;

    //find where the first line ends
    size_t pos = rawRequest.find("\r\n");
    if(pos == std::string::npos) return headers;

    //start looking at the next line
    //because pos is where r lives so we
    //go to characters to the right
    size_t start = pos + 2;

    while(start < rawRequest.length())
    {
        //finds the next \r\n but begins from start
        size_t end = rawRequest.find("\r\n", start);
        if(end == std::string::npos) break;

        std::string line = rawRequest.substr(start, end - start);

        //if line empty that means the end of the header
        if(line.empty()) break;

        //Find colon seperating key and value
        size_t colonPos = line.find(":");
        if(colonPos != std::string::npos)
        {
            std::string key = line.substr(0, colonPos);
            std::string value = line.substr(colonPos + 1);

            //convert key to lowercase for case insensitive lookups
            for(char& c : key) c = std::tolower(c);

            headers[key] = trim(value);
        }

        start = end + 2; // move to next line;
    }

    return headers;
}

//Error handler for SSL_Write
int sslWriteError(const int& iresult, SSL* ssl, SOCKET sock)
{
    if (iresult <= 0)
    {
        int err = SSL_get_error(ssl, iresult);

        switch (err)
        {
            //clean close
            case SSL_ERROR_ZERO_RETURN:
                std::cout << "Connection closed cleanly by peer\n";
                SSL_shutdown(ssl);
                SSL_free(ssl);
                closesocket(sock);
                return 0;

            //Added since we might need to read
            //TLS records from peer first
            //either way retry needed
            //return 2 to let main loop
            //know socket still alive
            case SSL_ERROR_WANT_READ:
            case SSL_ERROR_WANT_WRITE:
                std::cout << "SSL_write needs retry\n";
                return 2;

            //-1 for failure
            default:
                std::cout << "SSL_write failed\n";
                ERR_print_errors_fp(stderr);
                return -1;
        }
    }
    //success
    return 1;
}

//decode routes with % instead of spaces
std::string urlDecode(const std::string& encoded)
{
    std::string decoded;

    //allocates memory upfront for effeciency
    decoded.reserve(encoded.length());

    for(int i = 0; i < encoded.length(); i++)
    {
        if(encoded[i] == '%')
        {
            //Check if there are at least 2 digits left
            if(i + 2 < encoded.length())
            {
                int hexValue;
                //gets the two hex digits after the %
                std::istringstream hexStream(encoded.substr(i + 1, 2));

                //read the hex value
                if(hexStream >> std::hex >> hexValue)
                {
                    //takes the val hex val which we converted into decimal
                    //which we then read into a char using ascii
                    decoded += static_cast<char>(hexValue);
                    i += 2; //skips the two chars we consumed
                }
                else
                {
                    decoded += '%'; //if parsing fails just keep the '%'
                }
            }
            else
            {
                decoded += '%';
            }
        }
        else if(encoded[i] == '+')
        {
            decoded += ' '; //some old encodings use '+' for space
        }
        else
        {
            decoded += encoded[i];
        }
    }

    return decoded;
}

//Checks the mime type of the filepath
std::string getMimeType(const std::string &filePath)
{
    // make a map of all the mime types
    static const std::unordered_map<std::string, std::string> mimeTypes = {
        {".html" , "text/html"},
        {".css",  "text/css"},
        {".js",   "application/javascript"},
        {".png",  "image/png"},
        {".jpg",  "image/jpeg"},
        {".jpeg", "image/jpeg"},
        {".svg",  "image/svg+xml"},
        {".ico",  "image/x-icon"}
    };

    //get the position of the last '.'
    size_t dotPos = filePath.find_last_of('.');
    if(dotPos != std::string::npos)
    {
        //this will get us the mime type
        std::string ext = filePath.substr(dotPos);

        //we find the mimetype in the map and return it
        auto it = mimeTypes.find(ext);
        if(it != mimeTypes.end())
        {
            return it->second;
        }
    }

    //default if no mime type found
    return "text/plain";
}

//function to run server
void runServer(SSL_CTX* ctx)
{
    int iresult;
    int writeStatus;

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
        return;
    }

    //load the certificate onto the ctx
    if(SSL_CTX_use_PrivateKey_file(ctx, "key.pem", SSL_FILETYPE_PEM) != 1)
    {
        std::cout << "Failed to load key.pem" << std::endl;
        return;
    }

    std::cout << "OpenSSL Context loaded succesfully with localhost certs" << std::endl;

    // Create SOCKET
    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    // Check for invalid socket
    if(sock == INVALID_SOCKET)
    {
        std::cout << "Socket function failed with error " << WSAGetLastError() << std::endl;
        return;
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
            break;
        }

        //adds new clients to masterlist
        if(FD_ISSET(sock, &set))
        {

            //The socket we are accepting a connection from
            SOCKET acceptSock = accept(sock, (sockaddr*) &clientService, &addrlen);
            if(acceptSock == INVALID_SOCKET)
            {
                std::cout << "Accept failed with error " << WSAGetLastError() << std::endl;
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
                return;
            }

            //Establishes secure encryption layer before
            //we send any bytes
            if(SSL_accept(ssl) != 1)
            {
                std::cout << "SSL Handshake failed" << std::endl;
                SSL_free(ssl);
                closesocket(acceptSock);
                continue;
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

                    //Allocates iresult bytes from buffer into new memory space
                    std::string rawRequest(buffer, iresult);

                    //pointes to the first byte of this new string in memory
                    std::istringstream requestStream(rawRequest);

                    //strings to store the method, path, and protocol
                    std::string method, path, protocol;

                    //>> skips white space so method takes method
                    //path the path and protocol the protocol
                    requestStream >> method >> path >> protocol;

                    //Parse the headers from the raw request;
                    std::unordered_map<std::string, std::string> headers = parseHeaders(rawRequest);

                    std::cout << "--- Parsed Headers ---" << std::endl;
                    if(headers.find("host") != headers.end()) 
                    {
                        std::cout << "Host: " << headers["host"] << std::endl;
                    }
                    if(headers.find("user-agent") != headers.end()) 
                    {
                        std::cout << "User-Agent: " << headers["user-agent"] << std::endl;
                    }
                    std::cout << "----------------------" << std::endl;

                    std::string route = path;
                    std::string queryStr = "";

                    //We check if there is a query string aka a string
                    //with a ? in between instead of a space
                    size_t qMarkPos = path.find("?");
                    if(qMarkPos != std::string::npos)
                    {
                        //everything before ?
                        route = path.substr(0, qMarkPos);
                        //everything after ?
                        queryStr = path.substr(qMarkPos + 1);
                    }

                    std::cout << "Browser requested route: " << route << std::endl;
                    if(!queryStr.empty())
                    {
                        std::cout << "With query parameters: " << queryStr << std::endl;
                    }

                    std::string decodedRoute = urlDecode(route);

                    std::string filePath;
                    std::string httpResponse;
                    if(method == "GET")
                    {
                        if(decodedRoute == "/")
                        {
                            //Go up to dist and and grab index.html
                            filePath = "../dist/index.html";
                        }
                        //incase we want to display live data
                        //coming from c++ memory
                        else if(decodedRoute == "/api/status")
                        {
                            std::string jsonStr = "{\"status\": \"online\", \"version\": \"1.0.0\"}";

                            httpResponse = "HTTP/1.1 200 OK\r\n"
                                        "Content-Type: application/json\r\n"
                                        "Content-Length: " + std::to_string(jsonStr.length()) + "\r\n"
                                        "Connection: Close\r\n\r\n" + jsonStr;
                        }
                        else
                        {
                            try
                            {
                                //Finds absolute real path to dist folder on hard drive
                                //canonical gives us the true path to dist
                                fs::path baseDir = fs::canonical("../dist");

                                //combine baseDir with whatever the user requested
                                fs::path requestedPath = baseDir / path.substr(1);

                                //strips away any ".." and any dirs before it
                                //This allows us to see any malicious users
                                //true target. any "." do nothing
                                fs::path resolvedPath = fs::weakly_canonical(requestedPath);

                                //Convert both path's to strings
                                std::string baseStr = baseDir.string();
                                std::string resolvedStr = resolvedPath.string();

                                //If the resolved path doesn't start with the base
                                //dir then it is a malicious file
                                //All file paths must start with the base dir
                                //Which in this case is dist
                                if(resolvedStr.find(baseStr) == 0)
                                {
                                    filePath = resolvedStr;
                                }
                                else
                                {
                                    std::cout << "WARNING: Path traversal attempt blocked for path: " << path << std::endl;

                                    //clear path so file reader fails
                                    filePath = "";

                                    httpResponse = "HTTP/1.1 403 Forbidden\r\n"
                                                "Content-Type: text/html\r\n"
                                                "Connection: close\r\n\r\n"
                                                "<h1>403 Forbidden</h1><p>Access denied.</p>";
                                                
                                    //send response to malicious user
                                    iresult = SSL_write(it->clientSSL, httpResponse.c_str(), httpResponse.length());
                                    writeStatus = sslWriteError(iresult, it->clientSSL, it->clientSock);
                                    if(writeStatus == -1 || writeStatus == 0)
                                    {   
                                        //fatal error or clean disconnect
                                        it = clients.erase(it);
                                        continue;
                                    }
                                    else if(writeStatus == 2)
                                    {
                                        //retry socket. Do not erase from master list
                                        continue;
                                    }
                                }
                            }
                            catch (const fs::filesystem_error& e) 
                            {
                                std::cout << "Filesystem error: " << e.what() << '\n';

                                httpResponse = "HTTP/1.1 500 Internal Server Error\r\n"
                                                "Content-Type: text/html\r\n"
                                                "Connection: close\r\n\r\n"
                                                "<h1>500 Internal Server Error</h1><p>An unexpected error occurred on the server.</p>";
                                                
                                //dir doesn't exist
                                iresult = SSL_write(it->clientSSL, httpResponse.c_str(), httpResponse.length());
                                writeStatus = sslWriteError(iresult, it->clientSSL, it->clientSock);
                                if(writeStatus == -1 || writeStatus == 0)
                                {   
                                    //fatal error or clean disconnect
                                    it = clients.erase(it);
                                    continue;
                                }
                                else if(writeStatus == 2)
                                {
                                    //retry socket. Do not erase from master list
                                    continue;
                                }
                            }
                        }
                    }
                    else
                    {
                        httpResponse = "HTTP/1.1 405 Method Not Allowed\r\n"
                                       "Content-Type: text/html\r\n"
                                       "Connection: close\r\n\r\n"
                                       "<h1>405 Method Not Allowed</h1><p>An unexpected error occurred on the server.</p>";
                        
                        iresult = SSL_write(it->clientSSL, httpResponse.c_str(), httpResponse.length());
                        writeStatus = sslWriteError(iresult, it->clientSSL, it->clientSock);
                        if (writeStatus == -1 || writeStatus == 0) 
                        {
                            it = clients.erase(it);
                        } 
                        else if(writeStatus == 1 || writeStatus == 2)
                        {
                            // If it succeeded, we still need to close and erase 
                            // because we don't want it falling through to the file reader
                            SSL_shutdown(it->clientSSL);
                            SSL_free(it->clientSSL);
                            closesocket(it->clientSock);
                            it = clients.erase(it);
                        }
                        continue; // prevents falling through logic
                        
                    }

                    //Getting path result and creating
                    //A response to hand off to SSL_write

                    //Attempt to grab a file from the hard drive
                    //Make it read the file in binary mode to prevent
                    //Windows from altering line endings and also set to end of file
                    std::ifstream file(filePath, std::ios::binary | std::ios::ate);

                    //If file doesn't open then 
                    //return nothing
                    if(file.is_open())
                    {
                        //manually get the total number of bytes in the file
                        //without manually having to seek to the end after opening
                        std::streamsize fileSize = file.tellg();

                        //Go back to starting pointer
                        file.seekg(0, std::ios::beg);

                        //display file content based on the files
                        //mime type
                        std::string mimeType = getMimeType(filePath);

                        //send header only
                        std::string headers = "HTTP/1.1 200 OK\r\n"
                                        "Content-Type: " + mimeType +  "\r\n"
                                        //Line required to tell browser where file ends
                                        "Content-Length: " + std::to_string(fileSize) + "\r\n"
                                        "Connection: Close\r\n\r\n";

                        //send data over to client side
                        iresult = SSL_write(it->clientSSL, headers.c_str(), headers.length());
                        writeStatus = sslWriteError(iresult, it->clientSSL, it->clientSock);
                        if(writeStatus == -1 || writeStatus == 0)
                        {   
                            //fatal error or clean disconnect
                            it = clients.erase(it);
                            continue;
                        }
                        else if(writeStatus == 2)
                        {
                            //retry socket. Do not erase from master list
                            continue;
                        }

                        //send file in 8KB chunks
                        char buffer[8192];
                        bool socketDied = false;
                        //attemps to read enough bytes to fill buffer
                        //OR if gcount > 0 then there is a final
                        //partial chunk of data that will be read
                        while(file.read(buffer, sizeof(buffer)) || file.gcount() > 0)
                        {
                            //send data over to client side
                            //use file.gcount to tell use exactly how many bytes were just read
                            iresult = SSL_write(it->clientSSL, buffer, file.gcount());
                            writeStatus = sslWriteError(iresult, it->clientSSL, it->clientSock);
                            if(writeStatus == -1 || writeStatus == 0) 
                            {   
                                socketDied = true;
                                break; // Breaks out of the while loop not the for loop
                            }
                            else if(writeStatus == 2)
                            {
                                continue;
                            }
                        }
                        if (socketDied) 
                        {
                            it = clients.erase(it);
                            continue;
                        }

                        file.close();
                    }
                    else
                    {
                        httpResponse = "HTTP/1.1 404 Not Found\r\n"
                                       "Content-Type: text/html\r\n"
                                       "Connection: close\r\n\r\n"
                                       "<h1>404 Error</h1><p>The file '" + filePath + "' does not exist</p>";
                    }

                    //send data over to client side
                    //make sure to convert httpResponse into a c style string or
                    //at use .data() to make it a pointer to a string of characters
                    iresult = SSL_write(it->clientSSL, httpResponse.c_str(), httpResponse.length());
                    writeStatus = sslWriteError(iresult, it->clientSSL, it->clientSock);\
                    if(writeStatus == -1 || writeStatus == 0)
                    {
                        it = clients.erase(it);
                        continue;
                    }
                    else if(writeStatus == 2)
                    {   
                        continue;
                    }

                    std::cout << "Bytes sent: " << iresult << std::endl; 
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

    //Emergency cleanup loop for active sessions when exiting while(true)
    for(auto& c : clients)
    {
        SSL_shutdown(c.clientSSL);
        SSL_free(c.clientSSL);
        closesocket(c.clientSock);
    }

    iresult = closesocket(sock);
    if(iresult != 0)
    {
        std::cout << "closesocket function failed with error " << WSAGetLastError() << std::endl;
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