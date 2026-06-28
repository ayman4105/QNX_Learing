# QNX Resource Manager `write()` Handler — Quick README

## 1. Topic Goal

This README explains how a QNX Resource Manager handles a client `write()` request.

The script focuses on:

```text
POSIX write() behavior
How QNX implements write() using messages
_IO_WRITE messages
io_write_t and msg->i.nbytes
MsgSendv()
MsgReply()
iofunc_write_verify()
xtype check
write() handler logic
mtime/ctime update flags
large writes
resmgr_msgget()
```

---

## 2. POSIX `write()` from the Client Side

A normal POSIX client calls:

```c
ssize_t write(int fd, const void *buffer, size_t bytes_to_write);
```

Arguments:

```text
fd:
    File descriptor returned from open()

buffer:
    Data the client wants to write

bytes_to_write:
    Number of bytes the client wants to write
```

Return values:

```text
-1:
    Error happened.
    errno is set.

0:
    0 bytes were written.
    This may happen if the client requested write(fd, buf, 0).

1 to bytes_to_write:
    Number of bytes successfully written.
```

Important difference from `read()`:

```text
read() returning 0 means EOF.
write() returning 0 does NOT mean EOF.
```

For writing, there is no normal “end of file” idea like `read()`.

If a client writes past the end of a normal file, the file usually grows.

For a fixed-size device, such as a video frame buffer, the Resource Manager may return an error if there is no more space.

---

## 3. Example Resource Manager Behavior

The example Resource Manager currently accepts any write size.

It does not really process the data yet.

It simply replies:

```text
I successfully wrote msg->i.nbytes bytes.
```

So if the client calls:

```c
write(fd, data, 100);
```

The example handler replies with status:

```text
100
```

Then the client sees:

```text
write() returns 100
```

The script says this is basically “lying” for now because the example does not really do anything with the data yet.

In the exercise, you are expected to actually process the data.

---

## 4. QNX Implementation of `write()`

In QNX, `write()` is implemented using message passing.

The client does not call a traditional monolithic-kernel write syscall.

Instead:

```text
client write()
    |
    v
libc builds _IO_WRITE message
    |
    v
MsgSendv() to Resource Manager
    |
    v
Resource Manager write handler
```

---

## 5. Why `MsgSendv()`?

The `write()` message has two parts:

```text
1. Header
2. Data buffer
```

The header tells the Resource Manager:

```text
Message type = _IO_WRITE
Number of bytes = nbytes
xtype = usually _IO_XTYPE_NONE
```

The data buffer contains the bytes the client wants to write.

QNX uses `MsgSendv()` because it can send multiple memory regions as one message.

---

## 6. IOV Layout Used by `write()`

`MsgSendv()` uses an IOV array.

For `write()`, the IOV array has two parts:

```text
IOV[0] -> _IO_WRITE header
IOV[1] -> client data buffer
```

Diagram:

```text
Client write() message
+--------------------------+
| struct _io_write header  |
+--------------------------+
| client data buffer       |
+--------------------------+
```

So the Resource Manager receives:

```text
header followed by data
```

---

## 7. `_IO_WRITE` Message

The message type is:

```text
_IO_WRITE
```

The Resource Manager receives this inside an `io_write_t`.

Important field:

```c
msg->i.nbytes
```

Meaning:

```text
The number of bytes the client is trying to write.
```

Example:

```c
write(fd, buffer, 256);
```

Inside the Resource Manager write handler:

```text
msg->i.nbytes = 256
```

---

## 8. `io_write_t` Is a Union

The handler receives:

```c
io_write_t *msg
```

This is a union that includes the real `_io_write` structure.

The common access pattern is:

```c
msg->i.nbytes
msg->i.xtype
```

The `i` member overlays the normal input header.

---

## 9. Write Handler Prototype

A typical write handler has this shape:

```c
int io_write(resmgr_context_t *ctp, io_write_t *msg, iofunc_ocb_t *ocb);
```

Parameters:

```text
ctp:
    Context pointer.
    Contains rcvid and receive information.

msg:
    Pointer to the received _IO_WRITE message.

ocb:
    Open Control Block.
    Per-open state for this file descriptor.
```

---

## 10. `ctp`

`ctp` contains message receive information.

Most important field for this script:

```c
ctp->rcvid
```

This is used when replying:

```c
MsgReply(ctp->rcvid, status, reply_buffer, reply_size);
```

---

## 11. `ocb`

`ocb` means:

```text
Open Control Block
```

There is one OCB per successful `open()`.

It tracks per-open state such as:

```text
open mode
permissions
offset
device attributes pointer
client-specific state
```

The write handler uses it to verify that the client is allowed to write.

---

