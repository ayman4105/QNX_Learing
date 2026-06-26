# QNX IPC Methods Comparison — Quick README

## Core Idea

QNX has more than one IPC mechanism. The right choice depends on what you need:

```text
Do you need synchronous request/reply?
Do you need only notification?
Do you need to move large data?
Do you need POSIX portability?
Do you need remote communication?
Do you need priority inheritance?
```

---

## 1. QNX Native Messages

Main APIs:

```c
MsgSend()
MsgReceive()
MsgReply()
```

Model:

```text
Client ---> MsgSend() ---> Server
Client <--- MsgReply() <-- Server
```

The client blocks until the server replies.

### Good for

```text
client/server design
request/reply operations
drivers and services
real-time IPC
when priority matters
```

### Characteristics

```text
blocking
synchronous
copies data
supports small or large messages
carries priority information
server can inherit client priority
built-in synchronization
```

### Remember

```text
QNX native messages are the main IPC mechanism in QNX.
```

---

## 2. QNX Pulses

Pulse = small non-blocking notification.

Model:

```text
Sender ---> pulse ---> Receiver
Sender continues immediately
```

Receiver gets it through:

```c
MsgReceive()
```

If the received item is a pulse:

```text
rcvid == 0
```

### Good for

```text
wake up notification
event ready notification
timer notification
server tells client: data is ready
avoid deadlock when notifying upward
```

### Characteristics

```text
non-blocking
small payload
about 71 bits useful data
compatible with MsgReceive loop
carries priority information
```

### Remember

```text
Use pulse when you only need to notify, not transfer large data.
```

---

## 3. Signals

Signals are POSIX notifications.

Example:

```text
SIGUSR1
SIGTERM
SIGALRM
```

### Good for

```text
portable POSIX-style notification
simple process-level notification
legacy/ported code
```

### Problems

```text
interrupts target execution flow
harder to structure cleanly
no priority inheritance
not ideal for QNX server message loops
```

### Remember

```text
Signals are portable, but pulses usually fit QNX IPC better.
```

---

## 4. Shared Memory

Shared memory lets two or more processes see the same memory area.

Model:

```text
Process A ----\
               +--> shared memory
Process B ----/
```

### Good for

```text
large data
images
buffers
high-throughput bulk data
avoiding data copies
```

### Characteristics

```text
no data copy
very efficient for large data
requires synchronization
no priority information
```

### Needs synchronization

Use one of these:

```text
process-shared mutex
condition variable
semaphore
atomic operations
QNX messages for control
```

### Best design

```text
Use QNX messages for control and metadata.
Use shared memory for bulk data.
```

Example:

```text
Client writes image into shared memory.
Client sends MsgSend(): display image 5.
Server reads image from shared memory.
Server replies when done.
```

---

## 5. Pipes

Pipes are POSIX/Unix-style IPC.

In QNX, pipes require the pipe process.

### Good for

```text
porting Unix/Linux code
simple stream communication
legacy applications
```

### Problems

```text
relatively slow
extra copies
extra context switches
no priority information
requires pipe process
not optimal for native QNX real-time IPC
```

### Remember

```text
Use pipes mainly when porting existing POSIX/Unix code.
```

---

## 6. POSIX Message Queues

POSIX message queues are portable message queues.

They require the mqueue process.

### Good for

```text
portable POSIX queue-based design
ported applications
queued messages
```

### Characteristics

```text
message priority affects delivery order
message priority does not affect thread scheduling
no QNX priority inheritance
two data copies
requires mqueue process
```

### Remember

```text
POSIX message queues are portable, but not as real-time-integrated as QNX native messages.
```

---

## 7. TCP/IP Sockets

Sockets are POSIX-style network IPC.

In QNX, sockets require:

```text
io-sock
```

### Good for

```text
remote communication
communication with other machines
portable networking
client/server over network
```

### Problems for local IPC

```text
not fastest for local communication
extra copies
more context switches
no priority information
requires io-sock
```

### Remember

```text
Use TCP/IP when you need network or remote communication.
For local QNX IPC, native messages are usually better.
```

---

## 8. File Descriptors / Resource Managers

Standard APIs:

```c
open()
read()
write()
fopen()
fread()
fwrite()
```

In QNX, these talk to resource managers using QNX messages underneath.

Model:

```text
Client uses POSIX API
    open/read/write
        |
        v
QNX messages internally
        |
        v
Resource manager / driver
```

### Good for

```text
drivers
/dev interfaces
POSIX-friendly client API
resource managers
```

### Characteristics

```text
client sees normal POSIX file API
server must be QNX resource-manager aware
built on QNX messaging
no double copy like pipes
carries priority information
```

### Remember

```text
This is the best interface for drivers and device-like services.
```

---

## Comparison Table

| IPC Method                           |          Blocking? |       Data Size |            Copies | Priority Info? |        Portable? | Best Use                    |
| ------------------------------------ | -----------------: | --------------: | ----------------: | -------------: | ---------------: | --------------------------- |
| QNX Messages                         |                Yes |     small/large |          one copy |            Yes |     QNX-specific | client/server, RT services  |
| Pulses                               |                 No |            tiny | tiny notification |            Yes |     QNX-specific | wakeup/event notification   |
| Signals                              |                 No |            tiny |      notification |             No |            POSIX | simple/legacy notifications |
| Shared Memory                        | No direct blocking |      very large |           no copy |             No |            POSIX | bulk data                   |
| Pipes                                |          can block |     stream data |      extra copies |             No |            POSIX | ported Unix/Linux code      |
| POSIX Message Queues                 |          can block | queued messages |      extra copies |  delivery only |            POSIX | portable queued IPC         |
| TCP/IP Sockets                       |          can block |   stream/packet |      extra copies |             No |    POSIX/network | remote communication        |
| File Descriptors / Resource Managers |   usually blocking |     small/large |    no double copy |            Yes | POSIX client API | drivers, /dev services      |

---

## How to Choose

### Use QNX native messages when:

```text
you need real-time client/server IPC
you need priority inheritance
you want built-in synchronization
you are writing QNX-native services
```

### Use pulses when:

```text
you only need a small notification
you do not want the sender to block
you want to wake a MsgReceive loop
```

### Use shared memory when:

```text
data is large
you want to avoid copies
you can manage synchronization
```

### Use file descriptors/resource managers when:

```text
you are writing a driver
you want the client to use open/read/write
you still want QNX message behavior underneath
```

### Use pipes/message queues/signals when:

```text
you are porting POSIX/Unix/Linux code
portability matters more than QNX real-time behavior
```

### Use TCP/IP sockets when:

```text
you need communication outside the local machine
or compatibility with network clients
```

---

## Golden Rules

```text
QNX messages:
    best default for QNX real-time client/server IPC

Pulses:
    best for non-blocking small notifications

Shared memory:
    best for large data, but needs synchronization

Resource managers:
    best for drivers and POSIX-style APIs

Pipes / POSIX queues / signals:
    useful for portability and ported code

TCP/IP:
    best for remote communication
```

---

## Final Summary

```text
For local real-time IPC in QNX, start with QNX messages.
Use pulses for notifications.
Use shared memory for bulk data.
Use resource managers when you want open/read/write style access.
Use POSIX mechanisms mainly for portability or networking.
```