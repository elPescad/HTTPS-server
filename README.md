# Asynchronous Windows HTTPS Server (Winsock2 + OpenSSL)

This branch contains the Windows-native implementation of our high-concurrency HTTPS server. Built directly on top of the Winsock2 API and OpenSSL, this server manages multiplexed network connections through a single-threaded, non-blocking asynchronous state machine.

## Technical Deep-Dive & Architecture

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

## Future Roadmap: The C10k Leap
The current iteration utilizes the multiplexed `select()` API for socket polling. While functionally robust, `select()` suffers from $O(N)$ linear scanning scaling limitations and a default Windows `FD_SETSIZE` cap. 
* **Next Phase:** Upgrade the event loop architecture from `select()` to Windows **IOCP (I/O Completion Ports)** to achieve true $O(1)$ kernel-level event notification and master the C10k concurrency limit.
