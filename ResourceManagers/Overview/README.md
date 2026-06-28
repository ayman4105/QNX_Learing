# QNX Resource Managers — Full Quick README

## 1. Big Picture

A **QNX Resource Manager** is a normal user-space process that makes a resource appear as a file, directory, or device in the QNX pathname space.

Clients use normal POSIX APIs:

```c
open()
read()
write()
close()
devctl()
```

Internally, QNX converts these API calls into **message passing** between the client and the Resource Manager.

---

## 2. The Three Main Players

```text
+----------------+
| Client Process |
+----------------+
        |
        | POSIX APIs: open/read/write/close
        v
+----------------+
|     libc       |
+----------------+
        |
        | QNX messages
        v
+----------------+
| Resource       |
| Manager        |
+----------------+
```

There is also one very important system component:

```text
procnto = Process Manager + Microkernel
```

`procnto` stores the pathname mapping.

---

## 3. Important Correction

Do not confuse these three things:

```text
/dev/ser1        = pathname / registered name
devc-ser8250     = Resource Manager process
procnto          = stores the mapping between them
```

So:

```text
Resource Manager is NOT /dev/ser1.
Resource Manager is the process that registers /dev/ser1.
```

Example:

```text
devc-ser8250 registers /dev/ser1 with procnto.

procnto stores:
    /dev/ser1 -> devc-ser8250 PID + CHID
```

---

## 4. Resource Manager Definition

A Resource Manager is:

```text
A normal user-space process
that registers one or more pathnames
and handles POSIX-generated messages from clients.
```

It can represent:

```text
Hardware resource:
    serial port
    disk
    CAN controller
    I2C controller
    console

Software resource:
    logger
    queue
    virtual device
    configuration service
```

---

## 5. Pathname Space

The QNX pathname space is a mapping from slash-delimited names to Resource Manager processes.

Examples:

```text
/             -> filesystem resource manager or procnto entries
/dev/ser1     -> devc-ser8250
/dev/con1     -> devc-con
/my/logger    -> my_logger process
```

Diagram:

```text
QNX Pathname Space
================================================

/
├── dev
│   ├── ser1   -> devc-ser8250
│   └── con1   -> devc-con
├── proc       -> procnto
└── my
    └── logger -> my_logger
```

---

## 6. Who Stores the Mapping?

The mapping is stored by:

```text
procnto
```

A Resource Manager only registers a pathname.

Example:

```text
devc-ser8250 process
        |
        | register /dev/ser1
        v
procnto pathname space
        |
        v
/dev/ser1 -> devc-ser8250 PID + CHID
```

---

## 7. PID, CHID, Handle, and Flags

When a Resource Manager registers a path, `procnto` stores metadata such as:

```text
PID     = process ID of the Resource Manager
CHID    = channel ID used to receive messages
Handle  = extra value to distinguish registrations
Flags   = registration options
```

Example:

```text
/dev/con1 -> PID 28676, CHID 1, handle 0, flags 0
/dev/con2 -> PID 28676, CHID 1, handle 1, flags 0
```

Same Resource Manager process, different registered names.

---

## 8. `/proc/mount`

QNX exposes a debug view of pathname registrations through:

```sh
/proc/mount
```

Example:

```sh
cd /proc/mount/dev/con1
ls
```

You may see something like:

```text
0,28676,1,0,0
```

Meaning:

```text
0       -> unused field
28676   -> Resource Manager PID
1       -> channel ID
0       -> handle/index
0       -> flags
```

To identify the process:

```sh
pidin -p 28676
```

This can show that the owner is something like:

```text
devc-con
```

---

## 9. How `open()` Works

When a client calls:

```c
int fd = open("/dev/ser1", O_RDWR);
```

The client does not directly know who owns `/dev/ser1`.

The flow is:

