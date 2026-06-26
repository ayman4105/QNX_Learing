# QNX IPC — Multi-part Messages, IOV, MsgSendv, MsgReadv

## Core Idea

Normal `MsgSend()` sends one continuous buffer.

Real messages are often built from multiple parts:

```text
header + payload
buffer1 + buffer2 + buffer3
request header + variable-length data
```

Instead of doing:

```text
malloc big buffer
memcpy part 1
memcpy part 2
memcpy part 3
MsgSend(big buffer)
```

QNX gives us **IOV** and vector message APIs.

Golden sentence:

```text
IOV and vector message APIs let QNX move multi-part messages efficiently without forcing the client or server to assemble one big temporary buffer.
```

---

## 1. The Problem

Example client data:

```text
buffer 1 = 750 KB
buffer 2 = 500 KB
buffer 3 = 1000 KB
```

Total:

```text
2.25 MB
```

With normal `MsgSend()`, you would need one big continuous buffer.

Bad approach:

```c
big = malloc(total_size);

memcpy(big, buf1, size1);
memcpy(big + size1, buf2, size2);
memcpy(big + size1 + size2, buf3, size3);

MsgSend(coid, big, total_size, &reply, sizeof(reply));
```

Problems:

```text
extra memory
extra copy
more overhead
```

---

## 2. IOV

IOV means:

```text
Input / Output Vector
```

Each IOV entry describes one buffer:

```text
pointer + length
```

Conceptually:

```c
typedef struct
{
    void   *iov_base;
    size_t  iov_len;
} iov_t;
```

The IOV does not contain the data itself. It only describes where the data is and how large it is.

---

## 3. SETIOV

QNX provides a helper macro:

```c
SETIOV(&iov[index], buffer_address, buffer_length);
```

Example:

```c
iov_t siov[3];

SETIOV(&siov[0], buf1, size1);
SETIOV(&siov[1], buf2, size2);
SETIOV(&siov[2], buf3, size3);
```

Important:

```text
SETIOV does not copy data.
It only sets pointer and length.
```

---

## 4. MsgSendv

`MsgSendv()` sends multiple buffers as one logical message.

```c
status = MsgSendv(coid, siov, sparts, riov, rparts);
```

Meaning:

```text
coid:   connection ID
siov:   send IOV array
sparts: number of send parts
riov:   reply IOV array
rparts: number of reply parts
```

Example:

```c
iov_t siov[3];

SETIOV(&siov[0], buf1, size1);
SETIOV(&siov[1], buf2, size2);
SETIOV(&siov[2], buf3, size3);

status = MsgSendv(coid, siov, 3, NULL, 0);
```

This sends:

```text
buf1 + buf2 + buf3
```

as one logical message.

---

## 5. Gather and Scatter

### Gather

Multiple buffers become one logical message.

```text
buf1 ----\
buf2 -----+----> one logical message
buf3 ----/
```

This happens on the send side with `MsgSendv()`.

### Scatter

One logical message is copied into multiple destination buffers.

```text
one logical message
        |
        +--> buffer 1
        +--> buffer 2
        +--> buffer 3
```

This happens with `MsgReceivev()` or `MsgReadv()`.

---

## 6. Classic Example: write()

User calls:

```c
write(fd, buffer, nbytes);
```

In QNX, `fd` is a connection ID in the file descriptor range.

Internally, `write()` sends a message to the resource manager.

The message looks like:

```text
write header + user data
```

Header contains things like:

```text
type = _IO_WRITE
nbytes = number of bytes to write
```

Instead of building one big buffer, the C library can use:

```c
iov_t siov[2];

SETIOV(&siov[0], &header, sizeof(header));
SETIOV(&siov[1], buffer, nbytes);

MsgSendv(fd, siov, 2, NULL, 0);
```

This avoids copying the user buffer into a temporary larger buffer.

---

## 7. MsgReceivev

The server can receive into multiple buffers:

```c
rcvid = MsgReceivev(chid, riov, rparts, &info);
```

Example:

```c
iov_t riov[4];

SETIOV(&riov[0], &header, sizeof(header));
SETIOV(&riov[1], buf1, 4096);
SETIOV(&riov[2], buf2, 4096);
SETIOV(&riov[3], buf3, 4096);

rcvid = MsgReceivev(chid, riov, 4, &info);
```

