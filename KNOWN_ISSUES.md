# Known Issues

These bugs were found in a later review of the server, after the first version was finished. None of them are fixed yet. Each one is tagged in the source with a `KNOWN BUG #N` comment, so `grep -n "KNOWN BUG" server/main.cpp` shows where it lives.

The numbering is shared between both builds:

* **Linux build** (`feat/Linux-Server`): POSIX sockets + level-triggered `epoll`.
* **Windows build** (`feat/Windows-server`, also on `main`): Winsock2 + `select()`.

Reproduction commands assume the Linux build is running from `server/` (it loads `cert.pem` and `../dist` relative to its working directory) on `127.0.0.1:8080`.

**Severity:** *High* means one request or a small number of connections can crash, hang or exploit the server, or it is a hard scaling limit. *Medium* means it breaks correctness or availability under realistic conditions. *Low* is an edge case or hygiene.

## Summary

| # | Bug | Build | Severity |
|---|---|---|---|
| 1 | [Requesting a directory crashes the server](#1-requesting-a-directory-crashes-the-server) | Linux | High |
| 2 | [A malformed request line crashes the server](#2-a-malformed-request-line-crashes-the-server) | Both | High |
| 3 | [Requests with more than 1 KB of headers hang until the timeout](#3-requests-with-more-than-1-kb-of-headers-hang-until-the-timeout) | Both | High |
| 4 | [One client's leftover OpenSSL error kills other clients' connections](#4-one-clients-leftover-openssl-error-kills-other-clients-connections) | Both | High |
| 5 | [The last chunk of a file is never re-sent after `WANT_WRITE`](#5-the-last-chunk-of-a-file-is-never-re-sent-after-want_write) | Both | Medium |
| 6 | [Suspending and resuming the process shuts the server down](#6-suspending-and-resuming-the-process-shuts-the-server-down) | Linux | Medium |
| 7 | [A failing `accept()` makes the loop spin at 100% CPU](#7-a-failing-accept-makes-the-loop-spin-at-100-cpu) | Both | Medium |
| 8 | [A file that shrinks during a download makes the loop spin](#8-a-file-that-shrinks-during-a-download-makes-the-loop-spin) | Both | Low |
| 9 | [Path traversal through a sibling directory](#9-path-traversal-through-a-sibling-directory) | Both | High |
| 10 | [The 404 page reflects the URL and the server's absolute path](#10-the-404-page-reflects-the-url-and-the-servers-absolute-path) | Both | Medium |
| 11 | [The URL decoder accepts malformed escapes and decodes `+` in paths](#11-the-url-decoder-accepts-malformed-escapes-and-decodes--in-paths) | Both | Medium |
| 12 | [Slowloris: a client can hold a connection open indefinitely](#12-slowloris-a-client-can-hold-a-connection-open-indefinitely) | Both | Medium |
| 13 | [No limit on concurrent connections](#13-no-limit-on-concurrent-connections) | Both | Medium |
| 14 | [TLS version and cipher policy aren't set in code](#14-tls-version-and-cipher-policy-arent-set-in-code) | Both | Low |
| 15 | [Windows: `SO_REUSEADDR` lets another process take over the port](#15-windows-so_reuseaddr-lets-another-process-take-over-the-port) | Windows | Medium |
| 16 | [The TLS private key is committed to the repository](#16-the-tls-private-key-is-committed-to-the-repository) | Repo | Low |
| 17 | [`SSL_write`'s `WANT_READ` waits for the wrong event](#17-ssl_writes-want_read-waits-for-the-wrong-event) | Both | Low |
| 18 | [`SSL_read`'s `WANT_WRITE` is ignored while reading the request](#18-ssl_reads-want_write-is-ignored-while-reading-the-request) | Both | Low |
| 19 | [`SSL_write` retry logic: dead partial-write branch, retry works by accident](#19-ssl_write-retry-logic-dead-partial-write-branch-retry-works-by-accident) | Both | Low |
| 20 | [The response can run past `Content-Length` if the file grows](#20-the-response-can-run-past-content-length-if-the-file-grows) | Both | Low |
| 21 | [Closing with unread data sends a TCP RST, which can truncate the response](#21-closing-with-unread-data-sends-a-tcp-rst-which-can-truncate-the-response) | Both | Low |
| 22 | [Every `open()` failure is reported as "404 does not exist"](#22-every-open-failure-is-reported-as-404-does-not-exist) | Both | Low |
| 23 | [`std::tolower` on a negative `char` is undefined behaviour](#23-stdtolower-on-a-negative-char-is-undefined-behaviour) | Both | Low |
| 24 | [A stale epoll event can be applied to a reused descriptor](#24-a-stale-epoll-event-can-be-applied-to-a-reused-descriptor) | Linux | Low |
| 25 | [Windows: only 64 sockets fit in an `fd_set`](#25-windows-only-64-sockets-fit-in-an-fd_set) | Windows | High |
| 26 | [Windows: error paths that stop the server or keep going when they shouldn't](#26-windows-error-paths-that-stop-the-server-or-keep-going-when-they-shouldnt) | Windows | Low |
| 27 | [Windows: finished connections stay open for up to a second](#27-windows-finished-connections-stay-open-for-up-to-a-second) | Windows | Low |
| 28 | [Every request opens a new TLS connection, and full handshakes use RSA-4096](#28-every-request-opens-a-new-tls-connection-and-full-handshakes-use-rsa-4096) | Both | High |
| 29 | [Every wakeup scans every connection for timeouts](#29-every-wakeup-scans-every-connection-for-timeouts) | Both | High |
| 30 | [Large files are sent one 8 KB chunk per event-loop pass](#30-large-files-are-sent-one-8-kb-chunk-per-event-loop-pass) | Both | Medium |
| 31 | [Disk reads block the event loop](#31-disk-reads-block-the-event-loop) | Both | Medium |
| 32 | [Single thread, one core](#32-single-thread-one-core) | Both | Medium |
| 33 | [One `accept()` per wakeup, plus extra syscalls per connection](#33-one-accept-per-wakeup-plus-extra-syscalls-per-connection) | Both | Low |
| 34 | [Synchronous logging in the hot path](#34-synchronous-logging-in-the-hot-path) | Both | Low |
| 35 | [Unchecked return values and uninitialised structs](#35-unchecked-return-values-and-uninitialised-structs) | Both | Low |
| 36 | [Some startup failures exit with status 0](#36-some-startup-failures-exit-with-status-0) | Both | Low |

---

## Crashes and stalls

### 1. Requesting a directory crashes the server
**Build:** Linux (Windows most likely returns a 404 instead, but has the same latent problem) · **Severity:** High

* **What happens:** `GET /assets`, or any other directory under `dist/`, kills the process.
* **Why:** On Linux, `open()` with `O_RDONLY` succeeds on a directory, so `std::ifstream` reports `is_open()`. `fs::file_size()` then throws `filesystem_error` ("Is a directory"). That call sits after the `try` block, so the exception reaches `std::terminate`. On Windows the C runtime opens files with `CreateFile` without `FILE_FLAG_BACKUP_SEMANTICS`, which is required for a directory handle. So `is_open()` should be false there, and the request gets the 404 page (not tested on Windows).
* **Reproduce:** `curl -k https://127.0.0.1:8080/assets` gets an empty reply, and the server log ends with `terminate called after throwing an instance of 'std::filesystem::__cxx11::filesystem_error' ... Is a directory`.
* **Fix:** Check `fs::is_regular_file()` before opening, and use the non-throwing `fs::file_size(path, ec)` overload.

### 2. A malformed request line crashes the server
**Build:** Both · **Severity:** High

* **What happens:** A request line with no path (`GET\r\n\r\n`) kills the process. On Linux, a one-character target such as `GET x HTTP/1.1` crashes it too.
* **Why:** `requestStream >> method >> path >> protocol` leaves `path` empty when the target is missing. `decodedRoute.substr(1)` on an empty string throws `std::out_of_range`, and the surrounding `catch` only handles `fs::filesystem_error`, so the exception reaches `std::terminate`. For `GET x`, `substr(1)` returns `""`, the path resolves to `dist/` itself, and the request hits bug #1.
* **Reproduce:**
  ```bash
  python3 -c "import ssl,socket;s=ssl._create_unverified_context().wrap_socket(socket.create_connection(('127.0.0.1',8080)));s.sendall(b'GET\r\n\r\n');print(s.recv(100))"
  ```
  The server log ends with `terminate called after throwing an instance of 'std::out_of_range'`.
* **Fix:** Reject any target that is empty or doesn't start with `/` with a 400, and catch `std::exception` around request handling.

#2 contradicts the claim in the main and Windows READMEs that the server "cannot be crashed by large, malformed network payloads", and #1 crashes the Linux build with a perfectly well-formed request. The 8 KB size cap works, but the content of a request can still crash the server.

### 3. Requests with more than 1 KB of headers hang until the timeout
**Build:** Both · **Severity:** High

* **What happens:** A request whose headers are bigger than 1024 bytes (a few cookies are enough) gets no response, and the connection is dropped after 10 s.
* **Why:** OpenSSL has to read a whole TLS record, up to 16 KB, and check its integrity tag before it can decrypt any of it. `SSL_read` copies 1024 bytes into `buffer` and keeps the rest of the plaintext inside the `SSL` object (`SSL_pending()`). The kernel's receive queue is now empty, so the socket never becomes readable again, and neither level-triggered epoll nor `select()` wakes this connection. The 1024-byte buffer described in the "Memory-Cap Buffering" section of the main and Windows READMEs is what triggers it.
* **Reproduce:** `curl -k https://127.0.0.1:8080/ -H "Cookie: $(head -c 2000 /dev/zero | tr '\0' a)"` hangs for about 10 s and gets nothing. With `char buffer[16384]` the same request returns 200, and a 10 KB header correctly returns 431.
* **Fix:** Keep calling `SSL_read` until it returns `SSL_ERROR_WANT_READ` (or loop while `SSL_pending() > 0`) before going back to `epoll_wait()` or `select()`. This has to be fixed before switching to edge-triggered epoll, where every socket must be drained.

### 4. One client's leftover OpenSSL error kills other clients' connections
**Build:** Both · **Severity:** High

* **What happens:** If one connection fails or times out while another has already finished its TLS handshake, that second connection is dropped with no response the next time its `SSL_read()` needs more data (for example when its request arrives in more than one TCP segment). The window lasts until the next `SSL_accept()` on any connection.
* **Why:** OpenSSL keeps one error queue per thread, and every connection shares this server's single thread. `SSL_get_error()` checks that queue first and returns `SSL_ERROR_SSL` (or `SSL_ERROR_SYSCALL` for a system error) if anything is in it, even when the call only needed a retry (`WANT_READ`/`WANT_WRITE`). The code never calls `ERR_clear_error()`, and two common paths leave entries behind:
  * The timeout loop calls `SSL_shutdown()` on connections that never finished the handshake. That leaves `SSL_R_UNINITIALIZED` in the queue if `SSL_accept()` never ran (a client that sent nothing), or `SSL_R_SHUTDOWN_WHILE_IN_INIT` if the handshake had started.
  * A client that closes without sending `close_notify` leaves "unexpected eof while reading".

  The next legitimate `WANT_READ` is then treated as fatal. Only the next `SSL_accept()` clears the queue, because OpenSSL's handshake code calls `ERR_clear_error()` itself; that's why a client that connects after the failure is not affected.
* **Reproduce:**
  1. Open a plain TCP connection that sends nothing (`nc 127.0.0.1 8080`).
  2. About 8 s later, connect a TLS client (for example Python with `ssl.MemoryBIO`) and finish the handshake, but don't send the request yet.
  3. After the server times out the idle connection (10 to 11 s after step 1), send the request record in two TCP writes 0.3 s apart. It gets no response.

  With the same timing and no idle connection, the request gets 200. Adding `ERR_clear_error()` before `SSL_read` fixes it.
* **Fix:** Call `ERR_clear_error()` before every `SSL_accept`/`SSL_read`/`SSL_write`/`SSL_shutdown`, drain the queue on every error path, and only call `SSL_shutdown()` once `SSL_is_init_finished()` is true.

### 5. The last chunk of a file is never re-sent after `WANT_WRITE`
**Build:** Both · **Severity:** Medium

* **What happens:** If the socket's send buffer is full when the final, shorter chunk of a file is written, the client never gets the end of the file. The server spins at 100% CPU until the 60 s timeout.
* **Why:**
  1. The final `fileStream.read()` returns fewer than 8192 bytes, which sets both `eofbit` and `failbit`.
  2. On `WANT_WRITE` the code rewinds with `seekg(-bytesRead, cur)`. Since C++11, `seekg` clears `eofbit` but does nothing while `failbit` is set.
  3. The retry reads 0 bytes, skips `SSL_write`, and `continue`s with `fileBytesLeft > 0`.
  4. The socket is still writable, so level-triggered epoll (or `select()`) reports it again immediately, and the loop repeats.

  Chunks before the last one are full 8192-byte reads, so they don't set `failbit` and aren't affected.
* **Reproduce:** Serve a file whose size isn't a multiple of 8192, and force one `write()` of the final TLS record to return `EAGAIN` (an `LD_PRELOAD` shim works). curl stalls at the last 8 KB boundary and the server pins a core.
* **Fix:** Don't re-read the file on retry. Keep the unsent chunk in a per-connection buffer and retry `SSL_write` with the same pointer and length, which OpenSSL requires anyway (see #19). At minimum, call `fileStream.clear()` before `seekg()`.

### 6. Suspending and resuming the process shuts the server down
**Build:** Linux · **Severity:** Medium

* **What happens:** After Ctrl-Z then `fg` (SIGTSTP then SIGCONT; `kill -STOP` then `kill -CONT` does the same), the server prints `epoll_wait failed Interrupted system call` and exits.
* **Why:** On Linux, `epoll_wait()` fails with `EINTR` after the process is stopped and continued, even when no signal handlers are installed (documented in `man 7 signal`). The loop treats every `-1` as fatal and `break`s.
* **Reproduce:** `kill -STOP <pid>; kill -CONT <pid>`
* **Fix:** `if (errno == EINTR) continue;`. Other blocking calls need the same handling once signal handlers are added (for example for a graceful SIGTERM shutdown).

### 7. A failing `accept()` makes the loop spin at 100% CPU
**Build:** Both (worse on Windows) · **Severity:** Medium

* **What happens:** When the process runs out of file descriptors, the server burns a full core printing `Accept failed`. In testing that was about 2.5 million lines in a few seconds.
* **Why:** `accept()` fails with `EMFILE`, so the pending connection stays in the kernel's accept queue. The listening socket stays readable, so level-triggered epoll reports it again immediately. There's no back-off, and no spare descriptor to accept the connection and close it. On Windows it's worse: the `continue` in the accept error path skips the whole client loop, so no other client is served or timed out, and no descriptor is ever freed.
* **Reproduce:** Run `(ulimit -n 20; ./server)`, then open about 40 TCP connections.
* **Fix:** On `EMFILE`/`ENFILE`, stop watching the listener for a moment, or keep one spare fd open: close it, accept the connection, close that, then reopen the spare. On Windows, don't skip the client loop when accept fails.

### 8. A file that shrinks during a download makes the loop spin
**Build:** Both · **Severity:** Low

* **What happens:** If a file is truncated while it's being sent, the server spins at about 100% CPU on that connection until the 60 s timeout.
* **Why:** `fileBytesLeft` comes from `fs::file_size()` once, when the request is parsed. After the file shrinks, `read()` returns 0 bytes, so `fileBytesLeft` never reaches 0 and the writable socket keeps waking the loop. `lastActivity` is only updated when bytes are sent, so only the 60 s timeout ends it.
* **Reproduce:** Download a large file with a slow reader and run `truncate -s 0` on it mid-transfer.
* **Fix:** Treat `read() == 0` while `fileBytesLeft > 0` as an error and close the connection.

---

## Security

### 9. Path traversal through a sibling directory
**Build:** Both · **Severity:** High

* **What happens:** `/../dist-secret/s.txt` is served whenever a directory whose name starts with `dist` sits next to `dist/`.
* **Why:** `resolvedStr.find(baseStr) == 0` compares strings, not path components: `/app/dist-secret/s.txt` starts with `/app/dist`. Also, the path resolution only happens at runtime (`std::filesystem`), not at compile time as the main README says.
* **Reproduce:** With a `dist-secret/` directory containing `s.txt` next to `dist/`, run `curl -k --path-as-is https://127.0.0.1:8080/../dist-secret/s.txt`.
* **Fix:** Compare whole path components (for example `std::mismatch` over the two `fs::path`s), or require the prefix to be followed by a `/`. On Linux, `openat2()` with `RESOLVE_BENEATH` (kernel 5.6+) makes the kernel enforce it. That also closes the race between checking the path and opening it, for example a symlink swapped in between.

### 10. The 404 page reflects the URL and the server's absolute path
**Build:** Both · **Severity:** Medium

* **What happens:** `/<script>alert(1)</script>` comes back unescaped inside a `text/html` 404 page (reflected XSS), along with the absolute path of `dist/` on the server.
* **Why:** The 404 body is built from `filePath` with no HTML escaping.
* **Reproduce:** `curl -k 'https://127.0.0.1:8080/%3Cscript%3Ealert(1)%3C%2Fscript%3E'`
* **Fix:** Return a fixed 404 body (or HTML-escape the route), and never include server paths in a response.

### 11. The URL decoder accepts malformed escapes and decodes `+` in paths
**Build:** Both · **Severity:** Medium

* **What happens:**
  * `/index.html%00.png` serves `index.html`.
  * Invalid escapes such as `%4G`, `%-1` and `%+1` are decoded instead of rejected.
  * A file named `a+b.txt` can't be served from its plain URL: `/a+b.txt` returns 404, and only `/a%2Bb.txt` works.
* **Why:**
  * Each escape is parsed with `istringstream >> std::hex`, which accepts a sign and stops at the first non-hex character. The loop then always skips two characters.
  * A decoded `%00` stays inside the `std::string`, but the C-string system calls underneath stop at the NUL, so the path is cut short there.
  * Mapping `+` to a space is a rule for form-encoded query strings, not for paths (RFC 3986).
* **Reproduce:** `curl -k https://127.0.0.1:8080/index.html%00.png` returns 200 with the HTML page.
* **Fix:** Require two `isxdigit()` characters per escape, return 400 for bad escapes or decoded bytes below 0x20, and don't map `+` to a space in the path.

### 12. Slowloris: a client can hold a connection open indefinitely
**Build:** Both · **Severity:** Medium

* **What happens:** A client that sends one byte every few seconds stays in the request-reading state long after the 10 s timeout. Only the 8 KB header cap limits it.
* **Why:** `lastActivity` is reset on every `SSL_read` that returns data. The timeout is a sliding idle timer, not a deadline for receiving the whole header.
* **Reproduce:** A TLS client that sends `GET /\r\n\r\n` one byte every 4 s stays connected for 32 s, well past the 10 s timeout, and still gets a 200.
* **Fix:** Record when the request started, and enforce an absolute deadline for the full header block.

### 13. No limit on concurrent connections
**Build:** Both · **Severity:** Medium

* **What happens:** Every accepted connection is added to `clients` with no cap. Each TLS connection costs about 25 KB of memory (500 handshakes grew RSS by 12.5 MB), and each one makes the timeout scan in #29 slower.
* **Fix:** Enforce a maximum at `accept()`, and reject connections beyond it with 503 or by closing them.

### 14. TLS version and cipher policy aren't set in code
**Build:** Both · **Severity:** Low

* **What happens:** The minimum TLS version and the cipher list come entirely from the linked OpenSSL and the system's `openssl.cnf`. OpenSSL 3 defaults to TLS 1.2+, but with a permissive config the same binary negotiated TLS 1.0 with a CBC cipher.
* **Fix:** Call `SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION)` and set an explicit AEAD-only cipher list.

### 15. Windows: `SO_REUSEADDR` lets another process take over the port
**Build:** Windows · **Severity:** Medium

* **What happens:** On Linux, `SO_REUSEADDR` only allows rebinding while old connections are in `TIME_WAIT`. On Windows it lets another socket that also sets it bind the same address and port while the server is still running. Microsoft documents that incoming connections are then delivered to either socket unpredictably.
* **Fix:** Use `SO_EXCLUSIVEADDRUSE` on Windows, and keep `SO_REUSEADDR` on Linux.

### 16. The TLS private key is committed to the repository
**Build:** Repo · **Severity:** Low (it's a self-signed `localhost` certificate)

* **What happens:** `server/key.pem` is in git history, along with compiled binaries (`my_server.exe`, `my_server`).
* **Fix:** Generate the key locally (a script or a README step), add `*.pem` and the binaries to `.gitignore`, and remove them from history.

---

## TLS and HTTP correctness

### 17. `SSL_write`'s `WANT_READ` waits for the wrong event
**Build:** Both · **Severity:** Low (the trigger is rare)

* **Why:** `SSL_ERROR_WANT_READ` from `SSL_write` is handled like `WANT_WRITE`, so the code waits for the socket to be *writable*. A connected socket is almost always writable, so if this ever happens the loop retries and spins until inbound data arrives. TLS 1.3 removed renegotiation, the usual trigger. What remains is something like a peer's `KeyUpdate` message arriving split across TCP segments in the middle of a write.
* **Fix:** On `WANT_READ`, wait for readability (`EPOLLIN`, or `read_set` on Windows); on `WANT_WRITE`, wait for writability (`EPOLLOUT`, or `write_set`).

### 18. `SSL_read`'s `WANT_WRITE` is ignored while reading the request
**Build:** Both · **Severity:** Low (the trigger is rare)

* **Why:** In `STATE_READ_REQUEST`, `SSL_ERROR_WANT_WRITE` gets the same bare `continue` as `WANT_READ`, so the connection keeps waiting for readability. Suppose OpenSSL needs to send something (for example a `KeyUpdate` reply) while the send buffer is full, and the client is waiting for that before it sends more. Then the connection stalls until the 10 s timeout.
* **Fix:** On `WANT_WRITE`, set `ioWantWrite` so the socket is watched for writability (`EPOLLOUT` on Linux, `write_set` on Windows).

### 19. `SSL_write` retry logic: dead partial-write branch, retry works by accident
**Build:** Both · **Severity:** Low

* **Why:** Two separate problems:
  * **The partial-write branch never runs.** Unless `SSL_MODE_ENABLE_PARTIAL_WRITE` is set, `SSL_write` returns either the full length or an error, never a partial count. The 8 KB chunks also fit in one 16 KB TLS record. So the `iresult < bytesRead` branch is dead code, which 300 non-blocking writes against a tiny send buffer confirmed. The "partial write rollback" described in the main and Windows READMEs is really the `WANT_WRITE` rollback.
  * **The retry only works by accident.** That `WANT_WRITE` path re-reads the same bytes into the same stack array before retrying. OpenSSL requires a retry with the same pointer and length, or it fails with `SSL_R_BAD_WRITE_RETRY`. The retry works only because `fileBuf` ends up at the same stack address on every iteration.
* **Fix:** Delete the dead branch, and keep the pending chunk in a per-connection buffer, which also fixes #5. Alternatively, set `SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER`.

### 20. The response can run past `Content-Length` if the file grows
**Build:** Both · **Severity:** Low

* **Why:** `Content-Length` comes from `fs::file_size()`, looked up by path. Each chunk then reads a full 8192 bytes instead of `min(8192, fileBytesLeft)`. If the file grows during the download, up to 8191 extra bytes are sent, which breaks HTTP framing. In testing, 4,864 bytes went past a 20,000,000-byte `Content-Length`.
* **Fix:** Read `min(sizeof fileBuf, fileBytesLeft)`, and take the size from `fstat()` on the file that was actually opened.

### 21. Closing with unread data sends a TCP RST, which can truncate the response
**Build:** Both · **Severity:** Low

* **Why:** The server stops reading at the first `\r\n\r\n`. If more data is still in the socket's receive queue when it calls `close()` (a request body, or a second pipelined request in its own TLS record), Linux sends an RST instead of a FIN and discards whatever part of the response is still in the server's send buffer, so the client gets a truncated body followed by `ECONNRESET`: a 9.8 MB download was cut to 7.2 MB. (Some client TCP stacks also drop data they received but the application hasn't read yet when the RST arrives.) Separately, `SSL_shutdown()` is called once on a non-blocking socket and never retried.
* **Fix:** Drain the receiving side before closing (a "lingering close", as nginx does), and retry `SSL_shutdown()` on `WANT_READ`/`WANT_WRITE`.

### 22. Every `open()` failure is reported as "404 does not exist"
**Build:** Both · **Severity:** Low

* **Why:** The code never checks `errno` after a failed open. Running out of descriptors (`EMFILE`) or a permission error (`EACCES`) returns a 404 for a file that exists, and caches may keep that 404.
* **Fix:** Map `ENOENT` to 404, `EACCES` to 403, and `EMFILE`/`ENFILE` to 503.

### 23. `std::tolower` on a negative `char` is undefined behaviour
**Build:** Both · **Severity:** Low

* **Why:** `std::tolower` is only defined for `unsigned char` values and `EOF`. Plain `char` is signed on x86 and with MSVC, so a header-name byte of 0x80 or above becomes a negative `int`. glibc tolerates it (its tables accept -128 to 255), and so does the MSVC runtime in the default "C" locale, but it is still undefined, and a C library that indexes a 256-entry table with it reads out of bounds. On Linux/arm64 plain `char` is unsigned, so this one depends on the platform.
* **Fix:** `std::tolower(static_cast<unsigned char>(c))`.

### 24. A stale epoll event can be applied to a reused descriptor
**Build:** Linux · **Severity:** Low (theoretical)

* **Why:** `epoll_wait()` returns a snapshot of events. The timeout scan runs before those events are processed and can close a client's fd. `accept()` in the same batch can then get that same fd number back, because the kernel always hands out the lowest free one. A later event in the batch for the old fd is then looked up in `fdMap` and applied to the new connection. The state machine limits the damage to at most a pointless `SSL_accept()`, and this can't be triggered on demand.
* **Fix:** Process events before reaping timeouts, or tag each registration with a per-connection generation number. nginx stores an "instance" bit in its event pointer for this.

---

## Windows (`select`) build

### 25. Windows: only 64 sockets fit in an `fd_set`
**Build:** Windows · **Severity:** High

* **Why:** On Windows, `fd_set` is an array of 64 socket handles, not a bitmap (`FD_SETSIZE` defaults to 64), and `FD_SET` silently does nothing once it's full. The listener takes one slot, so only 63 handshaking or reading clients are polled on each pass. Clients further down the list never make progress and get dropped by the timeout. Purely idle connections are reaped after 10 s, so on their own they only delay others; but combined with #12, about 63 connections that finish the handshake and then trickle a byte every few seconds lock everyone else out. `FD_SET` and `FD_ISSET` both scan the array linearly, so each pass costs O(N × 64).
* **Fix:** Use `WSAPoll()` or I/O completion ports (IOCP). At minimum, `#define FD_SETSIZE` to a larger value before including `winsock2.h`, and cap connections.

### 26. Windows: error paths that stop the server or keep going when they shouldn't
**Build:** Windows (the `SSL_new` part affects both) · **Severity:** Low

* **Why:**
  * If `SSL_set_fd()` fails for one client, the code `return`s from `runServer()` and the whole server stops. The Linux build uses `continue`.
  * If `ioctlsocket()` fails on the listening socket, the socket is closed but execution carries on. `bind()` then fails with a misleading error, and `closesocket()` is called twice on the same handle.
  * The result of `SSL_new()` isn't checked for `NULL` in either build.
* **Fix:** Use `continue` instead of `return`, `return` after closing the listener, and check `SSL_new()` for `NULL`.

### 27. Windows: finished connections stay open for up to a second
**Build:** Windows · **Severity:** Low

* **Why:** When a response is finished, the connection moves to `STATE_DONE`, but a `STATE_DONE` socket isn't added to either `fd_set`. Its `SSL_shutdown()`/`closesocket()` only runs when `select()` wakes up for something else or its 1 s timeout expires. On a POSIX port of this loop, the EOF arrived about 1000 ms after the body, versus about 0.5 ms on the Linux build, which closes right away.
* **Fix:** Close the connection right after the last write, as the Linux build does.

---

## Scalability: what stands between this server and C10k

### 28. Every request opens a new TLS connection, and full handshakes use RSA-4096
**Build:** Both · **Severity:** High (throughput)

* **Why:** Every response is `Connection: close`, so a browser opens a new TCP connection and does a new TLS handshake for every asset. OpenSSL's default session tickets let a returning client resume without a certificate signature, but every new client (or one whose ticket expired) does a full handshake, and that signs with an RSA-4096 key. On a 4-vCPU x86 VM, `openssl speed` measured about 200 RSA-4096 signatures per second per core, against roughly 30,000 to 40,000 for ECDSA P-256, so full handshakes alone cap the server at a few hundred new clients per second.
* **Fix:** Support HTTP keep-alive and switch to an ECDSA P-256 certificate. Session resumption already works with OpenSSL's defaults, so keep it on (and share the ticket key if the server ever runs as several processes).

### 29. Every wakeup scans every connection for timeouts
**Build:** Both · **Severity:** High (scalability)

* **Why:** The timeout loop walks the whole `clients` list after every `epoll_wait()`/`select()` return, so each wakeup costs O(N) however few sockets are ready. That cancels out epoll's advantage of only reporting the ready sockets. `std::list` is node-based, so the scan is also one likely cache miss per connection.
* **Fix:** There are only two timeout lengths (10 s and 60 s), so keep one list per length, each ordered by last activity. On activity, `splice()` the connection to the back of its list in O(1) (moving it to the other list when its state changes), and expire connections from the front of each list. Or use a timer wheel or min-heap. Either way, pass the next deadline as the `epoll_wait()` timeout instead of a fixed 1 s.

### 30. Large files are sent one 8 KB chunk per event-loop pass
**Build:** Both · **Severity:** Medium

* **Why:** After one `read()` and one `SSL_write()`, the code goes back to `epoll_wait()` instead of writing until the socket is full. A 20 MB download took 2,450 `epoll_wait()` calls under `strace`, and each of those also paid for #29.
* **Fix:** Keep writing until `SSL_write` returns `WANT_WRITE` or the file is finished. Longer term, kernel TLS (`SSL_OP_ENABLE_KTLS` + `SSL_sendfile()`) lets the kernel encrypt straight from the page cache, without copying the file through user space.

### 31. Disk reads block the event loop
**Build:** Both · **Severity:** Medium

* **Why:** `std::ifstream::read()` blocks, and epoll can't watch regular files (`epoll_ctl` returns `EPERM`). When a read misses the page cache, every connection waits for the disk.
* **Fix:** Use `sendfile()` with kernel TLS, a small thread pool for file reads, or `io_uring`.

### 32. Single thread, one core
**Build:** Both · **Severity:** Medium

* **Fix:** Run one event loop per core, each with its own `SO_REUSEPORT` listener. The kernel then balances new connections across them with no shared state and no thundering herd. The alternative is a shared listener registered with `EPOLLEXCLUSIVE`. Pin each thread to the core that handles the matching NIC receive queue, for cache locality.

### 33. One `accept()` per wakeup, plus extra syscalls per connection
**Build:** Both · **Severity:** Low

* **Why:** Each time the listener is reported ready, the code accepts a single connection. It then makes it non-blocking with two `fcntl()` calls (or `ioctlsocket()` on Windows).
* **Fix:** On Linux, call `accept4(sock, ..., SOCK_NONBLOCK | SOCK_CLOEXEC)` in a loop until `EAGAIN`.

### 34. Synchronous logging in the hot path
**Build:** Both · **Severity:** Low

* **Why:** Every request prints several lines with `std::endl`, which flushes the stream, so each line is a blocking `write()` on the event-loop thread. The Windows build also prints "Select succesful" on every `select()` wakeup, which is once per 8 KB chunk during a download.
* **Fix:** Remove the debug prints, use `'\n'` instead of `std::endl`, and buffer the logs or write them from another thread.

---

## Minor

### 35. Unchecked return values and uninitialised structs
**Build:** Both (the `epoll`/`fcntl` items are Linux only) · **Severity:** Low

* **Why:**
  * The results of `epoll_ctl()`, `setsockopt()` and `fcntl(F_GETFL)` are ignored. For example, a failed `EPOLL_CTL_ADD` leaves a client in the list but never polled.
  * `sockaddr_in` and `epoll_event` aren't zero-initialised.
  * The handshake-error path calls `close()` before `EPOLL_CTL_DEL`, so the DEL fails with `EBADF`. That's harmless only because closing the last reference to the socket already removes it from epoll (epoll tracks the open file, not the fd number).
  * `epfd` is never closed.
* **Fix:** Check every call, write `sockaddr_in service{};`, and remove a socket from epoll before closing it.

### 36. Some startup failures exit with status 0
**Build:** Both · **Severity:** Low

* **Why:** `main()` returns 0 (success) when `SSL_CTX_new()` fails, and on Windows also when the Winsock version check fails. Every other failure, such as a missing `cert.pem` or a port that is already in use, falls through to `return 1` at the end of `main()`. That result is right, but only by accident: `runServer()` returns `void`, so `main()` can't tell an error from a normal exit.
* **Fix:** Return `EXIT_FAILURE` from those two paths, and have `runServer()` return a status that `main()` passes on.
