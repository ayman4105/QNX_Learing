# QNX IPC - Quick Reference Guide

## Architecture in 10 Seconds

```
  +-------+  +--------+  +--------+  +--------+
  | App A |  | App B  |  | Driver |  | Driver |    ← All separate processes
  +---+---+  +---+----+  +---+----+  +---+----+      with own address spaces
      |          |            |           |
      +----------+------------+-----------+
                       |
                  [ IPC Layer ]                     ← They MUST use IPC to talk
                       |
              +--------+--------+
              |     procnto     |                   ← Microkernel + Process Manager
              |     (PID 1)     |                      (only shared address space)
              +-----------------+
```

**Key insight:** Every `open()`, `read()`, `write()` you call is IPC under the hood.

---

## IPC Types at a Glance

| Type | Sync? | Data Size | Speed | Best For |
|------|-------|-----------|-------|----------|
| **Message Passing** | Synchronous (blocks) | Any | Fast | Client-Server, Resource Managers |
| **Pulse** | Async (fire & forget) | 8-bit code + 32-bit value | Very Fast | Interrupts, Timers, Notifications |
| **Signal** | Async | Minimal | Fast | Process-level notifications (POSIX) |
| **Message Queue** | Async (queued) | Medium | Medium | Decoupled producers/consumers |
| **Shared Memory** | Manual sync needed | Huge | Fastest | Bulk data (video, sensors) |

---

## 1. Message Passing (The Core IPC)

### How It Works

```
  CLIENT                              SERVER
    |                                   |
    |  1. ChannelCreate()               |
    |                          creates channel
    |                                   |
    |                          2. MsgReceive()
    |                          [RECEIVE-BLOCKED] ← waiting
    |                                   |
    | 3. ConnectAttach(pid,chid)        |
    | 4. MsgSend(coid, &msg, &reply)    |
    |  -------------------------------->|  message arrives
    |  [REPLY-BLOCKED] ← waiting        |
    |                          5. processes message
    |                          6. MsgReply(rcvid, &reply)
    |  <--------------------------------|
    |  got reply, continues             |
    |                          loops back to MsgReceive()
```

### Blocking States

| State | Who | Meaning |
|-------|-----|---------|
| **RECEIVE-BLOCKED** | Server | Called `MsgReceive()`, waiting for messages |
| **SEND-BLOCKED** | Client | Called `MsgSend()`, server hasn't received yet |
| **REPLY-BLOCKED** | Client | Server got message, hasn't replied yet |

### The rcvid Rule

```
rcvid = MsgReceive(chid, &msg, sizeof(msg), NULL);

rcvid > 0   →  MESSAGE received. Client is waiting. MUST call MsgReply(rcvid, ...)
rcvid == 0  →  PULSE received. Nobody waiting. Do NOT reply.
rcvid == -1 →  Error. Check errno.
```

### Key Functions

**Server side:**
```c
chid  = ChannelCreate(0);                              // create mailbox
rcvid = MsgReceive(chid, &msg, sizeof(msg), NULL);     // wait for message
MsgReply(rcvid, EOK, &reply, sizeof(reply));           // send reply back
ChannelDestroy(chid);                                  // cleanup
```

**Client side:**
```c
coid = ConnectAttach(0, server_pid, server_chid, _NTO_SIDE_CHANNEL, 0);  // connect
MsgSend(coid, &msg, sizeof(msg), &reply, sizeof(reply));                 // send + wait
ConnectDetach(coid);                                                     // cleanup
```

### Priority Inheritance

Server **inherits client's priority** automatically when it receives a message.
Prevents priority inversion where a high-priority client waits on a low-priority server.

---

## 2. Pulses (Lightweight Async Notification)

### How It Works

```
  SENDER                               SERVER
    |                                    |
    |   MsgSendPulse(coid, prio,        |
    |                code, value)        |
    |  -------------------------------->|  MsgReceive() returns
    |                                    |  with rcvid == 0
    |  continues immediately! 🏃         |
    |  does NOT wait for reply           |  handles pulse
```

### Pulse Data

```
+------------------+------------------+
|  code  (8 bits)  |  value (32 bits) |
|  "what happened" |  "extra info"    |
+------------------+------------------+
```

That's it! Very small payload. Trade-off for being async.

### Who Sends Pulses?

| Source | Why |
|--------|-----|
| **Interrupt handler (ISR)** | ISR must be fast, can't wait for reply |
| **Timer expiry** | Kernel sends pulse when timer fires |
| **Another process** | Quick notification without blocking |

### Key Functions

```c
// Sending a pulse
MsgSendPulse(coid, priority, code, value);

// Receiving (same MsgReceive as messages!)
rcvid = MsgReceive(chid, &msg, sizeof(msg), NULL);
if (rcvid == 0) {
    // It's a pulse!
    int code  = msg.pulse.code;
    int value = msg.pulse.value.sival_int;
}
```

---

## 3. Signals (POSIX)

Standard POSIX signals. Sent to **processes** (not channels).

### Common Signals