This can scatter received data into several buffers.

---

## 8. Why Servers Often Receive Only Header First

In real servers, payload size is usually unknown before reading the header.

The client may send:

```text
1 byte
100 bytes
12 KB
2 MB
200 MB
```

So the server should not allocate a huge receive buffer blindly.

Better design:

```text
1. receive fixed header only
2. read nbytes from header
3. allocate/select suitable buffers
4. use MsgRead() or MsgReadv() to get payload
```

Flow:

```text
Client:
    MsgSendv(header + payload)

Server:
    MsgReceive(header only)
    inspect header.nbytes
    MsgRead / MsgReadv(payload)
    process data
    MsgReply()
```

---

## 9. MsgRead

`MsgRead()` lets the server copy more data from the client's original send buffer.

```c
bytes = MsgRead(rcvid, buffer, length, offset);
```

Meaning:

```text
rcvid:  which REPLY-blocked client/message to read from
buffer: destination buffer in server memory
length: bytes requested
offset: where to start in the original client message
```

Example:

```c
bytes = MsgRead(rcvid, payload, payload_size, sizeof(header));
```

This means:

```text
skip header
copy payload
```

---

## 10. MsgReadv

`MsgReadv()` reads more data from the client and scatters it into multiple server buffers.

```c
bytes = MsgReadv(rcvid, riov, rparts, offset);
```

Example:

```c
iov_t riov[3];

SETIOV(&riov[0], buf1, 4096);
SETIOV(&riov[1], buf2, 4096);
SETIOV(&riov[2], buf3, 4096);

bytes = MsgReadv(rcvid, riov, 3, sizeof(header));
```

This reads payload after the header and scatters it into three buffers.

---

## 11. Important MsgRead Rules

`MsgRead()` is possible only while the client is still:

```text
REPLY blocked
```

After:

```c
MsgReply(rcvid, ...);
```

or:

```c
MsgError(rcvid, ...);
```

the server can no longer read from that client's send buffer.

Correct order:

```text
MsgReceive()
MsgRead() / MsgReadv()
process
MsgReply()
```

Wrong order:

```text
MsgReceive()
MsgReply()
MsgRead()
```

---

## 12. MsgRead Is Not a Stream

`MsgRead()` is not destructive.

You can read the same data again.

The kernel does not remember your last read position.

You must give the offset every time.

Example:

```c
MsgRead(rcvid, buf1, 4096, sizeof(header));
MsgRead(rcvid, buf2, 4096, sizeof(header) + 4096);
MsgRead(rcvid, buf3, 4096, sizeof(header) + 8192);
```

---

## 13. MsgRead Return Value

`MsgRead()` returns the number of bytes actually copied.

Rule:

```text
copied = min(requested length, available bytes from offset)
```

Example:

```text
message size = 12 KB
MsgRead length = 8 KB
offset = 8 KB
available = 4 KB
return = 4 KB
```

If offset is past the end:

```text
message size = 12 KB
offset = 16 KB
return = 0
```

Return `0` here is not an error.

---

## 14. MsgWrite

`MsgWrite()` copies data from the server into the client's reply buffer.

```c
bytes = MsgWrite(rcvid, buffer, length, offset);
```

Meaning:

```text
rcvid:  which REPLY-blocked client
buffer: server source data
length: bytes to write
offset: where to write inside client reply buffer
```

Important:

```text
MsgWrite does not unblock the client.
```

You must still call:

```c
MsgReply(rcvid, status, NULL, 0);
```

or:

```c
MsgError(rcvid, error);
```

---

## 15. MsgWritev

`MsgWritev()` copies multiple server buffers into the client's reply buffer.

```c
bytes = MsgWritev(rcvid, wiov, wparts, offset);
```

Example:

```c
iov_t wiov[2];

SETIOV(&wiov[0], part1, part1_size);
SETIOV(&wiov[1], part2, part2_size);

bytes = MsgWritev(rcvid, wiov, 2, sizeof(reply_header));
```

---

## 16. MsgReply vs MsgWrite

`MsgReply()`:

```text
copies optional reply data
writes at the start of client reply buffer
unblocks the client
has no offset
```

`MsgWrite()`:

```text
copies reply data at an offset
does not unblock the client
must be followed by MsgReply() or MsgError()
```

---

## 17. Function Variants