```text
Client
  |
  | open("/dev/ser1")
  v
libc
  |
  | asks procnto:
  | who owns /dev/ser1?
  v
procnto
  |
  | replies:
  | devc-ser8250 owns it
  v
libc
  |
  | ConnectAttach to devc-ser8250
  | send open/connect message
  v
Resource Manager
  |
  | check open request
  v
open() returns fd
```

---

## 10. Longest Prefix Match

QNX uses:

```text
Longest slash-delimited whole-word matching prefix
```

Example:

Registered paths:

```text
/           -> filesystem
/dev/ser1   -> devc-ser8250
```

Client opens:

```c
open("/dev/ser1", O_RDWR);
```

Possible matches:

```text
/           yes, but short
/dev        not registered
/dev/ser1   yes, longest match
```

So QNX selects:

```text
devc-ser8250
```

---

## 11. Whole-Word Matching

This is important:

```text
/dev/ser
```

does **not** match:

```text
/dev/ser1
```

Because `ser` is not the same full pathname component as `ser1`.

---

## 12. Fallback to `/`

If a path is not directly registered, QNX may fall back to a Resource Manager registered at `/`.

Example:

```c
open("/home/bill/spud.dat", O_RDONLY);
```

Maybe only `/` is registered by the filesystem.

Flow:

```text
open("/home/bill/spud.dat")
        |
        v
procnto finds best prefix = /
        |
        v
filesystem Resource Manager
        |
        v
filesystem searches disk:
    /home
    /home/bill
    /home/bill/spud.dat
```

---

## 13. Multiple Possible Resource Managers

Sometimes multiple Resource Managers register the same prefix, such as `/`.

`procnto` can return a list of possible Resource Managers.

`libc` tries them in order.

```text
procnto returns:
    1. procnto
    2. filesystem

libc tries #1
    if ENOENT -> try next

libc tries #2
    if success -> return fd
    if error other than ENOENT -> fail
```

Rule:

```text
ENOENT:
    try another Resource Manager if available

EACCES / EPERM / EIO / other errors:
    stop and return failure
```

---

## 14. After `open()` Succeeds

After `open()` returns a file descriptor, `procnto` is not involved in normal `read()` and `write()` operations.

```text
open():
    needs procnto to resolve pathname

read/write:
    use fd
    go directly to Resource Manager
```

Diagram:

```text
Before open:
    Client -> procnto -> Resource Manager

After open:
    Client -----------------> Resource Manager
```

---

## 15. File Descriptor Relationship

After:

```c
int fd = open("/dev/ser1", O_RDWR);
```

Conceptually:

```text
fd
 |
 v
connection to devc-ser8250 Resource Manager
```

Then:

```c
write(fd, "hello", 5);
read(fd, buf, 100);
close(fd);
```

become messages sent directly to the Resource Manager.

---

## 16. `write()` Flow

Client code:

```c
write(fd, "hello", 5);
```

Internal flow:

```text
Client Process                         Resource Manager
=======================================================

write(fd, "hello", 5)
        |
        v
libc builds _IO_WRITE message
        |
        |------------------------------------>
                                             |
                                             v
                                      MsgReceive()
                                             |
                                             v
                                      write handler
                                             |
                                             v
                                      process data
                                             |
        <------------------------------------|
             reply status = 5

write() returns 5
```

---

## 17. `read()` Flow

Client code:

```c
read(fd, buf, 100);
```

Internal flow:

```text
Client Process                         Resource Manager
=======================================================

read(fd, buf, 100)
        |
        v
libc builds _IO_READ message
        |
        |------------------------------------>
                                             |
                                             v
                                      MsgReceive()
                                             |
                                             v
                                      read handler
                                             |
                                             v
                                      prepare data
                                             |
        <------------------------------------|
             reply status = n + data

read() returns n
buf contains data
```

---

## 18. Resource Manager Internal Structure

A Resource Manager is a message-receiving process.

High-level structure:

```text
Resource Manager Process
================================================

1. Initialize internal state
2. Create message channel
3. Register pathname with procnto
4. Enter forever loop
5. Wait for messages
6. Dispatch message to handler
7. Do real work
8. Reply to client
```

