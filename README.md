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

### 4. Local Testing
```bash
# Compile with Winsock2 and OpenSSL linking
g++ main.cpp -o my_server -lws2_32 -lssl -lcrypto

# Run the server
./my_server.exe
```

## Known Issues

A later review found bugs that aren't fixed yet. Each one is documented in **[KNOWN_ISSUES.md](KNOWN_ISSUES.md)** with its root cause, reproduction steps and a planned fix, and tagged in `server/main.cpp` as `KNOWN BUG #N`. The numbering is shared with the Linux build, so a few numbers only apply there. The most significant for this build:

* **#2: One malformed request can crash the server.** A request line with no path makes `substr(1)` throw `std::out_of_range`, which is never caught. This contradicts the "cannot be crashed" claim in Memory-Cap Buffering above: the size cap works, but malformed content can still crash it.
* **#3: Requests with more than 1 KB of headers hang.** OpenSSL keeps the rest of the decrypted TLS record in its own buffer, so `select()` never reports the socket readable again. The 1024-byte chunking described above is what triggers it.
* **#4: One client's OpenSSL error can kill other clients' connections.** The code never clears OpenSSL's per-thread error queue, so after one connection fails, a harmless `WANT_READ` on another connection that is already past its handshake is treated as fatal.
* **#9: Path traversal through a sibling directory.** The `dist` check compares strings, not path components, so a request for `/../dist-secret/s.txt` (which resolves to `...\dist-secret\s.txt`) passes it.
* **#15: `SO_REUSEADDR` lets another process take over the port.** On Windows it lets another socket that also sets `SO_REUSEADDR` bind the same address and port while the server is running, so this build should use `SO_EXCLUSIVEADDRUSE`.
* **#19: The partial-write rollback described above never runs.** `SSL_write` never returns a partial count unless `SSL_MODE_ENABLE_PARTIAL_WRITE` is set, so only the `WANT_WRITE` rollback matters.
* **#25: Each `fd_set` holds at most 64 sockets.** `FD_SETSIZE` defaults to 64, so with the listener taking one slot only 63 handshaking or reading clients are polled per pass; `FD_SET` silently drops the rest, and they time out.

## Future Roadmap: The C10k Leap
The current iteration utilizes the multiplexed `select()` API for socket polling. While functionally robust, `select()` suffers from $O(N)$ linear scanning scaling limitations and a default Windows `FD_SETSIZE` cap. 
* **Next Phase:** Upgrade the event loop architecture from `select()` to Windows **IOCP (I/O Completion Ports)** to achieve true $O(1)$ kernel-level event notification and master the C10k concurrency limit.