## 12. First Step: `iofunc_write_verify()`

The first thing a write handler should normally do is:

```c
status = iofunc_write_verify(ctp, msg, ocb, NULL);
```

This checks POSIX rules that you do not want to manually handle.

Example:

```text
Client opened the device read-only.
Client now calls write().
This should fail.
```

`iofunc_write_verify()` handles checks like that.

Typical pattern:

```c
int status;

status = iofunc_write_verify(ctp, msg, ocb, NULL);
if (status != EOK) {
    return status;
}
```

If it fails, return the error value.

The Resource Manager framework will make the client `write()` fail with:

```text
write() returns -1
errno = returned error
```

---

## 13. Check `xtype`

The normal QNX `write()` library call sets:

```c
msg->i.xtype = _IO_XTYPE_NONE;
```

The handler should check this:

```c
if (msg->i.xtype != _IO_XTYPE_NONE) {
    return ENOSYS;
}
```

Meaning:

```text
If this is not a normal write request, reject it as unsupported.
```

`ENOSYS` means:

```text
Functionality not supported.
```

Then the client sees:

```text
write() returns -1
errno = ENOSYS
```

---

## 14. Where Is the Data?

The client sends:

```text
header + data
```

So the data normally follows the `io_write_t` header in the receive buffer.

Conceptually:

```text
msg points here
    |
    v
+--------------------------+
| io_write_t header        |
+--------------------------+
| write data begins here   |
+--------------------------+
```

A common idea is:

```c
data = msg + 1;
```

Because:

```text
msg + 1 means:
    move forward by sizeof(*msg)
```

Not by one byte.

So it points after the header.

However, this is only safe if all data was actually received in the receive buffer.

That may not always be true.

---

## 15. Replying to `write()`

For `write()`, the client sends data to the Resource Manager.

The client usually does not expect data back.

So the Resource Manager replies with:

```c
MsgReply(ctp->rcvid, bytes_written, NULL, 0);
```

Example:

```c
MsgReply(ctp->rcvid, msg->i.nbytes, NULL, 0);
```

Meaning:

```text
status = msg->i.nbytes
reply data = none
reply data length = 0
```

The status becomes the return value of the client `write()`.

So:

```text
MsgReply status = 100
        |
        v
client write() returns 100
```

---

## 16. Return `_RESMGR_NOREPLY`

After calling `MsgReply()` manually, the handler must tell the framework not to reply again.

Use:

```c
return _RESMGR_NOREPLY;
```

Meaning:

```text
I already replied.
Framework should not send another reply.
```

---

## 17. Error Handling

If something fails, return an errno value from the handler.

Examples:

```c
return EIO;
return EPERM;
return ENOSYS;
return ENOMEM;
```

The framework will internally reply with an error.

The client sees:

```text
write() returns -1
errno = returned errno value
```

Example:

```text
Resource Manager returns EIO
        |
        v
client write() returns -1
errno = EIO
```

---

## 18. Write Time Updates

A write operation affects file/device timestamps.

Relevant timestamps:

```text
mtime:
    modification time

ctime:
    status change time
```

The Resource Manager could update these immediately by getting the current time.

But that requires an extra kernel call on every write.

That can hurt performance.

Instead, most Resource Managers mark the times for updating later.

The default `stat()` handler can update them when a client calls `stat()`.

---

## 19. Only Mark Times for Real Writes

If the client writes 0 bytes:

```c
write(fd, buffer, 0);
```

That should not count as a real write for timestamp updates.

So the handler checks:

```c
if (msg->i.nbytes > 0) {
    mark mtime and ctime for update;
}
```

Meaning:

```text
If the client wrote real data:
    mark write-related times for update.

If the client wrote 0 bytes:
    do not update times.
```

---

## 20. Basic Example Write Handler Logic

Conceptual flow:

```text
io_write()
    |
    v
iofunc_write_verify()
    |
    +-- failed:
    |       return error
    |
    v
check xtype == _IO_XTYPE_NONE
    |
    +-- unexpected:
    |       return ENOSYS
    |
    v
get data
    |
    v
process data
    |
    v
if nbytes > 0:
    mark mtime/ctime for update
    |
    v
MsgReply(status = bytes_written, no data)
    |
    v
return _RESMGR_NOREPLY
```

Pseudo-code:

```c
int io_write(resmgr_context_t *ctp, io_write_t *msg, iofunc_ocb_t *ocb)
{
    int status;

    status = iofunc_write_verify(ctp, msg, ocb, NULL);
    if (status != EOK) {
        return status;
    }

    if (msg->i.xtype != _IO_XTYPE_NONE) {
        return ENOSYS;
    }

    /*
     * In the simple example, the Resource Manager does not really
     * process the data yet. It only pretends that all bytes were written.
     */

    if (msg->i.nbytes > 0) {
        /*
         * Mark mtime and ctime for update.
         * The real code uses the proper QNX IOFUNC flags.
         */
    }

    MsgReply(ctp->rcvid, msg->i.nbytes, NULL, 0);

    return _RESMGR_NOREPLY;
}
```