Pseudo-code:

```c
int main(void)
{
    initialize_resource_manager();

    create_channel();

    register_pathname("/my/device");

    while (1) {
        wait_for_message();

        dispatch_message_to_handler();
    }
}
```

---

## 19. Resource Manager Layers

For a hardware driver:

```text
Client
  |
  | open/read/write/devctl
  v
Resource Manager
+----------------------------------+
| POSIX handlers                   |
|   open handler                   |
|   read handler                   |
|   write handler                  |
|   devctl handler                 |
+----------------------------------+
| Business logic                   |
|   buffers                        |
|   state                          |
|   protocol logic                 |
+----------------------------------+
| Hardware layer                   |
|   registers                      |
|   interrupts                     |
|   DMA                            |
+----------------------------------+
```

For a software service, the hardware layer may not exist.

---

## 20. Message Categories

Resource Manager messages are grouped into three categories:

```text
1. Connect messages
2. I/O messages
3. Other messages / pulses
```

---

## 21. Connect Messages

Connect messages are pathname-based.

They happen before a file descriptor exists.

Examples:

```c
open("/dev/ser1", O_RDWR);
unlink("/tmp/file");
rename("/old", "/new");
```

Examples of connect subtypes:

```text
open()    -> _IO_CONNECT_OPEN
unlink()  -> _IO_CONNECT_UNLINK
rename()  -> _IO_CONNECT_RENAME
```

The most important connect message for simple Resource Managers is usually:

```text
_IO_CONNECT_OPEN
```

---

## 22. I/O Messages

I/O messages are file-descriptor-based.

They happen after a successful `open()`.

Examples:

```c
read(fd, buf, size);
write(fd, data, size);
close(fd);
devctl(fd, ...);
fstat(fd, ...);
```

Examples:

```text
read()   -> _IO_READ
write()  -> _IO_WRITE
close()  -> _IO_CLOSE
```

---

## 23. Connect vs I/O Summary

```text
Connect Messages
================================================
- Pathname-based
- Before fd exists
- Establish or modify relationship
- Examples:
    open()
    unlink()
    rename()

I/O Messages
================================================
- File-descriptor-based
- After fd exists
- Use existing relationship
- Examples:
    read()
    write()
    close()
    devctl()
```

Golden idea:

```text
open() creates the relationship.
read/write use the relationship.
close() ends the relationship.
```

---

## 24. Resource Manager Framework

QNX provides a Resource Manager Framework in the C library.

It helps with:

```text
message receiving
message decoding
dispatching
default handlers
OCB management
helper functions
reply helpers
```

Without the framework, you would need to manually parse and handle many message types.

---

## 25. Default Handlers

You do not need to implement every possible POSIX operation.

The framework provides default behavior.

Example input-only device:

```text
custom:
    read handler

default:
    open handling
    write unsupported/error
    close cleanup
    other operations default response
```

This lets you build the Resource Manager gradually.

---

## 26. Handlers

A handler is a function you write to implement an operation.

```text
Client API              Resource Manager Handler
================================================
open()                  open handler
read()                  read handler
write()                 write handler
close()                 close handler
devctl()                devctl handler
```

Flow:

```text
Incoming message
       |
       v
Resource Manager Framework
       |
       +-- _IO_CONNECT_OPEN -> open handler
       |
       +-- _IO_READ         -> read handler
       |
       +-- _IO_WRITE        -> write handler
       |
       +-- _IO_CLOSE        -> close handler
```

---

## 27. OCB

`OCB` means:

```text
Open Control Block
```

It is Resource Manager state for one successful `open()`.

Client side:

```text
fd
```

Resource Manager side:

```text
OCB
```

Diagram:

```text
Client Process                    Resource Manager
==================================================

fd 3  --------------------------> OCB #1
```

---

## 28. Why OCB Is Needed

Each client may open the same resource differently.

Example:

```c
fd1 = open("/dev/ser1", O_RDONLY);
fd2 = open("/dev/ser1", O_WRONLY);
fd3 = open("/dev/ser1", O_RDWR);
```

Resource Manager side:

```text
fd1 -> OCB 1: read-only
fd2 -> OCB 2: write-only
fd3 -> OCB 3: read-write
```

The OCB may store:

```text
open mode
current file offset
device instance
blocking/non-blocking flags
client-specific state
permissions
```

---

## 29. OCB Example

Client:

```c
int fd = open("/dev/ser1", O_RDONLY);
```

OCB:

```text
mode = read-only
device = ser1
```

Later:

```c
write(fd, "A", 1);
```

Resource Manager checks the OCB:

```text
mode = read-only
```

So it can reject the write.

---

## 30. Same Process, Multiple Paths

A Resource Manager can register several paths.

Example:

```text
devc-con registers:
    /dev/con1
    /dev/con2
```

Both paths may point to the same process.

When clients open them:

```c
fd1 = open("/dev/con1", O_RDWR);
fd2 = open("/dev/con2", O_RDWR);
```

The OCB can track:

```text
fd1 -> device = con1
fd2 -> device = con2
```

---

## 31. Write Data Handling

For a small write:

```c
write(fd, "hello", 5);
```

The Resource Manager may receive:

```text
_IO_WRITE header
+
"hello"
```

in its receive buffer.

Diagram:

```text
Receive Buffer
+----------------------+
| _IO_WRITE header     |
+----------------------+
| "hello"              |
+----------------------+
```

The write handler can process the data and reply:

```text
status = 5
```

Then client sees:

```text
write() returns 5
```

---

## 32. Large Write Data

For a large write:

```c
write(fd, big_buffer, 20000);
```

The Resource Manager receive buffer may be smaller than the full message.

Example:

```text
Client sends:
    20 KB

Resource Manager receive buffer:
    about 2 KB
```

The Resource Manager may receive the header and only part of the data.

To get the remaining data, it uses:

```c
resmgr_msgread()
```

Conceptual flow:

```text
write(fd, 20 KB)
        |
        v
Resource Manager receives first part
        |
        v
Need more data?
        |
        v
resmgr_msgread()
        |
        v
Get remaining client data
        |
        v
Process all data
        |
        v
Reply status = bytes processed
```

---

## 33. Read Data Handling

For a read:

```c
read(fd, buf, 200);
```

The client is asking for up to 200 bytes.

The Resource Manager checks its internal data source:

```text
RX buffer
memory buffer
queue
hardware data
software data
```

If 20 bytes are available:

```text
reply status = 20
reply data   = 20 bytes
```

Then client sees:

```text
read() returns 20
buf contains 20 bytes
```

---

## 34. Reply Types

The Resource Manager must reply to the client.

Possible responses:

```text
1. Error
2. Success with status only
3. Success with status + data
4. Delayed reply
```

---

### Error Reply

Examples:

```text
read from write-only fd
write to read-only fd
permission denied
hardware I/O error
```

Possible errors:

```text
EPERM
EACCES
EBADF
EIO
```

---

### Success With Status Only

Common for `write()`.

Example:

```text
Client writes 200 bytes
Resource Manager accepts 200 bytes
Reply status = 200
No data returned
```

Client:

```text
write() returns 200
```

---

### Success With Status + Data

Common for `read()`.

Example:

```text
Client asks for 200 bytes
Resource Manager has 20 bytes
Reply status = 20
Reply data = 20 bytes
```

Client:

```text
read() returns 20
buffer contains data
```

---

### Delayed Reply

Sometimes the Resource Manager may intentionally delay replying.

Example:

```text
Client calls read()
No data is available
Resource Manager keeps client blocked
Later hardware interrupt brings data
Resource Manager replies to client
```

This is useful for blocking drivers, but it is more advanced.

---

## 35. Serial Driver Example

Client:

```c
int fd = open("/dev/ser1", O_RDWR);

write(fd, "A", 1);

read(fd, buf, 10);

close(fd);
```

Internal flow:

```text
open("/dev/ser1")
    |
    v
procnto resolves /dev/ser1
    |
    v
devc-ser8250 receives _IO_CONNECT_OPEN
    |
    v
OCB created
    |
    v
fd returned


write(fd, "A", 1)
    |
    v
_IO_WRITE to devc-ser8250
    |
    v
write handler sends data to UART TX buffer
    |
    v
reply status = 1


read(fd, buf, 10)
    |
    v
_IO_READ to devc-ser8250
    |
    v
read handler checks UART RX buffer
    |
    v
reply with data or block/error


close(fd)
    |
    v
_IO_CLOSE
    |
    v
OCB cleanup
```

---

## 36. Software Logger Example

Resource Manager:

```text
my_logger registers /my/logger
```

Client:

```c
int fd = open("/my/logger", O_WRONLY);

write(fd, "System started", 14);

close(fd);
```

Internal flow:

```text
open("/my/logger")
    -> procnto resolves path
    -> my_logger receives open
    -> OCB created

write(fd)
    -> _IO_WRITE message
    -> my_logger stores log message
    -> reply status = bytes written

close(fd)
    -> cleanup OCB
```

No hardware is required.

---

## 37. Final End-to-End Diagram

```text
Boot Script
================================================
devc-ser8250 &


Resource Manager Startup
================================================
devc-ser8250 starts
    |
    v
creates channel
    |
    v
registers /dev/ser1 with procnto
    |
    v
enters receive loop


Client open()
================================================
client open("/dev/ser1")
    |
    v
libc asks procnto
    |
    v
procnto returns devc-ser8250 PID + CHID
    |
    v
client sends _IO_CONNECT_OPEN
    |
    v
devc-ser8250 creates OCB
    |
    v
open returns fd


Client I/O
================================================
write(fd)
    |
    v
_IO_WRITE directly to devc-ser8250

read(fd)
    |
    v
_IO_READ directly to devc-ser8250

close(fd)
    |
    v
_IO_CLOSE directly to devc-ser8250
```

---

## 38. Quick Checklist for Understanding

Before moving on, make sure you can answer these:

```text
1. Is /dev/ser1 the Resource Manager?
2. Who stores pathname mappings?
3. What does open() do before talking to the Resource Manager?
4. What happens after open() returns fd?
5. What is the difference between Connect messages and I/O messages?
6. What is an OCB?
7. Why might a write handler need resmgr_msgread()?
8. How does read() return data?
9. Why can a Resource Manager delay a reply?
```

Answers:

```text
1. No. /dev/ser1 is a pathname.
2. procnto.
3. It asks procnto to resolve the path.
4. read/write/close go directly to the Resource Manager.
5. Connect messages are pathname-based; I/O messages are fd-based.
6. OCB is per-open state in the Resource Manager.
7. Because the client may write more data than fits in the receive buffer.
8. The Resource Manager replies with status + data.
9. To implement blocking behavior, such as waiting for hardware data.
```

---

## 39. Final Summary

```text
Resource Manager:
    user-space process
    registers pathname
    receives POSIX-generated messages
    replies to clients

procnto:
    stores pathname mapping
    resolves open() paths

open():
    pathname-based
    connect message
    creates fd + OCB

read/write/close:
    fd-based
    I/O messages
    go directly to Resource Manager

Framework:
    receives messages
    dispatches to handlers
    provides default behavior
    manages OCBs

OCB:
    per-open state
    connects client fd to Resource Manager internal state

write:
    client sends data to Resource Manager
    large writes may need resmgr_msgread()

read:
    Resource Manager sends data back to client

reply:
    success, data, error, or delayed reply
```

Golden sentence:

```text
A QNX Resource Manager turns POSIX calls into message-based service requests: open creates the relationship, read/write use that relationship, and the Resource Manager replies with status, data, or an error.
```