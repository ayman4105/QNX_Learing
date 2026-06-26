# QNX IPC — Client Information Structure `_msg_info` Quick README

## Core Idea

When a QNX server receives a message using `MsgReceive()`, it can also receive extra information about the client that sent the message.

This information is stored in:

```c
struct _msg_info
```

Golden sentence:

```text
Use _msg_info when the server needs to know who sent the message, how large it was, how much reply space the client has, or whether the client is allowed to use the service.
```

---

## 1. Normal MsgReceive() Without Client Info

In many examples, the server receives messages like this:

```c
rcvid = MsgReceive(chid, &msg, sizeof(msg), NULL);
```

The last parameter is:

```c
NULL
```

Meaning:

```text
I only want the message data.
I do not need extra client information.
```

---

## 2. MsgReceive() With Client Info

If the server needs information about the client, use:

```c
struct _msg_info info;

rcvid = MsgReceive(chid, &msg, sizeof(msg), &info);
```

Now `MsgReceive()` fills:

```text
1. msg:
    message data from the client

2. info:
    information about the client and message
```

Diagram:

```text
Client
  |
  | MsgSend()
  v
Kernel
  |
  | copies message data
  | fills _msg_info
  v
Server
  |
  | MsgReceive(chid, &msg, sizeof(msg), &info)
  v
Server gets:
    msg  -> request data
    info -> client/message information
```

---

## 3. Why Not Use MsgInfo() Every Time?

QNX also provides:

```c
MsgInfo()
```

This can get message information after receiving a message.

But this means an extra kernel call.

Less efficient style:

```c
rcvid = MsgReceive(chid, &msg, sizeof(msg), NULL);
MsgInfo(rcvid, &info);
```

Better style:

```c
rcvid = MsgReceive(chid, &msg, sizeof(msg), &info);
```

Why?

```text
MsgReceive() can receive the message and fill client info in one call.
```

---

## 4. Important Warning: Pulses Do Not Update _msg_info

`_msg_info` is valid for normal messages.

A normal message means:

```text
rcvid > 0
```

But for pulses:

```text
rcvid == 0
```

`_msg_info` is not updated reliably.

So this is wrong for pulses:

```c
if (rcvid == 0)
{
    printf("Pulse from PID=%d\n", info.pid);   /* Wrong / unreliable */
}
```

Correct rule:

```text
For messages:
    use _msg_info

For pulses:
    do not trust _msg_info
    use struct _pulse fields instead
```

---

## 5. Message vs Pulse Rule

```text
rcvid > 0:
    normal message
    info is valid

rcvid == 0:
    pulse
    info is not valid / not updated

rcvid == -1:
    error
```

Example:

```c
rcvid = MsgReceive(chid, &msg, sizeof(msg), &info);

if (rcvid == 0)
{
    /* Pulse received */
    /* Do not use info here */
}
else if (rcvid > 0)
{
    /* Normal message */
    /* info is valid here */
}
else
{
    /* Error */
}
```

---

## 6. What Is Inside `_msg_info`?

The `_msg_info` structure contains useful information such as:

```text
client process ID
client thread ID
channel ID
client connection ID
server connection ID
message priority
flags
message length
reply buffer length
```

Important fields conceptually:

| Field       | Meaning                                  |
| ----------- | ---------------------------------------- |
| `pid`       | Client process ID                        |
| `tid`       | Client thread ID that called `MsgSend()` |
| `chid`      | Channel ID used                          |
| `coid`      | Client-side connection ID                |
| `scoid`     | Server-side connection ID                |
| `priority`  | Priority of the client/message           |
| `msglen`    | Source message length                    |
| `dstmsglen` | Client reply buffer capacity             |
| `flags`     | Extra message flags                      |

---

## 7. `pid`

`pid` tells the server which process sent the message.

Example use:

```text
Log which client process sent the request.
```

Example log:

```text
Received request from PID=1234
```

---

## 8. `tid`

`tid` tells the server which thread inside the client process called `MsgSend()`.

Example use:

```text
Debug which client thread is sending requests.
```

Example log:

```text
Received request from PID=1234 TID=2
```

---

## 9. `chid`

`chid` tells which channel received the message.

This is useful if a server process has multiple channels.

```text
Server process
    |
    +-- channel 1
    +-- channel 2
```

The server can know which channel the message came through.

---

## 10. `coid`

`coid` is the connection ID from the client side.

The client uses `coid` when calling:

```c
MsgSend(coid, &msg, sizeof(msg), &reply, sizeof(reply));
```

Concept:

```text
client side:
    coid
```

---

## 11. `scoid`

`scoid` means:

```text
server connection ID
```

It is the server-side identifier for the client's connection.

Concept:

```text
Client side:
    coid

Kernel connection

Server side:
    scoid
```

Diagram:

```text
Client Process                         Server Process
--------------                         --------------
coid  ------------------------------>  scoid
        same connection relationship
```

---

## 12. Why `scoid` Is Important

The server can use `scoid` to track clients.

Example table:

```text
scoid 10:
    authenticated = true
    active requests = 2

scoid 11:
    authenticated = false
    active requests = 0
```

So the server can keep per-client data.

---

## 13. Using `scoid` For Cleanup

If a client disconnects or dies, the server may receive a disconnect pulse.

The server can use the `scoid` to know which client connection went away.

Flow:

```text
Client connects
Server stores client state using scoid
Client dies / disconnects
Kernel sends disconnect pulse
Server uses scoid
Server frees the client data
```

---

## 14. Using `_msg_info` For Authentication

The server may want to accept messages only from allowed clients.

Example reasons:

```text
only specific users can send commands
only trusted clients can connect
limit the number of clients
reject unknown clients
```

Basic flow:

```text
MsgReceive(..., &info)
check info.scoid / info.pid
get more client info if needed
allow or deny request
```

If denied:

```c
MsgError(rcvid, EACCES);
```

---

## 15. `ConnectClientInfo()`

If `_msg_info` is not enough, the server can call:

```c
ConnectClientInfo()
```

This can provide more information about the client, such as:

```text
user ID
group ID
permissions
privileges / abilities
```

The server can use this for access control.

Example flow:

```text
Server receives message
Server gets info.scoid
Server calls ConnectClientInfo()
Server checks UID/GID/permissions
Server accepts or rejects request
```

---

## 16. Message Length Information

`_msg_info` can tell the server the source message length.

This is useful because QNX copies the smaller of:

```text
client send size
server receive buffer size
```

Example:

```text
client sends 100 bytes
server receive buffer is 200 bytes

kernel copies 100 bytes
```

Another example:

```text
client sends 100 bytes
server receive buffer is 40 bytes

kernel copies 40 bytes
```

The server can use message length information to check:

```text
Did I receive the full message?
Is the message too small?
Do I need MsgRead() to read more data?
```

---

## 17. Validating Message Size

Good server code should check whether the received message is large enough.

Example:

```c
if (info.msglen < sizeof(my_msg_t))
{
    MsgError(rcvid, EINVAL);
    continue;
}
```

Meaning:

```text
If the client sent less data than expected, reject the request.
```

---

## 18. Reply Buffer Capacity

The client calls:

```c
MsgSend(coid, &msg, sizeof(msg), &reply, sizeof(reply));
```

So the client has a reply buffer with a known size.

`_msg_info` can tell the server how much reply space the client provided.

This helps the server avoid sending too much data.

Example:

```text
server wants to reply with 200 bytes
client reply buffer is 80 bytes
```

The server can decide:

```text
send only 80 bytes
or
return an error
or
use another protocol
```

---

## 19. Verification Uses

The server can use `_msg_info` for verification.

Examples:

```text
verify message length
verify reply buffer size
verify client identity
verify allowed command
verify request source
```

Example:

```c
if (info.dstmsglen < sizeof(my_reply_t))
{
    MsgError(rcvid, EMSGSIZE);
    continue;
}
```

Meaning:

```text
Client reply buffer is too small for the reply.
```

---

## 20. Logging / Debugging Uses

`_msg_info` is very useful in logs.

Instead of:

```text
Received request
```

Write:

```text
Received request from PID=1234 TID=2 SCOID=7 Priority=20
```

This helps answer:

```text
Which client sent the request?
Which thread sent it?
Which client is spamming?
Which priority is being used?
Which connection should be cleaned up?
```

---

## 21. Simple Server Example