---

## 21. The Big Problem: Large Writes

The script then warns about a real issue:

```text
The data may not all be in the receive buffer.
```

Why?

Because QNX message passing copies the smaller of:

```text
bytes sent by client
bytes the server receive buffer can hold
```

Example:

```text
Client sends 3000 bytes
Resource Manager receive buffer can hold 1000 bytes

Result:
    only first 1000 bytes are received initially
```

So if the client writes a lot of data, the Resource Manager may initially receive only part of it.

---

## 22. Why `msg + 1` Is Not Always Enough

The data usually starts after the header:

```c
msg + 1
```

But that only points to the data already in the receive buffer.

If the client sent more data than the receive buffer could hold:

```text
msg + 1 gives access only to the first part
```

The rest must be fetched separately.

---

## 23. Message Copy Example

Client:

```text
write size = 3000 bytes
```

Resource Manager receive buffer:

```text
capacity = 1000 bytes
```

Kernel copies:

```text
first 1000 bytes only
```

Diagram:

```text
Client sends
+----------------------------------------------+
| header | data byte 0 ... data byte 2999      |
+----------------------------------------------+

Resource Manager receive buffer gets
+-------------------------------+
| header | first part of data    |
+-------------------------------+

Remaining data still must be read
```

---

## 24. Solution: `resmgr_msgget()`

For Resource Managers, QNX provides:

```c
resmgr_msgget()
```

Purpose:

```text
Get the client write data safely, even if it was not all received initially.
```

It is a convenience function that:

```text
copies locally from the receive buffer if possible
calls MsgRead() only if more data is needed
```

---

## 25. `resmgr_msgget()` Inputs

Conceptually:

```c
resmgr_msgget(ctp, destination_buffer, bytes_to_get, offset);
```

You provide:

```text
destination_buffer:
    Where to store the client data

bytes_to_get:
    How many bytes you want to get

offset:
    Where the data starts inside the message
```

For write data, the offset should skip the header:

```c
sizeof(io_write_t)
```

Because you do not want to copy the `_IO_WRITE` header into your data buffer.

You only want the actual client data.

---

## 26. Using `msg->i.nbytes`

The number of data bytes the client wants to write is:

```c
msg->i.nbytes
```

So a simple buffer allocation could be:

```c
char *buf = malloc(msg->i.nbytes);
```

Then:

```c
n = resmgr_msgget(ctp, buf, msg->i.nbytes, sizeof(io_write_t));
```

Conceptually:

```text
Get msg->i.nbytes bytes
starting after the write header
and put them into buf.
```

---

## 27. `resmgr_msgget()` Is Optimized

It is smart:

```text
1. Copy what is already in the local receive buffer.
2. If all data is already there, no kernel call is needed.
3. If more data is needed, call MsgRead() to get the rest.
```

Diagram:

```text
resmgr_msgget()
        |
        v
Is requested data already in receive buffer?
        |
        +-- yes:
        |       memcpy locally
        |
        +-- no:
                memcpy local part
                MsgRead() remaining part
```

This avoids unnecessary kernel calls.

---

## 28. `resmgr_msggetv()`

There is also a vector version:

```c
resmgr_msggetv()
```

Use it when you want to copy client data into multiple destination buffers.

Example use cases:

```text
multiple cache buffers
ring buffer segments
scatter buffers
hardware buffers
```

It is similar in spirit to vector message APIs such as:

```text
MsgSendv()
MsgReceivev()
MsgReplyv()
```

---

## 29. Safer Write Handler with Data Processing

Conceptual flow for a real write handler:

```text
1. Verify write permissions.
2. Check xtype.
3. Allocate or select destination buffer.
4. Use resmgr_msgget() to get all client data.
5. Process the data.
6. Mark mtime/ctime if nbytes > 0.
7. Reply with number of bytes processed.
8. Return _RESMGR_NOREPLY.
```

Pseudo-code:

