# QNX IPC — Choosing the Right IPC Method

## Core Idea

QNX has many IPC mechanisms. There is no single best choice for every case.

Choose based on:

```text
portability
amount of data
blocking or non-blocking behavior
buffering control
safety requirements
network communication
combining mechanisms
```

---

## 1. Is POSIX Required?

Ask first:

```text
Do I need portable code?
Am I porting Linux/Unix/open-source code?
Does the source need to stay POSIX compliant?
```

If yes, prefer POSIX-style APIs:

```text
signals
shared memory
pipes
POSIX message queues
TCP/IP sockets
open/read/write resource-manager style APIs
```

If no, and this is a QNX-native real-time design, prefer:

```text
QNX messages
QNX pulses
QNX resource managers
```

---

## 2. How Much Data Is Being Moved?

### Small data

Use:

```text
QNX messages
pulses if it is only a tiny notification
```

### Large data

Use:

```text
shared memory
```

Best common design:

```text
QNX message passing -> control and metadata
shared memory       -> big data buffers
```

Example:

```text
Client writes image into shared memory.
Client sends MsgSend: display image 5.
Server reads image from shared memory.
Server replies when done.
```

---

## 3. Do I Need a Direct Response?

Ask:

```text
Can the sender block and wait for a reply?
```

If yes:

```text
Use QNX messages.
```

Because:

```text
MsgSend() blocks until server replies.
```

If no:

```text
Use pulses or another non-blocking notification.
```

Because:

```text
Pulse sends notification and sender continues running.
```

---

## 4. Am I Willing to Build a Buffering Scheme?

Some IPC mechanisms already have buffering, but sometimes you need full control.

For strict systems, you may want to:

```text
preallocate buffers
avoid dynamic allocation during runtime
handle low-memory cases predictably
control exactly who owns each buffer
```

This often pushes the design toward:

```text
QNX native IPC
shared memory with controlled buffers
```

---

## 5. Is Safety Certification Important?

If safety matters, POSIX mechanisms may not be the best choice.

For safety-focused systems, QNX-specific mechanisms may be preferred because they can be part of the safety-certified solution.

Think:

```text
safety-critical path -> prefer controlled QNX-native design
portable non-critical code -> POSIX may be acceptable
```

---

## 6. Do I Need Network Communication?

If communication must cross a network or another machine:

```text
Use TCP/IP sockets.
```

Example:

```text
QNX target <---- TCP/IP ----> PC tool
```

If communication is local only, TCP/IP is usually not the fastest option.

---

## 7. Can I Combine IPC Methods?

Yes. This is very common.

A real design often uses more than one mechanism:

```text
QNX messages for commands
pulses for notifications
shared memory for big data
TCP/IP for remote clients
open/read/write for driver-style interfaces
```

Do not force one IPC mechanism to solve every problem.

---

## Quick Decision Table

| Requirement                       | Good Choice                               |
| --------------------------------- | ----------------------------------------- |
| QNX real-time request/reply       | QNX messages                              |
| Small non-blocking notification   | Pulses                                    |
| Huge data buffers                 | Shared memory                             |
| Driver or `/dev` interface        | Resource manager + open/read/write        |
| Porting POSIX/Linux code          | POSIX IPC: signals, pipes, message queues |
| Network or remote system          | TCP/IP sockets                            |
| Safety-critical controlled design | QNX-native mechanisms                     |

---

## Simple Rule

```text
Need reply?           -> QNX messages
Need wake-up only?    -> Pulse
Need big data?        -> Shared memory
Need network?         -> TCP/IP
Need driver API?      -> Resource manager
Need portability?     -> POSIX IPC
```

---

## Final Summary

When choosing IPC in QNX, ask:

```text
1. Do I need POSIX portability?
2. How much data am I moving?
3. Do I need the sender to wait for a reply?
4. Do I need strict buffering control?
5. Is this safety-critical?
6. Do I need to communicate over a network?
7. Can I combine mechanisms?
```

Golden sentence:

```text
Use QNX messages for real-time control, pulses for notification, shared memory for large data, and TCP/IP only when communication must go outside the local system.
```