```c
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <sys/neutrino.h>

typedef struct
{
    uint16_t type;              /* Message type */
    int value;                  /* Example payload */
} my_msg_t;

int main(void)
{
    int chid;                   /* Server channel ID */
    int rcvid;                  /* Receive ID from MsgReceive */
    my_msg_t msg;               /* Receive buffer */
    struct _msg_info info;      /* Client/message information */

    chid = ChannelCreate(0);    /* Create channel */

    if (chid == -1)
    {
        perror("ChannelCreate");
        return 1;
    }

    while (1)
    {
        rcvid = MsgReceive(chid, &msg, sizeof(msg), &info);  /* Receive message and info */

        if (rcvid == -1)
        {
            perror("MsgReceive");
            continue;
        }

        if (rcvid == 0)
        {
            /*
             * This is a pulse.
             * _msg_info is not reliable for pulses.
             * Do not reply to pulses.
             */
            continue;
        }

        printf("Message from PID=%d TID=%d SCOID=%d\n", info.pid, info.tid, info.scoid);
        printf("Message length=%d\n", info.msglen);

        if (info.msglen < sizeof(my_msg_t))
        {
            MsgError(rcvid, EINVAL);
            continue;
        }

        /*
         * Process the normal message here.
         */

        MsgReply(rcvid, 0, NULL, 0);  /* Reply even if no reply data */
    }

    return 0;
}
```

---

## 22. Full Flow

```text
Client:
    MsgSend()

Kernel:
    copies message data
    fills _msg_info

Server:
    MsgReceive(chid, &msg, sizeof(msg), &info)

Server uses info for:
    logging
    validation
    authentication
    client tracking
    reply buffer checking

Server:
    MsgReply() or MsgError()
```

---

## 23. Common Mistakes

### Mistake 1: Always passing NULL

This is not always wrong, but you lose useful client information.

```c
MsgReceive(chid, &msg, sizeof(msg), NULL);
```

Use `&info` if you need logging, validation, or authentication.

---

### Mistake 2: Trusting `_msg_info` for pulses

Wrong:

```c
if (rcvid == 0)
{
    printf("Pulse PID=%d\n", info.pid);
}
```

Correct:

```c
if (rcvid == 0)
{
    /*
     * Handle pulse using struct _pulse fields.
     * Do not trust _msg_info here.
     */
}
```

---

### Mistake 3: Not checking message length

Wrong:

```c
/* Immediately use msg as full structure */
```

Correct:

```c
if (info.msglen < sizeof(expected_msg_t))
{
    MsgError(rcvid, EINVAL);
    continue;
}
```

---

### Mistake 4: Sending reply larger than client buffer

Wrong idea:

```text
Server sends any reply size without checking client capacity.
```

Better:

```text
Use dstmsglen / reply buffer size info to avoid oversized replies.
```

---

## 24. Quick Quiz Questions

### Q1: What is `_msg_info`?

```text
A structure filled by MsgReceive() that contains information about the client and the received message.
```

### Q2: Which parameter of `MsgReceive()` receives `_msg_info`?

```text
The fourth parameter.
```

### Q3: What do we pass if we do not need client info?

```c
NULL
```

### Q4: What should we pass if we need client info?

```c
&info
```

Example:

```c
struct _msg_info info;
MsgReceive(chid, &msg, sizeof(msg), &info);
```

### Q5: Is `_msg_info` valid for pulses?

```text
No.
It is updated for normal messages, not for pulses.
```

### Q6: How do we know if `MsgReceive()` returned a normal message?

```text
rcvid > 0
```

### Q7: How do we know if `MsgReceive()` returned a pulse?

```text
rcvid == 0
```

### Q8: What does `pid` identify?

```text
The client process ID.
```

### Q9: What does `tid` identify?

```text
The client thread ID that called MsgSend().
```

### Q10: What does `scoid` identify?

```text
The server-side connection ID for the client connection.
```

### Q11: What can `scoid` be used for?

```text
Client tracking, cleanup, and authentication.
```

### Q12: What call can give more information about the client?

```text
ConnectClientInfo()
```

### Q13: Why is message length information useful?

```text
To verify whether the client sent enough data and whether more data must be read.
```

### Q14: Why is reply buffer length useful?

```text
To know how much data the server can safely reply back to the client.
```

### Q15: How can `_msg_info` help debugging?

```text
It provides PID, TID, priority, scoid, and message length for logging.
```

---

## Final Summary

```text
_msg_info:
    filled by MsgReceive()
    describes the client and the received message
```

Use it for:

```text
logging
validation
authentication
reply size checking
client tracking
cleanup
debugging
```

Important rule:

```text
For normal messages:
    _msg_info is valid

For pulses:
    _msg_info is not updated / not reliable
```

Final key sentence:

```text
Pass a struct _msg_info pointer to MsgReceive() when the server needs to identify, validate, authenticate, log, or track the client that sent the message.
```