```c
int io_write(resmgr_context_t *ctp, io_write_t *msg, iofunc_ocb_t *ocb)
{
    int status;
    char *buf;
    int got;

    status = iofunc_write_verify(ctp, msg, ocb, NULL);
    if (status != EOK) {
        return status;
    }

    if (msg->i.xtype != _IO_XTYPE_NONE) {
        return ENOSYS;
    }

    if (msg->i.nbytes == 0) {
        MsgReply(ctp->rcvid, 0, NULL, 0);
        return _RESMGR_NOREPLY;
    }

    buf = malloc(msg->i.nbytes);
    if (buf == NULL) {
        return ENOMEM;
    }

    got = resmgr_msgget(ctp, buf, msg->i.nbytes, sizeof(io_write_t));
    if (got < 0) {
        free(buf);
        return errno;
    }

    /*
     * Process the received data here.
     * Example:
     *     write it to a log
     *     copy it into a device buffer
     *     send it to hardware
     */

    free(buf);

    /*
     * Mark mtime and ctime for update here.
     */

    MsgReply(ctp->rcvid, msg->i.nbytes, NULL, 0);

    return _RESMGR_NOREPLY;
}
```

Note:

```text
This pseudo-code is for learning.
Real production code should carefully handle partial processing, memory limits, and device-specific rules.
```

---

## 30. Example: Logger Resource Manager

Client:

```c
write(fd, "System started", 14);
```

Resource Manager:

```text
1. Receives _IO_WRITE
2. msg->i.nbytes = 14
3. Gets 14 bytes using resmgr_msgget()
4. Stores the text in a log buffer
5. Replies status = 14
```

Client sees:

```text
write() returns 14
```

---

## 31. Example: Frame Buffer Resource Manager

Client:

```c
write(fd, pixels, pixel_count);
```

Resource Manager:

```text
1. Receives _IO_WRITE
2. Gets pixel data
3. Checks if data fits in frame buffer
4. If it fits:
       copy pixels
       reply bytes written
5. If it does not fit:
       return error
```

Possible error:

```text
ENOSPC
EIO
EINVAL
```

depending on the device behavior.

---

## 32. Key Difference Between `read()` and `write()`

```text
read():
    Client asks Resource Manager for data.
    Resource Manager replies with data.
    MsgReply status = number of bytes read.
    MsgReply data size = number of bytes returned.

write():
    Client sends data to Resource Manager.
    Resource Manager usually replies with no data.
    MsgReply status = number of bytes written.
    MsgReply data size = 0.
```

Table:

```text
Operation     Direction of data          Reply data?
====================================================
read()        Resource Manager -> Client yes
write()       Client -> Resource Manager no
```

---

## 33. Common Mistakes

### Mistake 1: Thinking `write()` returning 0 means EOF

Wrong.

```text
EOF is a read concept.
```

For write, 0 usually means 0 bytes were written.

---

### Mistake 2: Forgetting `iofunc_write_verify()`

Always verify first.

It handles POSIX permission checks.

---

### Mistake 3: Not checking `xtype`

If `xtype` is not `_IO_XTYPE_NONE`, return:

```c
ENOSYS
```

---

### Mistake 4: Replying twice

If you call:

```c
MsgReply()
```

then return:

```c
_RESMGR_NOREPLY
```

Do not let the framework reply again.

---

### Mistake 5: Assuming all write data is in the receive buffer

For large writes, only part may be received.

Use:

```c
resmgr_msgget()
```

---

### Mistake 6: Copying the header as data

The actual write data starts after:

```c
sizeof(io_write_t)
```

Do not process the header as part of the client data.

---

## 34. Final Flow Diagram

```text
Client write(fd, buffer, nbytes)
        |
        v
libc creates:
    _IO_WRITE header
    client data
        |
        v
MsgSendv()
        |
        v
Resource Manager receive buffer
        |
        v
io_write handler
        |
        +-- iofunc_write_verify()
        |
        +-- check xtype
        |
        +-- get nbytes from msg->i.nbytes
        |
        +-- get data:
        |       msg + 1 if enough received
        |       or resmgr_msgget() for safe full copy
        |
        +-- process data
        |
        +-- mark mtime/ctime if nbytes > 0
        |
        +-- MsgReply(status = bytes_written, data = NULL, len = 0)
        |
        v
client write() returns bytes_written
```

---

## 35. Final Summary

```text
write() in QNX:
    is implemented with message passing

client write():
    sends _IO_WRITE header + data

MsgSendv():
    sends header and data as two IOV parts

write handler receives:
    ctp
    msg
    ocb

msg->i.nbytes:
    number of bytes client wants to write

iofunc_write_verify():
    checks POSIX write validity

xtype:
    should be _IO_XTYPE_NONE

MsgReply status:
    becomes client's write() return value

_RES MGR_NOREPLY:
    used because the handler already replied

large writes:
    may not fully fit in receive buffer

resmgr_msgget():
    safely retrieves all client data
```

Golden sentence:

```text
In QNX, write() sends an _IO_WRITE message containing a header and client data; the Resource Manager verifies the request, retrieves the data safely, processes it, then replies with the number of bytes successfully written.
```