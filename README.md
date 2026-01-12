# epoll-et-lt-benchmark

This project implements an epoll-based chat server designed for high-concurrency environments and experimentally compares the performance characteristics and bottlenecks of Edge-Triggered (ET) and Level-Triggered (LT) modes through load testing.


## Motivation

Before working in the field of moblie communications, I wanted to gain hands-on experience with fundamental network server code structures and event-driven I/O models.

By implementing and experimenting with epoll, which is widely used in Linux server enviroments, this project aims to reduce the gap betwenn academic knowledge and real-world network code, enabling faster adaptation to practical development environments

In addition, this project provided an opportunity to apply and validate operating systems and networking concepts learned during undergraduate studies through real implementation and benchmarking.


## Architecture

The server is implemented as a single-process, event-driven architecture using  epoll.
A single main event loop is responsible for handling all connection lifecycle events,
including accept, read, and write operations.

The server supports both Edge-Triggered (ET) and Level-Triggered (LT) modes.
The behavior is controlled by an internal flag ('is_et'), allowing the same codebase to switch between ET and LT for performance comparison.

All server and client sockets are configured as non-blocking.
In ET mode, the event loop explicitly drains all available data until 'EAGAIN' is returned,
which is required for correct Edge-Triggered behavoir.
In LT mode, events are repeatedly delivered as long as unread data remains,
allowing simpler handling logic while still using non-blocking I/O.

This design allows direct comparison of ET and LT characteristics while keeping the overall server structure identical.
