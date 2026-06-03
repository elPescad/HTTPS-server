#include <iostream>
#include <string>
#include <list>
#include <chrono>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <unordered_map>
#include <iomanip>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

namespace fs = std::filesystem;
using namespace std::chrono_literals;


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
    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    // Check for invalid socket
    if(sock == -1)
    {
        std::cout << "Socket function failed with error " << strerror(errno) << std::endl;
        return;
    }
    else
    {
        std::cout << "Socket function succeeded" << std::endl;
    }
    
    //allows socket to bind to address that is in the TIME_WAIT state
    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));

    //this makes the socket non blocking
    //first call prevents overwriting the previous
    //existing flags. Second call sets those flags
    int flags = fcntl(sock, F_GETFL, 0);
    if(fcntl(sock, F_SETFL, flags | O_NONBLOCK) != 0)
    {
        std::cout << "fcntl failed with error " << strerror(errno) << std::endl;
        close(sock);
    }

    //creates new epoll instance
    //kernel creates an internal watch list of all I/O events
    //which in this case is the sockets
    int epfd = epoll_create1(0);
    if(epfd == -1)
    {
        std::cout << "epoll_create1 failed: " << strerror(errno) << std::endl;
        close(sock);
        return;
    }

    //start monitering a specific network socket event
    struct epoll_event ev;
    //asks to be notified if socket has data to be read
    //or disconnects
    ev.events = EPOLLIN;
    //once line above calls this hands us ev data
    //so we know which socket has incoming data
    ev.data.fd = sock;

    //tell kernel to watch for this
    epoll_ctl(epfd, EPOLL_CTL_ADD, sock, &ev);

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
        std::cout << "Bind failed with error " << strerror(errno) << std::endl;
        close(sock);
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
        std::cout << "Listen failed with error " << strerror(errno) << std::endl;
        close(sock);
        return;
    }

    std::cout << "Listening on socket" << std::endl;

    //struct for IPv4 but for accepting socket
    sockaddr_in clientService;
    socklen_t addrlen = sizeof(clientService);

    //holds the current state
    //of the client
    enum ClientState
    {
        STATE_HANDSHAKE,
        STATE_READ_REQUEST,
        STATE_WRITE_RESPONSE,
    };

    struct ClientConnection 
    {
        int clientSock;
        SSL* clientSSL;
        //default state
        ClientState state = STATE_HANDSHAKE;

        //asynchronous openSSL flag
        bool ioWantWrite = false;

        //dynamic storage for accumulation
        std::string readBuffer;
        std::string writeBuffer;
        size_t writeOffset = 0; //tracks how far into writebuffer we have sent

        //file streaming state
        std::ifstream fileStream;        
        std::streamsize fileBytesLeft = 0;

        //used to check last activity to prevent attacks and bots
        std::chrono::steady_clock::time_point lastActivity = std::chrono::steady_clock::now();
    };

    //Master list of all sockets
    std::list<ClientConnection> clients;

    //translates raw file descriptor into an iterator pointing specific client's
    //connection data stored inside a list since if we remove from the list
    //the other elements never move in memory
    std::unordered_map<int, std::list<ClientConnection>::iterator> fdMap;

    //dynamically adjust what epoll is waiting for
    //'[&]' means capture by reference
    auto epollUpdate = [&](std::list<ClientConnection>::iterator it, int op)
    {
        struct epoll_event ev;
        ev.data.fd = it->clientSock;

        //watch for out
        if(it->ioWantWrite)
        {
            ev.events = EPOLLOUT;
        }
        //watch for in
        else if(it->state == STATE_HANDSHAKE || it->state == STATE_READ_REQUEST)
        {
            ev.events = EPOLLIN;
        }
        //watch for out
        else if(it->state == STATE_WRITE_RESPONSE)
        {
            ev.events = EPOLLOUT;
        }
        //mutes socket
        else
        {
            ev.events = 0;
        }
        //takes ev and applies to kernel
        epoll_ctl(epfd, op, it->clientSock, &ev);
    };

    //Since the sockets are non blocking this does NOT send
    //chunks all at once. Rather it sends partial chunks.
    //This prevents other sockets from having to wait for
    //one socket to be serviced before the other is serviced.
    while(true)
    {
        //array of 64 epoll events
        //when sockets have data write down in this array
        struct epoll_event events[64];
        //puts thread to sleep so uses 0% CPU
        int n = epoll_wait(epfd, events, 64, 1000);
        if(n == -1)
        {
            std::cout << "epoll_wait failed " << strerror(errno) << std::endl;
            break;
        }

        //iterates through clients and checks their last activity
        //to prevent bot attacks or check for timeouts
        for(auto it = clients.begin(); it != clients.end();)
        {
            auto elapsed = std::chrono::steady_clock::now() - it->lastActivity;
            auto timeout = (it->state == STATE_WRITE_RESPONSE) ? 60s : 10s;
            if(elapsed > timeout)
            {
                std::cout << "Client timed out. Disconnecting.\n";
                epoll_ctl(epfd, EPOLL_CTL_DEL, it->clientSock, nullptr);
                fdMap.erase(it->clientSock);
                SSL_shutdown(it->clientSSL);
                SSL_free(it->clientSSL);
                close(it->clientSock);
                it = clients.erase(it);
                continue;
            }
            ++it;
        }

        //services the 64 or less sockets
        //if more need to be serviced we will return
        //on the next while pass
        for(int i = 0; i < n; i++)
        {
            //gets data
            int fd = events[i].data.fd;

            //if data is same as sock that means a new client
            //is waiting to be accepted. Else it's an existing client
            if(fd == sock)
            {
                //The socket we are accepting a connection from
                int acceptSock = accept(sock, (sockaddr*) &clientService, &addrlen);

                if(acceptSock == -1)
                {
                    std::cout << "Accept failed with error " << strerror(errno) << std::endl;
                    continue;
                }

                //this makes the socket non blocking
                //first call prevents overwriting the previous
                //existing flags
                int flags = fcntl(acceptSock, F_GETFL, 0);
                if(fcntl(acceptSock, F_SETFL, flags | O_NONBLOCK) != 0)
                {
                    std::cout << "ioctlsocket failed with error " << strerror(errno) << std::endl;
                    close(acceptSock);
                    continue;
                }

                //Creates ssl object
                SSL *ssl = SSL_new(ctx);

                //creates socket BIO to help bridge physical network
                //and the encrypted SSL/TLS session
                if(SSL_set_fd(ssl, acceptSock) != 1)
                {
                    std::cout << "SSL_set_fd failed" << std::endl;
                    SSL_free(ssl);
                    close(acceptSock);
                    continue;
                }

                //add new clients to clients
                clients.push_back({acceptSock, ssl});
                //get a pointer to new client we just added
                auto newIt = std::prev(clients.end());
                //map the raw socket num to the pointer
                fdMap[acceptSock] = newIt;

                //watch this new socket
                struct epoll_event ev;
                ev.events = EPOLLIN;
                ev.data.fd = acceptSock;
                epoll_ctl(epfd, EPOLL_CTL_ADD, acceptSock, &ev);
                continue;
            }

            //if fd not sock then find what fd is
            auto mapIt = fdMap.find(fd);
            if(mapIt == fdMap.end())
            {
                continue;
            }
            //gets data of it
            auto it = mapIt->second;

            //use bitwise operators to check for read and write
            bool readyToRead = events[i].events & EPOLLIN;
            bool readyToWrite = events[i].events & EPOLLOUT;

            //state handshake
            if(it->state == STATE_HANDSHAKE && (readyToRead || readyToWrite))
            {
                iresult = SSL_accept(it->clientSSL);
                //set read request
                if(iresult == 1)
                {
                    it->state = STATE_READ_REQUEST;
                    it->ioWantWrite = false;
                    it->lastActivity = std::chrono::steady_clock::now();
                    epollUpdate(it, EPOLL_CTL_MOD);
                    continue;
                }

                int err = SSL_get_error(it->clientSSL, iresult);
                //socket already accepted earlier wait for read
                if(err == SSL_ERROR_WANT_READ)
                {
                    it->ioWantWrite = false;
                    epollUpdate(it, EPOLL_CTL_MOD);
                }
                //socket already accepted earlier wait for write
                else if(err == SSL_ERROR_WANT_WRITE)
                {
                    it->ioWantWrite = true;
                    epollUpdate(it, EPOLL_CTL_MOD);
                }
                //actual error
                else
                {
                    ERR_print_errors_fp(stderr);
                    close(it->clientSock);
                    SSL_free(it->clientSSL);
                    epoll_ctl(epfd, EPOLL_CTL_DEL, it->clientSock, nullptr);
                    fdMap.erase(it->clientSock);
                    it = clients.erase(it);
                }
                continue;
            }            

           if(it->state == STATE_READ_REQUEST && readyToRead)
            {
                //buffer so that ram isn't overloaded
                //essentially makes it so we can only 
                //recieve 1024 bytes at a time
                //if more needed we can loop
                //This prevents crashes or from using
                //too much memory.
                char buffer[1024];

                //recieves the incoming data from the client
                iresult = SSL_read(it->clientSSL, buffer, sizeof(buffer));
                //data is recieved in bytes so if multiple bytes
                //there is data
                if(iresult > 0)
                {
                    //appends iresult bytes to buffer;
                    it->readBuffer.append(buffer, iresult);
                    it->lastActivity = std::chrono::steady_clock::now();

                    //no legit https header execeeds 8kb
                    if(it->readBuffer.size() > 8192)
                    {
                        // send 431 then close, rather than silent drop
                        std::string body = "<h1>431 Request Header Fields Too Large</h1>";
                        it->writeBuffer = "HTTP/1.1 431 Request Header Fields Too Large\r\n"
                                        "Content-Type: text/html\r\n"
                                        "Connection: close\r\n"
                                        "Content-Length: " + std::to_string(body.length()) + "\r\n"
                                        "Strict-Transport-Security: max-age=31536000\r\n"
                                        "X-Content-Type-Options: nosniff\r\n"
                                        "X-Frame-Options: DENY\r\n\r\n" + body;

                        it->writeOffset = 0;
                        it->state = STATE_WRITE_RESPONSE;
                        epollUpdate(it, EPOLL_CTL_MOD);
                        continue;
                    }

                    //checks to see if we have full header block
                    if(it->readBuffer.find("\r\n\r\n") == std::string::npos)
                    {
                        continue; //if not come back next select() iteration
                    }
                    //pointes to the first byte of this new string in memory
                    std::istringstream requestStream(it->readBuffer);

                    //strings to store the method, path, and protocol
                    std::string method, path, protocol;

                    //>> skips white space so method takes method
                    //path the path and protocol the protocol
                    requestStream >> method >> path >> protocol;

                    //Parse the headers from the raw request;
                    auto parsedHeaders = parseHeaders(it->readBuffer);
                    std::cout << "--- Parsed Headers ---" << std::endl;
                    if(parsedHeaders.count("host")) 
                    {
                        std::cout << "Host: " << parsedHeaders["host"] << std::endl;
                    }
                    if(parsedHeaders.find("user-agent") != parsedHeaders.end()) 
                    {
                        std::cout << "User-Agent: " << parsedHeaders["user-agent"] << std::endl;
                    }
                    std::cout << "----------------------" << std::endl;

                    std::string route = path;
                    std::string queryStr;

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

                    std::string decodedRoute = urlDecode(route);
                    std::cout << "Route: " << decodedRoute << std::endl;

                    if(method != "GET")
                    {
                        std::string body = "<h1>405 Method Not Allowed</h1>";
                        it->writeBuffer = "HTTP/1.1 405 Method Not Allowed\r\n"
                                          "Content-Type: text/html\r\n"
                                          "Connection: close\r\n"
                                          "Content-Length: " + std::to_string(body.length()) + "\r\n"
                                          "Strict-Transport-Security: max-age=31536000\r\n"
                                          "X-Content-Type-Options: nosniff\r\n"
                                          "X-Frame-Options: DENY\r\n\r\n" + body;
                    }
                    else if(decodedRoute == "/api/status")
                    {
                        std::string json = "{\"status\": \"online\", \"version\": \"1.0.0\"}";
                        it->writeBuffer = "HTTP/1.1 200 OK\r\n"
                                          "Content-Type: application/json\r\n"
                                          "Content-Length: " + std::to_string(json.length()) + "\r\n"
                                          "Connection: close\r\n"
                                          "Strict-Transport-Security: max-age=31536000\r\n"
                                          "X-Content-Type-Options: nosniff\r\n"
                                          "X-Frame-Options: DENY\r\n\r\n" + json;
                    }
                    else
                    {
                        //Resolve file path
                        std::string filePath;
                        if(decodedRoute == "/")
                        {
                            filePath = "../dist/index.html";
                        }
                        else
                        {
                            try
                            {
                                //Get true path
                                fs::path baseDir = fs::canonical("../dist");
                                //add paths to get requested route
                                fs::path requested = baseDir / decodedRoute.substr(1);
                                //removes any .. and . and shifts dirs accordingly
                                fs::path resolved = fs::weakly_canonical(requested);

                                std::string baseStr = baseDir.string();
                                std::string resolvedStr = resolved.string();

                                //if the path starts with /dist then continue
                                if(resolvedStr.find(baseStr) == 0)
                                {
                                    filePath = resolvedStr;
                                }
                                else
                                {
                                    std::cout << "WARNING: Path traversal blocked: " << path << "\n";
                                    std::string body = "<h1>403 Forbidden</h1>";
                                    it->writeBuffer = "HTTP/1.1 403 Forbidden\r\n"
                                                      "Content-Type: text/html\r\n"
                                                      "Connection: close\r\n"
                                                      "Content-Length: " + std::to_string(body.length()) + "\r\n"
                                                      "Strict-Transport-Security: max-age=31536000\r\n"
                                                      "X-Content-Type-Options: nosniff\r\n"
                                                      "X-Frame-Options: DENY\r\n\r\n" + body;
                                }
                            }
                            catch(const fs::filesystem_error& e)
                            {

                                std::cout << "Filesystem error: " << e.what() << "\n";
                                std::string body = "<h1>500 Internal Server Error</h1>";
                                it->writeBuffer = "HTTP/1.1 500 Internal Server Error\r\n"
                                                  "Content-Type: text/html\r\n"
                                                  "Connection: close\r\n"
                                                  "Content-Length: " + std::to_string(body.length()) + "\r\n"
                                                  "Strict-Transport-Security: max-age=31536000\r\n"
                                                  "X-Content-Type-Options: nosniff\r\n"
                                                  "X-Frame-Options: DENY\r\n\r\n" + body;
                            }
                        }

                        //Try to open file only if we have a valid path and no error yet
                        if(!filePath.empty() && it->writeBuffer.empty())
                        {
                            it->fileStream.open(filePath, std::ios::binary);
                            if(it->fileStream.is_open())
                            {
                                it->fileBytesLeft = fs::file_size(filePath);
                                std::string mime = getMimeType(filePath);

                                //writeBuffer holds only the https headers
                                //the body comes from fileStream in STATE_WRITE_RESPONSE
                                it->writeBuffer = "HTTP/1.1 200 OK\r\n"
                                                  "Content-Type: " + mime + "\r\n"
                                                  "Content-Length: " + std::to_string(it->fileBytesLeft) + "\r\n"
                                                  "Connection: close\r\n"
                                                  "Strict-Transport-Security: max-age=31536000\r\n"
                                                  "X-Content-Type-Options: nosniff\r\n"
                                                  "X-Frame-Options: DENY\r\n\r\n";
                            }
                            else
                            {
                                std::string body = "<h1>404 Not Found</h1><p>'" + filePath + "' does not exist.</p>";
                                it->writeBuffer = "HTTP/1.1 404 Not Found\r\n"
                                                  "Content-Type: text/html\r\n"
                                                  "Connection: close\r\n"
                                                  "Content-Length: " + std::to_string(body.length()) + "\r\n"
                                                  "Strict-Transport-Security: max-age=31536000\r\n"
                                                  "X-Content-Type-Options: nosniff\r\n"
                                                  "X-Frame-Options: DENY\r\n\r\n" + body;
                            }
                        }
                    }

                    //regardless of what was written
                    //change state to write response
                    //never write in this large block
                    it->writeOffset = 0;
                    it->lastActivity = std::chrono::steady_clock::now();
                    it->state = STATE_WRITE_RESPONSE;
                    epollUpdate(it, EPOLL_CTL_MOD);
                    continue;
                }
                else
                {
                    int err = SSL_get_error(it->clientSSL, iresult);
                    if(err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE)
                    {
                        continue;
                    }
                    //connection closed or real error
                    SSL_free(it->clientSSL);
                    epoll_ctl(epfd, EPOLL_CTL_DEL, it->clientSock, nullptr);
                    fdMap.erase(it->clientSock);
                    close(it->clientSock);
                    it = clients.erase(it);
                    continue;
                }
            }

            //drain writebuffer first then stream filestream if open
            //resumes from exactly where it left off on WANT_WRITE
            if(it->state == STATE_WRITE_RESPONSE && readyToWrite)
            {
                //drain the header/response string
                if(it->writeOffset < it->writeBuffer.size())
                {
                    //read only cannot change
                    //points to (starting char + offset) of writeBuffer
                    const char* ptr = it->writeBuffer.c_str() + it->writeOffset;
                    //convert to int due to size_t
                    int toSend = (int)(it->writeBuffer.size() - it->writeOffset);

                    iresult = SSL_write(it->clientSSL, ptr, toSend);
                    if(iresult > 0)
                    {
                        it->lastActivity   = std::chrono::steady_clock::now();
                        it->writeOffset += iresult;
                        it->ioWantWrite = false;
                    }
                    else
                    {
                        int err = SSL_get_error(it->clientSSL, iresult);
                        if(err == SSL_ERROR_WANT_WRITE || err == SSL_ERROR_WANT_READ)
                        {
                            it->ioWantWrite = true;
                            epollUpdate(it, EPOLL_CTL_MOD);
                        }
                        else
                        {
                            SSL_free(it->clientSSL);
                            epoll_ctl(epfd, EPOLL_CTL_DEL, it->clientSock, nullptr);
                            fdMap.erase(it->clientSock);
                            close(it->clientSock);
                            it = clients.erase(it);
                        }
                        continue;
                    }
                }

                //stream file body if there is one
                if(it->fileStream.is_open() && it->fileBytesLeft > 0)
                {
                    char fileBuf[8192];
                    //attempts to read all bytes
                    it->fileStream.read(fileBuf, sizeof(fileBuf));
                    std::streamsize bytesRead = it->fileStream.gcount();

                    if(bytesRead > 0)
                    {
                        iresult = SSL_write(it->clientSSL, fileBuf, (int)bytesRead);
                        if(iresult > 0)
                        {
                            it->lastActivity = std::chrono::steady_clock::now();
                            it->fileBytesLeft -= iresult;
                            it->ioWantWrite = false;

                            //if SSL didnt' consume all bytes we read, seek back
                            //so we re-send the remainder next iteration
                            if(iresult < bytesRead)
                            {
                                it->fileStream.seekg(iresult - bytesRead, std::ios::cur);
                                it->fileBytesLeft += (bytesRead - iresult);
                                it->ioWantWrite = true;
                                epollUpdate(it, EPOLL_CTL_MOD);
                            }
                        }
                        else
                        {
                            int err = SSL_get_error(it->clientSSL, iresult);
                            if(err == SSL_ERROR_WANT_WRITE || err == SSL_ERROR_WANT_READ)
                            {
                                it->fileStream.seekg(-bytesRead, std::ios::cur);
                                it->ioWantWrite = true;
                                epollUpdate(it, EPOLL_CTL_MOD);
                            }
                            else
                            {
                                SSL_free(it->clientSSL);
                                epoll_ctl(epfd, EPOLL_CTL_DEL, it->clientSock, nullptr);
                                fdMap.erase(it->clientSock);
                                close(it->clientSock);
                                it = clients.erase(it);
                            }
                            continue;
                        }
                    }
                    //Stay in write response until all file bytes are gone
                    if(it->fileBytesLeft > 0)
                    {
                        continue;
                    }
                }
                SSL_shutdown(it->clientSSL);
                SSL_free(it->clientSSL);
                epoll_ctl(epfd, EPOLL_CTL_DEL, it->clientSock, nullptr);
                fdMap.erase(it->clientSock);
                close(it->clientSock);
                clients.erase(it);
                continue;
            }
        }       
    }

    //Emergency cleanup loop for active sessions when exiting while(true)
    for(auto& c : clients)
    {
        SSL_shutdown(c.clientSSL);
        SSL_free(c.clientSSL);
        close(c.clientSock);
    }

    iresult = close(sock);
    if(iresult != 0)
    {
        std::cout << "closesocket function failed with error " << strerror(errno) << std::endl;
        return;
    }
}

int main() {
    //Using SSL_METHOD create SSL_CTX object
    //with which we will create all other
    //SSL objects
    const SSL_METHOD *method = TLS_server_method();
    SSL_CTX *ctx = SSL_CTX_new(method);
    if(!ctx)
    {
        std::cout << "Failed to create SSL_CTX" << std::endl;
        return 0;
    }

    std::cout << "Starting server" << std::endl;
    signal(SIGPIPE, SIG_IGN);
    runServer(ctx);

    //Close SSL_CTX global var
    SSL_CTX_free(ctx);

    // typically won't be called since server will be run for a while
    std::cout << "Yup we got it" << std::endl;
    return 1;
}