| Function      | Send Side | Reply Side |
| ------------- | --------- | ---------- |
| `MsgSend()`   | single    | single     |
| `MsgSendv()`  | vector    | vector     |
| `MsgSendvs()` | vector    | single     |
| `MsgSendsv()` | single    | vector     |

Remember:

```text
s = single buffer
v = vector / IOV array
```

Other vector variants:

```text
MsgReceivev()
MsgReplyv()
MsgReadv()
MsgWritev()
```

---

## 18. Full Example Flow

Client sends:

```text
header + payload
```

Server receives:

```text
header first
```

Server reads:

```text
payload later
```

Server replies:

```text
status / optional reply data
```

Flow:

```text
Client:
    SETIOV(header)
    SETIOV(payload)
    MsgSendv()

Server:
    MsgReceive(header only)
    MsgReadv(payload)
    process payload
    MsgReply()
```

---

## 19. Slow Device Example

For a slow device, the server does not need to buffer all payload.

Example:

```text
client sends 200 KB
server uses one 4 KB chunk buffer
```

Server loop:

```text
read 4 KB using MsgRead()
write 4 KB to hardware
read next 4 KB
write next 4 KB to hardware
repeat
```

This reduces server memory usage.

---

## 20. Copy Rules

### MsgReceive

Copies from the start of client send message.

```text
copied = min(client send bytes, server receive buffer bytes)
```

### MsgRead

Copies from client send message at offset.

```text
copied = min(requested bytes, available bytes from offset)
```

### MsgWrite

Copies into client reply buffer at offset.

```text
written = min(requested bytes, available reply space from offset)
```

### MsgReply

Copies optional reply data to the start of client reply buffer and unblocks client.

```text
no offset
ends the transaction
```

---

## 21. Common Mistakes

### Mistake 1: Copying data manually into one big buffer

Use IOV when data is already split.

### Mistake 2: Thinking SETIOV copies data

`SETIOV` only sets pointer and length.

### Mistake 3: Calling MsgReply before MsgRead

After `MsgReply`, the client is no longer REPLY blocked.

### Mistake 4: Thinking MsgRead is a stream

It is not a stream. It is an offset-based memory copy.

### Mistake 5: Forgetting MsgWrite does not unblock the client

Always finish with `MsgReply()` or `MsgError()`.

### Mistake 6: Using fixed-size payload inside header

For variable-length data, use:

```text
fixed header + payload
```

not:

```c
char data[4096];
```

inside every message struct.

---

## 22. Quick Quiz

### Q1: What is an IOV?

```text
A descriptor containing a pointer and a length.
```

### Q2: Does SETIOV copy data?

```text
No. It only sets pointer and length.
```

### Q3: What does the `v` in MsgSendv mean?

```text
Vector.
```

### Q4: Why use MsgSendv?

```text
To send multiple buffers as one logical message without copying them into one big buffer first.
```

### Q5: Why does a server often receive only the header first?

```text
Because it does not know the variable payload size before reading the header.
```

### Q6: What does MsgRead need?

```text
rcvid, destination buffer, length, offset.
```

### Q7: Can the server call MsgRead after MsgReply?

```text
No. The client is no longer REPLY blocked.
```

### Q8: Is MsgRead destructive?

```text
No. You can read the same data again.
```

### Q9: What does MsgWrite do?

```text
It writes data into the client's reply buffer at an offset.
```

### Q10: Does MsgWrite unblock the client?

```text
No. MsgReply or MsgError is still required.
```

### Q11: Does MsgReply have an offset?

```text
No. It writes at the start of the client reply buffer.
```

### Q12: What does `s` and `v` mean in MsgSendvs or MsgSendsv?

```text
s = single buffer
v = vector / IOV array
```

---

## Final Summary

```text
IOV:
    pointer + length descriptor

MsgSendv:
    send multiple buffers as one logical message

MsgReceivev:
    receive message into multiple buffers

MsgRead / MsgReadv:
    read more data from REPLY-blocked client send buffer using offset

MsgWrite / MsgWritev:
    write data into REPLY-blocked client reply buffer using offset

MsgReply / MsgReplyv:
    finish the transaction and unblock the client
```

Golden sentence:

```text
IOV and vector message APIs let QNX move multi-part messages efficiently, avoiding extra local copies while preserving process isolation.
```