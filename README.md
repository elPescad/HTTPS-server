# Dual-Platform Asynchronous HTTPS C10k Server Core

A deep-dive exploration into low-level systems programming, asynchronous network I/O, and cryptographic state tracking. This project consists of two distinct, from-scratch implementations of a high-concurrency HTTPS/1.1 server tailored for Windows (Winsock2) and Linux (POSIX BSD Sockets), utilizing raw OpenSSL for non-blocking TLS termination.

The goal of this project is to implement a robust, secure, single-threaded asynchronous state machine capable of scaling toward the C10k bottleneck without relying on high-level abstractions like Node.js, Go, or Rust's Tokio.

## Architecture Index

This repository is split across dedicated branches to isolate operating system paradigms and kernel-level event APIs:

* **[Linux Server](https://github.com/elPescad/HTTPS-server/blob/feat/Linux-Server/server/main.cpp)**
* **[Windows Server](https://github.com/elPescad/HTTPS-server/blob/feat/Windows-server/server/main.cpp)**

Regardless of the underlying OS flavor, both implementations enforce strict production-grade security constraints written purely in modern C++:
* **Directory Traversal Protection:** Mitigation of `../` attacks via compile-time and runtime filesystem resolution using `std::filesystem::canonical`.
* **DoS Mitigation:** Strict 8KB buffer limits on HTTP request headers to prevent memory exhaustion attacks, gracefully failing with an HTTP `431 Request Header Fields Too Large` response.
* **Hardened HTTP Headers:** Native injection of security headers including HTTP Strict Transport Security (HSTS), `X-Content-Type-Options: nosniff`, and `X-Frame-Options: DENY`.

# (WINDOWS BRANCH) Asynchronous Windows HTTPS Server (Winsock2 + OpenSSL)

This branch contains the Windows-native implementation of my high-concurrency HTTPS server. Built directly on top of the Winsock2 API and OpenSSL, this server manages multiplexed network connections through a single-threaded, non-blocking asynchronous state machine.

##  Technical Deep-Dive & Architecture

### 1. Non-Blocking TLS State Machine
Traditional blocking sockets stall the execution thread while waiting for network I/O. This implementation forces all client sockets into non-blocking mode, handling OpenSSL's volatile `SSL_ERROR_WANT_READ` and `SSL_ERROR_WANT_WRITE` signals seamlessly. The connection life cycle transitions through an explicit state machine:
`STATE_READ_REQUEST` -> `STATE_WRITE_RESPONSE` -> `STATE_DONE`

### 2. High-Precision Flow Control & Buffer Rollback
One of the primary challenges of asynchronous TLS writing is that `SSL_write` may not consume an entire file buffer if the kernel's network window fills up. This implementation solves that via precision stream seeking:
* Tracks exact bytes consumed by OpenSSL per loop iteration.
* Automatically calculates partial writes and rolls back the `std::ifstream` file pointer using `seekg()` relative to the unread delta.
* Resumes streaming seamlessly on subsequent event loop ticks without data corruption.

### 3. Memory-Cap Buffering
To maintain low RAM overhead, incoming socket data is drained in small 1024-byte chunks per loop iteration into a persistent session `readBuffer`, ensuring the server cannot be crashed by large, malformed network payloads.

### 4. Local Testing
```bash
# Compile with Winsock2 and OpenSSL linking
g++ main.cpp -o my_server -lws2_32 -lssl -lcrypto

# Run the server
./my_server.exe
```

## Future Roadmap: The C10k Leap
The current iteration utilizes the multiplexed `select()` API for socket polling. While functionally robust, `select()` suffers from $O(N)$ linear scanning scaling limitations and a default Windows `FD_SETSIZE` cap. 
* **Next Phase:** Upgrade the event loop architecture from `select()` to Windows **IOCP (I/O Completion Ports)** to achieve true $O(1)$ kernel-level event notification and master the C10k concurrency limit.

# (LINUX BRANCH) Asynchronous Linux HTTPS Server (POSIX + OpenSSL)

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
