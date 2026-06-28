# QNX Resource Manager `read()` Handler — Quick README

## 1. Goal of This Section

This section explains how a QNX Resource Manager handles `read()` requests from a client.

The example device is:

```text
/dev/example
```

The example Resource Manager currently behaves like this:

```text
read()  -> returns 0 bytes
write() -> accepts any size
```

Returning `0` from `read()` means **end of file** in POSIX.

---

## 2. Example: `cat /dev/example`

A simplified version of what `cat` does:

```c
fd = open("/dev/example", O_RDONLY);

while ((n = read(fd, buf, size)) > 0) {
    write(STDOUT_FILENO, buf, n);
}

close(fd);
```

Flow:

```text
cat /dev/example
      |
      v
open("/dev/example")
      |
      v
read(fd, buf, size)
      |
      +-- Resource Manager returns data > 0
      |       cat prints data and reads again
      |
      +-- Resource Manager returns 0
              cat sees EOF and exits loop
```

In the initial example, `read()` returns `0`, so `cat` exits immediately.

---

## 3. What Happens Under QNX

`open()` is pathname-based and starts by asking `procnto` who owns the path.

```text
cat
 |
 | open("/dev/example")
 v
procnto
 |
 | returns Resource Manager PID + CHID
 v
cat/libc
 |
 | sends open message
 v
example Resource Manager
 |
 | replies success
 v
cat gets fd
```

After `open()` succeeds, `read()` goes directly to the Resource Manager:

```text
cat
 |
 | read(fd, buf, size)
 v
example Resource Manager
 |
 | replies with data or 0 bytes
 v
cat
```

---

## 4. `read()` and `write()` Handler Prototypes

Conceptually, handlers look like this:

```c
int io_read(resmgr_context_t *ctp, io_read_t *msg, iofunc_ocb_t *ocb);
int io_write(resmgr_context_t *ctp, io_write_t *msg, iofunc_ocb_t *ocb);
```

The three important parameters are:

```text
ctp -> dispatch context pointer
msg -> received I/O message
ocb -> Open Control Block for this open()
```

---

## 5. `ctp` — Context Pointer

`ctp` contains information about the received message.

Important fields include:

```text
ctp->rcvid
    receive ID used when replying to the client

ctp->info
    message information such as client PID, TID, SCOID, message lengths

ctp->msg
    pointer to the receive buffer

ctp->msg_max_size
    size of the receive buffer
```

You need `ctp->rcvid` when using `MsgReply()`:

```c
MsgReply(ctp->rcvid, status, buffer, size);
```

---

## 6. `ocb` — Open Control Block

`OCB` means:

```text
Open Control Block
```

There is one OCB per successful `open()`.

```text
Client fd  -------------------->  Resource Manager OCB
```

The OCB stores per-open state, such as:

```text
open mode
current offset
client-specific state
pointer to device attributes
```

The default open handler, such as `iofunc_open_default()`, allocates and initializes the OCB.

---

## 7. Per-Open Data Example: File Offset

If a client reads from a file-like device:

```text
first read  -> starts at offset 0
second read -> starts where first read stopped
third read  -> starts where second read stopped
```

That current position is per-open data.

If the same process opens the same device twice:

```c
fd1 = open("/dev/example", O_RDONLY);
fd2 = open("/dev/example", O_RDONLY);
```

Then each open gets a separate OCB:

```text
fd1 -> OCB #1 offset = 0
fd2 -> OCB #2 offset = 0
```

---

## 8. Device Attributes from OCB

The `read()` and `write()` handlers receive the OCB, not the device attribute structure directly.

But the OCB points to the device attributes:

```text
ocb -> attr
```

Device attributes can contain data such as:

```text
buffers
interrupt IDs
timer IDs
hardware state
device metadata
file timestamps
```

---

## 9. POSIX `read()` Return Values

Client-side prototype:

```c
ssize_t read(int fd, void *buf, size_t nbytes);
```

Possible return values:

```text
-1
    error occurred, errno is set

0
    end of file, or a valid zero-byte read

1 to nbytes
    number of bytes successfully read
```

If the client asks for zero bytes:

```c
read(fd, buf, 0);
```

Then `read()` normally returns `0` if read permission is valid.

If the fd does not allow reading, the operation fails.

---

## 10. QNX `read()` Implementation

In QNX, `read()` is not implemented as a traditional Unix kernel read syscall.

Instead, libc builds an `_IO_READ` message and sends it to the Resource Manager.

```text
client read()
    |
    v
libc builds _IO_READ message
    |
    v
MsgSend() to Resource Manager
    |
    v
Resource Manager read handler
    |
    v
MsgReply()
    |
    v
read() returns
```

---

## 11. `_IO_READ` Message

The read message contains:

```text
message type = _IO_READ
xtype        = _IO_XTYPE_NONE in normal cases
nbytes       = number of bytes the client wants
```

In the handler, `msg` is an `io_read_t *`.

`io_read_t` is a union that contains the actual `_io_read` structure.

To access the requested byte count:

```c
msg->i.nbytes
```

Example:

```c
read(fd, buf, 100);
```

Then inside the handler:

```text
msg->i.nbytes = 100
```

---

## 12. Replying from the `read()` Handler

The clearest method is to reply explicitly using `MsgReply()`.

```c
MsgReply(ctp->rcvid, status, data_buffer, data_size);
```

Parameters:

```text
ctp->rcvid
    who to reply to

status
    value returned by MsgSend()
    for read(), this becomes read() return value

data_buffer
    data copied back to the client

data_size
    number of bytes copied to the client buffer
```

