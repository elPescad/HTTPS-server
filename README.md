# Asynchronous Linux HTTPS Server (POSIX + OpenSSL)

This branch contains the Linux-native counter-part of the high-concurrency HTTPS project, leveraging standard POSIX BSD sockets, non-blocking file descriptors, and OpenSSL. It mirrors the exact logical state machine of the Windows counterpart but conforms entirely to Unix-system paradigms.

## Technical Deep-Dive & Architecture

### 1. POSIX Signal and File Descriptor Management
* Employs non-blocking socket configuration via `fcntl(fd, F_SETFL, O_NONBLOCK)`.
* Ensures structural durability by ignoring `SIGPIPE` signals, preventing the server process from crashing when a remote client abruptly severs a TLS session during active transmission.

### 2. Symmetrical OpenSSL Pipeline
Maintains identical logical parity with the Windows implementation’s non-blocking cryptographic architecture. It handles asynchronous TLS handshakes and partial data writes by accurately responding to OpenSSL’s event-driven error codes on a single execution thread.

### 3. Local Testing
```bash
# Compile with C++17 standard and OpenSSL linking
g++ -std=c++17 main.cpp -o server -lssl -lcrypto

# Run the server
./server
```

## Known Issues

A later review found bugs that aren't fixed yet. Each one is documented in **[KNOWN_ISSUES.md](KNOWN_ISSUES.md)** with its root cause, reproduction steps and a planned fix, and tagged in `server/main.cpp` as `KNOWN BUG #N`. The numbering is shared with the Windows build, so a few numbers only apply there. This build already uses level-triggered `epoll`; the roadmap item below is the move to edge-triggered mode. The most significant for this build:

* **#1, #2: One request can crash the server.** `GET /assets` (a directory) or a request line with no path throws an exception that is never caught.
* **#3: Requests with more than 1 KB of headers hang.** OpenSSL keeps the rest of the decrypted TLS record in its own buffer, so the kernel never reports the socket readable again. This also has to be fixed before moving to edge-triggered `epoll`.
* **#4: One client's OpenSSL error can kill other clients' connections.** The code never clears OpenSSL's per-thread error queue, so after one connection fails, a harmless `WANT_READ` on another connection that is already past its handshake is treated as fatal.
* **#6: Ctrl-Z then `fg` shuts the server down.** `epoll_wait()` fails with `EINTR` after the process is stopped and continued, and the loop treats that as fatal.
* **#9: Path traversal through a sibling directory.** The `dist/` check compares strings, not path components, so `/../dist-secret/` passes it.
* **#28, #29: The main C10k blockers.** Every request opens a new TCP connection and TLS handshake (`Connection: close`), and every new client's full handshake signs with an RSA-4096 key. Every `epoll_wait()` wakeup also scans every connection for timeouts, which undoes epoll's O(ready) advantage.

## Future Roadmap: The C10k Leap
* **Next Phase:** Transition the current polling infrastructure to native Linux **Edge-Triggered `epoll`**. By shifting from linear descriptor arrays to a kernel-level event readiness queue, the Linux implementation will fully realize its C10k capabilities, handling tens of thousands of simultaneous connections with minimal CPU overhead.
