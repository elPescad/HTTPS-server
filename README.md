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
