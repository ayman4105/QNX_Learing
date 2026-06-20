# QNX System Library — Quick Study README

## 1. Core Idea

The **QNX System Library** is the layer used by applications to access QNX services.

It contains:

```text
QNX System Library
├── Standard / POSIX functions
│   ├── open()
│   ├── read()
│   ├── write()
│   ├── fork()
│   ├── posix_spawn()
│   └── timer_settime()
│
└── QNX-specific functions
    ├── MsgSend()
    ├── ThreadCtl()
    └── TimerSettime()
```

Key idea:

```text
Application code usually calls standard APIs.
The QNX library hides the QNX-specific implementation details.
```

---

## 2. POSIX Function vs QNX Kernel Call

Some POSIX functions are thin wrappers over QNX kernel calls.

Example:

```text
timer_settime()   -> POSIX / standard function
TimerSettime()    -> QNX kernel call
```

Flow:

```text
Application
    |
    | timer_settime()
    v
QNX C Library
    |
    | Convert arguments
    v
TimerSettime()
    |
    v
Kernel
```

---

## 3. Naming Hint

QNX kernel calls often use mixed capital letters:

```text
TimerSettime()
MsgSend()
ThreadCtl()
```

Standard/POSIX calls are usually lowercase or snake_case:

```text
timer_settime()
open()
read()
write()
posix_spawn()
```

Quick reminder:

```text
lowercase / snake_case  -> usually POSIX-style API
MixedCapitalStyle       -> often QNX-specific API
```

---

## 4. What Does the Wrapper Do?

A wrapper may:

```text
Convert argument format
Adapt POSIX style to QNX internal style
Call a QNX kernel call
Build a message to a QNX server
Handle errors and return standard errno values
```

Example with timers:

```text
POSIX time format:
    seconds + nanoseconds

QNX kernel format:
    64-bit nanoseconds
```

So `timer_settime()` converts the POSIX format, then calls `TimerSettime()`.

---

## 5. QNX Microkernel Effect

QNX is a **microkernel OS**, so many services are outside the kernel.

That means some calls do not go directly to the kernel to do all the work.

They may become messages to servers.

```text
Application
    |
    | POSIX function
    v
QNX C Library
    |
    | Build message
    v
MsgSend()
    |
    v
Server / Resource Manager / Process Manager
```

---

## 6. Example: read()

When you write:

```c
read(fd, buffer, size);
```

Internally, QNX may do:

```text
Application calls read()
        |
        v
QNX C Library builds a read message
        |
        v
MsgSend()
        |
        v
Resource Manager receives the message
        |
        v
Resource Manager handles the read
        |
        v
Resource Manager replies
        |
        v
read() returns to the application
```

Important:

```text
The application uses read().
The library handles the message passing.
```

---

## 7. Example: open()

When you write:

```c
open("/dev/ser1", O_RDWR);
```

Flow:

```text
Application
    |
    | open("/dev/ser1")
    v
QNX C Library
    |
    | Ask Process Manager:
    | Who owns this path?
    v
Process Manager
    |
    | Returns Resource Manager info
    v
QNX C Library
    |
    | Sends open message
    v
Serial Resource Manager
```

Quick note:

```text
Process Manager resolves the pathname.
Resource Manager implements the actual device/service behavior.
```

---

## 8. Example: fork()

When you write:

```c
fork();
```

QNX library may build a message and send it to the **Process Manager**.

```text
Application calls fork()
        |
        v
QNX C Library builds message
        |
        v
Process Manager
        |
        v
Creates the new process
        |
        v
Reply back to application
```

---

## 9. What Should You Use?

If you have a choice between:

```text
Standard / POSIX function
```

and:

```text
Direct QNX-specific kernel call or custom message
```

Prefer:

```text
Standard / POSIX function
```

Examples:

```text
Prefer timer_settime() over TimerSettime() when possible.
Prefer read() over manually building a read message.
Prefer fork() / posix_spawn() over manually sending process-manager messages.
```

---

## 10. Why Prefer Standard Calls?

Standard calls make code:

```text
More portable
Easier to read
Easier to review
Easier to maintain
More familiar to other developers
```

Even if you only target QNX, standard APIs are usually better for clean code.

---

## 11. Do Not Manually Build Internal Messages

Avoid this when a standard function already exists:

```text
Custom-build QNX internal message
        |
        v
Call MsgSend() manually
        |
        v
Send to system server
```

Because it can break due to:

```text
Wrong message format
Wrong error handling
Compatibility issues
Future QNX changes
Harder maintenance
```

Use `MsgSend()` directly only when you are intentionally building your own IPC protocol or QNX-specific service.

---

## 12. Big Picture Diagram

```text
                    Application
                        |
        +---------------+----------------+
        |                                |
        v                                v
 POSIX function                    QNX-specific API
 open/read/write                 MsgSend/ThreadCtl
 timer_settime
        |
        v
                QNX System Library
                        |
        +---------------+----------------+
        |                                |
        v                                v
 Kernel call                    Message to server
 TimerSettime()                 Resource Manager
                                Process Manager
```

---

## 13. Quiz Notes

### Q: What is the QNX System Library?

Answer:

```text
A library layer that provides standard POSIX APIs and QNX-specific APIs for applications.
```

---

### Q: What does timer_settime() call underneath?

Answer:

```text
TimerSettime()
```

---

### Q: Why should you prefer POSIX functions?

Answer:

```text
They make code portable, readable, maintainable, and easier to review.
```

---

### Q: In QNX, what can read() become internally?

Answer:

```text
A message sent to a Resource Manager.
```

---

### Q: In QNX, what can fork() become internally?

Answer:

```text
A message sent to the Process Manager.
```

---

### Q: Should you manually build system messages if a POSIX function exists?

Answer:

```text
No. Use the standard cover function when possible.
```

---

## 14. Final Mental Model

```text
QNX System Library = bridge

From:
    Portable POSIX APIs

To:
    QNX kernel calls
    QNX message-passing servers
```

Final key sentence:

```text
In QNX, the C library hides whether a standard function becomes a kernel call or a message to a server.
```