For `read()`, the status and data size are usually the same value.

Example:

```text
Resource Manager returns 20 bytes

MsgReply status = 20
MsgReply data size = 20

Client read() returns 20
Client buffer receives 20 bytes
```

They are equal for `read()`, but they mean different things.

```text
status:
    what read() returns

data size:
    how many bytes are copied into the client's buffer
```

---

## 13. Returning EOF

To return EOF:

```c
MsgReply(ctp->rcvid, 0, NULL, 0);
return _RESMGR_NOREPLY;
```

Meaning:

```text
status = 0
    read() returns 0

buffer = NULL
size = 0
    no data returned
```

POSIX meaning:

```text
read() returned 0 -> EOF
```

---

## 14. `_RESMGR_NOREPLY`

If your handler already called `MsgReply()`, return:

```c
_RESMGR_NOREPLY
```

This tells the Resource Manager framework:

```text
Do not reply again.
I already replied manually.
```

This prevents double replies.

---

## 15. Error Handling

If something fails in the handler, return an errno value.

Example:

```c
return EPERM;
return EIO;
return ENOSYS;
```

The framework will call `MsgError()` internally.

Effect on the client:

```text
MsgSend() returns -1
errno is set
read() returns -1
errno is set
```

So in a Resource Manager handler:

```text
return errno_value;
```

means:

```text
client function fails with that errno
```

---

## 16. `xtype` Check

The read message has an `xtype` field.

Normally, it should be:

```c
_IO_XTYPE_NONE
```

A simple handler should check it:

```c
if (msg->i.xtype != _IO_XTYPE_NONE) {
    return ENOSYS;
}
```

Meaning:

```text
I only support normal read requests.
If xtype is something else, return ENOSYS.
```

Client effect:

```text
read() returns -1
errno = ENOSYS
```

---

## 17. `iofunc_read_verify()`

A good read handler should start with:

```c
status = iofunc_read_verify(ctp, msg, ocb, NULL);
if (status != EOK) {
    return status;
}
```

This checks common rules such as:

```text
Was the fd opened with read permission?
Is this read request valid?
```

Example:

```text
Client opened fd as write-only
Client calls read(fd)

iofunc_read_verify() catches this
handler returns error
client read() fails
```

---

## 18. Access Time Update

A successful read should usually update the access time (`atime`) of the device.

Doing an actual time update on every read may require an extra kernel call, which can hurt performance.

Instead, many Resource Managers mark the access time as needing an update.

Conceptually:

```c
if (msg->i.nbytes > 0) {
    ocb->attr->flags |= ATIME_UPDATE_FLAG;
}
```

The exact flag name depends on the QNX headers, but the idea is:

```text
Mark access time for update later.
```

Then if a client calls `stat()`, the default stat handler can update the time.

If the client requested zero bytes:

```c
read(fd, buf, 0);
```

Do not mark access time, because no actual data was read.

---

## 19. Full Read Handler Logic

The example read handler logic is:

```text
1. Verify the read request with iofunc_read_verify()
2. Check xtype is _IO_XTYPE_NONE
3. If nbytes > 0, mark access time for update
4. Reply with 0 bytes because example device has no data
5. Return _RESMGR_NOREPLY because we already replied
```

Pseudo-code:

```c
int io_read(resmgr_context_t *ctp, io_read_t *msg, iofunc_ocb_t *ocb)
{
    int status;

    status = iofunc_read_verify(ctp, msg, ocb, NULL);
    if (status != EOK) {
        return status;
    }

    if (msg->i.xtype != _IO_XTYPE_NONE) {
        return ENOSYS;
    }

    if (msg->i.nbytes > 0) {
        /* Mark access time for update */
        ocb->attr->flags |= ATIME_UPDATE_FLAG;
    }

    MsgReply(ctp->rcvid, 0, NULL, 0);

    return _RESMGR_NOREPLY;
}
```

The real QNX code uses the actual access-time flag from the QNX headers.

---

## 20. What Happens with `cat /dev/example`

Because the example handler replies with 0 bytes:

```text
cat opens /dev/example
cat calls read()
Resource Manager replies status = 0, data size = 0
cat sees EOF
cat closes fd
cat exits
```

Diagram:

```text
cat                         Resource Manager
================================================
open("/dev/example")  --->  open success
read(fd, buf, size)   --->  MsgReply(status=0, data=NULL, size=0)
cat sees EOF
close(fd)             --->  close handled
```

---

## 21. Final Summary

```text
cat reads until read() returns 0.
read() returning 0 means EOF.

In QNX:
    read() builds an _IO_READ message.
    The message is sent to the Resource Manager.
    The read handler receives ctp, msg, and ocb.

ctp:
    contains rcvid and message information.

msg:
    contains the read request.
    msg->i.nbytes tells how many bytes the client wants.

ocb:
    per-open state.
    also points to device attributes.

MsgReply status:
    becomes the return value of read().

MsgReply data size:
    tells the kernel how many bytes to copy to the client buffer.

Errors:
    return errno value from the handler.

_NORMAL xtype:
    _IO_XTYPE_NONE.
    unsupported xtype should return ENOSYS.

Example handler:
    verifies request
    checks xtype
    marks atime if nbytes > 0
    replies with 0 bytes
    returns _RESMGR_NOREPLY
```

Golden sentence:

```text
In QNX, a client read() becomes an _IO_READ message sent to the Resource Manager; the read handler replies with status and optional data, and that status becomes the client's read() return value.
```