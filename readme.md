# 2Congested

2Congested is a UDP-based congestion-controlled transport protocol built in C++. While I don't expect this to be on the level of QUIC or TCP—which have had decades of development by some of the greatest minds in network programming, I wanted to implement a subset myself for learning purposes.

The goal of this project is to explore how reliability, acknowledgements, congestion control, and RTT estimation actually work under the hood rather than treating TCP or QUIC as black boxes.

---

## Features List

Current features implemented:

- UDP transport using raw sockets
- Custom packet format and manual serialization
- Sequence numbers and acknowledgements
- Selective acknowledgements using a 64-bit ACK bitmap
- Out-of-order packet tracking and contiguous ACK advancement
- Congestion window (cwnd) tracking
- Slow-start style congestion growth
- Bytes-in-flight accounting
- RTT estimation:
  - Smoothed RTT
  - RTT variance
  - RTO calculation
- Non-blocking sockets
- epoll-based event-driven receive loop
- Asynchronous send and receive threads using `std::async`
- Basic handshake packet structure (framework in place)

---

## How It Works

### Packet Structure

Each packet contains:

Type (1 byte)
Connection ID (4 bytes)
Sequence Number (8 bytes)
Last Contiguous ACK (8 bytes)
ACK Bitmap (8 bytes)
Timestamp (8 bytes)
Payload Size (4 bytes)
Payload (variable)

All multi-byte values are serialized in network byte order and deserialized in host order.

---

### Sending Data

When `Connection::send()` is called:

1. A packet header is constructed containing:
   - Current sequence number
   - Current ACK state
   - Timestamp
2. A shared pointer to the packet is added to:
   - The send queue
   - The inflight tracker
3. Sequence number increments.

The send loop:

- Stops sending when:

bytes_in_flight >= congestion_window

- Otherwise:
  - Serializes packet
  - Sends using `sendto()`
  - Updates inflight byte count

---

### Receiving Data

The receive thread:

1. Waits on `epoll_wait()`
2. Reads packets using `recvfrom()`
3. Deserializes headers
4. Updates:
   - Inflight tracker using received ACKs
   - ACK state for received sequence numbers
5. Sends an empty ACK packet
6. Logs packet metadata for debugging

---

### Acknowledgement System

ACK logic combines:

- Last contiguous ACK
- 64-bit bitmap of out-of-order packets

This allows selective acknowledgement of packets beyond the contiguous ACK range.

When missing packets arrive:

- The contiguous ACK advances
- Entries are removed from the out-of-order tracker

---

### Congestion Control

Currently implements a simplified slow-start style mechanism:

- cwnd increases by MSS when packets are acknowledged
- Sending halts when congestion window is full
- RTT samples are calculated from packet timestamps
- RTO is computed using:

RTO = smoothed_rtt + 4 × rtt_variance

With this, I plan to implement retransmission logic and more advanced congestion control some time in the future.

---

### Concurrency

Two async tasks:

- Receives
- Sends

This separates network IO from send scheduling.

Right now sends happen via spinning. I don't like this implementation as it takes unnecessary CPU time and would rather it be triggered via a condition varaiable, but that is a problem I will solve when I feel masochistic enough to continue this C++ project.

---

## Dependencies

### Ubuntu / Ubuntu-based:

sudo apt install cmake build-essential

Kernel features required:
- epoll
- POSIX sockets

---

## Building

cd build
cmake ..
make

---

## Running

The binary:

`build/peer`

This executable sends a configurable number of packets to another peer. Behavior can be modified in:

`src/peer.cc`

---

## Code Structure

Protocol implementation:

`src/data.cc`
`include/data.h`

Contains:

- Packet encoding and decoding
- Congestion control
- ACK logic
- RTT estimation
- Socket management

Peer executable:

`src/peer.cc`

Contains example usage and traffic generation logic.

---

## Future Goals

Planned improvements:

- Retransmission timers based on RTO
- Loss detection
- Advanced congestion avoidance algorithms
- Packet pacing
- Handshake protocol
- Optional encryption layer

---

## Why This Exists

I don't like black boxes.

TCP and QUIC are incredible engineering achievements, but building even a small subset forces you to understand:

- Packet reordering
- ACK strategies
- Congestion dynamics
- Timing sensitivity

Ty for reading :)