| Signal | Meaning | Default Action |
|--------|---------|----------------|
| `SIGTERM` | Please terminate | Terminate |
| `SIGKILL` | Force terminate (can't catch!) | Terminate |
| `SIGSEGV` | Bad memory access | Core dump |
| `SIGCHLD` | Child process died | Ignore |
| `SIGUSR1/2` | User-defined | Terminate |

### Key Functions

```c
// Send signal
kill(pid, SIGTERM);

// Handle signal
struct sigaction sa;
sa.sa_handler = my_handler;
sigaction(SIGTERM, &sa, NULL);

void my_handler(int signo) {
    // keep it short!
}
```

---

## 4. Message Queues (POSIX Async Queued)

```
  SENDER                QUEUE                 RECEIVER
    |                     |                      |
    |  mq_send()         |                      |
    |  ----------------->| [msg1]               |
    |  (doesn't block)   | [msg2]  mq_receive() |
    |                    | [msg3] ------------->|
    |                     |                      |
```

Messages persist in queue until receiver reads them.

### Key Functions

```c
// Open queue
mqd_t mq = mq_open("/my_queue", O_CREAT | O_RDWR, 0666, &attr);

// Send (non-blocking for sender)
mq_send(mq, buffer, size, priority);

// Receive (blocks until message available)
mq_receive(mq, buffer, size, &priority);

// Cleanup
mq_close(mq);
mq_unlink("/my_queue");
```

---

## 5. Shared Memory (Fastest IPC)

```
  Process A              Process B
  +--------+            +--------+
  | ptr ---|---+    +---|--- ptr |
  +--------+   |    |   +--------+
               v    v
          +==============+
          | SHARED       |
          | MEMORY       |     ← Both read/write directly
          | (no copying!)|        MUST use mutex!
          +==============+
```

### Key Functions

```c
// Create
int fd = shm_open("/my_shm", O_CREAT | O_RDWR, 0666);
ftruncate(fd, size);

// Map into your address space
void *ptr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

// Use it like normal memory
((my_data_t *)ptr)->value = 42;

// Cleanup
munmap(ptr, size);
shm_unlink("/my_shm");
```

### ⚠️ MUST synchronize access with mutex or semaphore!

```c
pthread_mutex_lock(&shared->mutex);
shared->data = new_value;            // safe write
pthread_mutex_unlock(&shared->mutex);
```

---

## Decision Flowchart

```
Need to communicate between processes?
│
├─ Need a reply back? (request/response)
│  └─ YES → Message Passing
│
├─ Just a quick notification? No data?
│  └─ YES → Pulse (from ISR/timer) or Signal (POSIX)
│
├─ Need to queue messages? Sender can't wait?
│  └─ YES → Message Queue
│
├─ Transferring large/bulk data?
│  └─ YES → Shared Memory (+ synchronization!)
│
└─ Default → Message Passing (safest, most common)
```

---

## Common Patterns in QNX

### Pattern 1: Resource Manager (Most Common)

```
  Client App                    Driver / Service
  (uses standard C)             (Resource Manager)

  fd = open("/dev/mydev",...) --[MsgSend]--> handles _IO_CONNECT
  read(fd, buf, n)            --[MsgSend]--> handles _IO_READ
  write(fd, buf, n)           --[MsgSend]--> handles _IO_WRITE
  close(fd)                   --[MsgSend]--> handles _IO_CLOSE
```

Client uses POSIX. Server handles messages. IPC is hidden.

### Pattern 2: Pulse from Interrupt

```
  [Hardware IRQ] → ISR → MsgSendPulse() → Server thread handles it
```

ISR can't block, so it fires a pulse. Server picks it up in `MsgReceive()`.

### Pattern 3: Shared Memory + Pulse Notification

```
  Writer: writes data to shared memory → sends pulse "data ready"
  Reader: receives pulse → reads from shared memory
```

Best of both worlds: fast data transfer + async notification.

---

## Quick API Cheat Sheet

| What | Function |
|------|----------|
| Create channel | `ChannelCreate(0)` |
| Destroy channel | `ChannelDestroy(chid)` |
| Connect to channel | `ConnectAttach(nd, pid, chid, idx, flags)` |
| Disconnect | `ConnectDetach(coid)` |
| Send message (block) | `MsgSend(coid, smsg, sbytes, rmsg, rbytes)` |
| Receive message (block) | `MsgReceive(chid, msg, bytes, info)` |
| Reply to message | `MsgReply(rcvid, status, msg, bytes)` |
| Reply with error only | `MsgError(rcvid, errno)` |
| Send pulse (no block) | `MsgSendPulse(coid, prio, code, value)` |
| Send signal | `kill(pid, signo)` |
| Open message queue | `mq_open(name, flags, mode, attr)` |
| Send to queue | `mq_send(mqd, msg, len, prio)` |
| Receive from queue | `mq_receive(mqd, msg, len, prio)` |
| Create shared memory | `shm_open(name, flags, mode)` |
| Map memory | `mmap(addr, len, prot, flags, fd, off)` |
| Unmap memory | `munmap(addr, len)` |

---

## References

- [QNX Neutrino IPC Documentation](https://www.qnx.com/developers/docs/)
- QNX Neutrino Programmer's Guide - Interprocess Communication chapter
- QNX System Architecture Guide - Microkernel chapter
