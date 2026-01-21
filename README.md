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


## Event Handling Flow

When an event occurs on the server socket, the server accepts the new connection and stores the client information 
in a linked list for simple connection lifecycle management.
The client socket is then registered to the epoll instance.

The main event loop waits for events using 'epoll_wait' and iterates over the returned event set and dispatches handling logic based on socket type.
When an event occurs on a client socket (EPOLLIN), the server first checks for error conditions and closes the connection if necessarry.

Error conditions such as EPOLLERR, EPOLLHUP, and EPOLLDHUP are handled prior to nomal processing to immediately clean up invalid client connections.

For readable events, the server invokes the 'on_readable' handler.
In ET mode, the handler drains all available data in a loop until 'EAGAIN' is returned
In LT mode, the handler reads available data once per event, relying on repeated event notification if data remains unread.

This event handling model closely reflects a minimal, single-threaded, epoll-based server architecture commonly used as a foundation in high-concurrency systems.


## Benchmark Design

The performance evaluation of this project focuses on two primary aspects.

First, the stability of the server under load is evaluated:
how reliably the server operates and how many concurrent connections it can safely handle.

Second, the throughput characteristics of Edge-Triggered (ET) and Level-Triggered (LT) modes are compared
under identical server architecture and workload conditions.

To conduct the benchmark, a custom load client is executed with configurable parameters, including:
- 'conns': number of concurrent connections
- 'duration': target connection duration
- 'msg_bytes': size of each message
- 'rate': target message send rate per second
- 'do_recv': flag indicating whether the client processes broadcast messages from the server
- 'burst: flag controlling whether all clients send messages simultaneously in bursts

During execution, the load client reports:
- total messages sent
- total bytes sent
- actual elapsed time
- effective message rate
- bytes sent per second

Server efficiency is primarily evaluated using bytes-per-second throughput.
This metric is used to observe how ET and LT modes differ in handling increasing load,
as well as how throughput behavior changes as the number of concurrent connections grows.


## Test Environment

All benchmarks were conducted in a controlled local environment to ensure consistent and reproducible results.

### Execution Environment
- Operating System: Linux (Ubuntu-based)
- Kernel: Linux kernel (epoll-supported)
- CPU: Multi-core x86_64 processor
- Memory: Sufficient RAM to avoid memory pressure during tests

The server and load clients were executed on the same machine to eliminate external network variability.

---

### Server Configuration
- single-process, single-threaded server architecture
- Event-driven I/O using 'epoll'
- Edge-Triggered (ET) and Level-Triggered (LT) modes implemented within the same codebase
- ET/LT behavior controlled via a runtime flag ('is_et') to ensure identical logic except for event handling semantics
- All sockets configured as non-blocking

This configuration ensures that performance differences originate primarily from ET/LT behavior rather than architectural changes.

---

### Client Configuration
- Custom load client implemented for benchmarking
- All tests executed using the same client binary.
- Load parameters controlled via command-line options:
  - number of concurrent connections
  - target test duration
  - message size (bytes per message)
  - target send rate
  - receive handling flag
  - burst sending option

This approach allows fine-grained control over workload characteristics while maintaining consistent test conditions.

---

### Network Environment
- Server and load clients executed in separate Docker containers
- Containers connected via a Docker bridge network
- Inter-container communication performed over virtual Ethernet interfaces (veth)
- No external physical network or WAN involved

By using an inter-container virtual network, the benchmark focuses on server-side event handling and I/O behavior while avoiding variability from external network latency or packet loss.

---

### Measurement Method
- All performance metrics were measured on the client side
- Elapsed time measured using wall-clock time
- Reported metrics include:
  - total messages sent
  - total bytes sent
  - effective message rate (messages/sec)
  - effective throughput (bytes/sec)

Throughput (bytes/sec) was used as the primary metric to evaluate server efficiency under varying load conditions.


## Benchmark Results

Test parameters(Fixed)
- concurrent connections: 500
- duration: 20s
- send rate: 50msg/s per client
- burst mode enabled
- receive disabled

Message size varied to observe ET/LT behavior under different payload characteristics.

Results Summary


| Message Size | Mode | Msg Rate (msg/s) | Throughput (bytes/s) |
|--------------|------|------------------|----------------------|
| 64B  | ET | 21962.9 | 1405.6 |
| 64B  | LT | 22072.8 | 1412.7 |
| 128B | ET | 22286.6 | 2852.7 |
| 128B | LT | 22111.7 | 2830.3 |
| 256B | ET | 13518.4 | 3460.7 |
| 256B | LT | 12065.4 | 3088.7 |
| 512B | ET | 7294.2  | 3734.6 |
| 512B | LT | 6905.9  | 3535.8 |

### Observations

- With small message sizes (64B, 128B), ET and LT exhibit similar throughput.
- As message size increases, ET consistently outperforms LT in throughput.
- ET benefits from reduced epoll wakeups and efficient buffer draining.
- LT incurs additional overhead due to repeated readiness notifications.


## Limitations and Future Work

This benchmark was designed to isolate and compare the behavioral differences between Edge-Triggered (ET) and Level-Triggered (LT) epoll modes. As a result, several limitations were intentionally introduced to reduce confounding variables.

### Limitations

- The server operates in a single-process, single-threaded configuration.
  While this simplifies analysis of epoll behavior, it does not reflect production-scale server architectures that leverage multi-core parallellism.

- All benchmarks were executed in a local environment using container-to-container communication on the same host machine.
  While traffic traverses the kernel network stack via virtual Ethernet (veth) interfaces, no physical network interfaces, external switches, or real network latency were involved.
  As a result, hardware-level NIC behavior and cross-host network effects are not captured in this benchmark.

- Performance metrics were collected solely from the client side.
  Server-side metrics such as CPU utilization, context switch counts, and epoll wakeup frequency were not directly measured.

- The workload primarily focuses on write-heavy message broadcasting.
  Read-dominant or mixed I/O patterns were not explored in this benchmark.


### Future Work

- Extend the server to a multi-threaded or multi-process architecture to evaluate how ET and LT scale under concurrent event loops.

- Incorporate server-side profiling using tools such as 'perf' or 'strace' to measure epoll wakeups, system call frequency, and CPU usage.

- Run benchmarks across separate hosts or physical network interfaces to evaluate the impact of real network conditions.

- Explore additional workloads, including receive-heavy and asymmetric traffic patterns, to better understand ET/LT behavior under diverse application scenarios.
