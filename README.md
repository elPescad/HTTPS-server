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

## Future Roadmap: The C10k Leap
* **Next Phase:** Transition the current polling infrastructure to native Linux **Edge-Triggered `epoll`**. By shifting from linear descriptor arrays to a kernel-level event readiness queue, the Linux implementation will fully realize its C10k capabilities, handling tens of thousands of simultaneous connections with minimal CPU overhead.
