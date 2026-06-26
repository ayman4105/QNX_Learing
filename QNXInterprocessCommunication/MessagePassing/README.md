# QNX IPC — Message Passing Quick Study README

## Core Idea

QNX native IPC is based on **message passing**.

It uses a **client-server** model:

```text
Client:
    sends request using MsgSend()

Server:
    receives request using MsgReceive()
    processes request
    replies using MsgReply() or MsgError()
```

Golden sentence:

```text
QNX message passing is a synchronous RPC-like IPC system where the client sends through a coid, the server receives on a chid, and the server replies using the rcvid returned by MsgReceive().
```

---

## 1. What Is IPC?

IPC means:

```text
Interprocess Communication
```

It is a way for different processes or threads to communicate.

In QNX, the native IPC mechanism is:

```text
Message Passing
```

---

## 2. Client / Server Design Pattern

QNX message passing is based on a **client-server design pattern**.

```text
Client:
    asks for service

Server:
    provides service
```

Example:

```text
Dashboard Client:
    asks for speed

Vehicle Data Server:
    receives request
    reads speed
    replies with speed value
```

Diagram:

```text
Client
  |
  | MsgSend()
  v
Server
  |
  | MsgReceive()
  v
Process request
  |
  | MsgReply()
  v
Client continues
```

---

## 3. Bidirectional Communication

The communication is bidirectional.

```text
Client -> Server:
    request message

Server -> Client:
    reply message
```

So it is not just sending data one way.

---

## 4. Synchronous Communication

`MsgSend()` is synchronous.

That means:

```text
Client sends a request
Client blocks
Server processes the request
Server replies
Client becomes runnable again
```

This is not asynchronous.

Asynchronous means:

```text
send message
continue running immediately
receive response later
```

But QNX `MsgSend()` does not work like that by default.

---

## 5. RPC-like Model

QNX message passing can be thought of as **RPC-like**.

RPC means:

```text
Remote Procedure Call
```

The client feels like it is calling a function on the server.

```text
Client calls service
Client waits for result
Server returns result
```

---

## 6. The Three Main Calls

| Call           | Used By | Purpose                         |
| -------------- | ------- | ------------------------------- |
| `MsgSend()`    | Client  | Send request and wait for reply |
| `MsgReceive()` | Server  | Receive request from client     |
| `MsgReply()`   | Server  | Reply to client and unblock it  |

Basic flow:

```text
Client                          Server
------                          ------
MsgSend()  ------------------>  MsgReceive()
blocked                         process message
returns   <------------------   MsgReply()
continues
```

---

## 7. Important Blocking States

QNX message passing has important blocking states.

| State             | Usually Who? | Meaning                                                  |
| ----------------- | ------------ | -------------------------------------------------------- |
| `RECEIVE blocked` | Server       | Waiting for a message                                    |
| `SEND blocked`    | Client       | Waiting for server to receive the message                |
| `REPLY blocked`   | Client       | Server received the message; client is waiting for reply |

---

## 8. Case 1: Server Is Ready

If the server is already waiting in `MsgReceive()`:

```text
Server state:
    RECEIVE blocked
```

Then when the client calls `MsgSend()`:

```text
1. Kernel copies client message to server buffer
2. Server wakes from MsgReceive()
3. Client becomes REPLY blocked
4. Server processes message
5. Server calls MsgReply()
6. Client becomes runnable again
```

Diagram:

```text
Client                          Server
------                          ------
runnable                        RECEIVE blocked
   |
   | MsgSend()
   v
REPLY blocked  ------------->   MsgReceive() returns
                                process request
                                MsgReply()
   ^
   | reply copied back
   |
runnable again
```

---

## 9. Case 2: Server Is Busy

If the server is not waiting in `MsgReceive()`:

```text
Server:
    busy
```

Then when the client calls `MsgSend()`:

```text
1. Client becomes SEND blocked
2. No message copy happens yet
3. Later server calls MsgReceive()
4. Kernel copies client message to server buffer
5. Client changes from SEND blocked to REPLY blocked
6. Server processes message
7. Server calls MsgReply()
8. Client becomes runnable again
```

Diagram:

```text
Client                          Server
------                          ------
runnable                        busy
   |
   | MsgSend()
   v
SEND blocked                    still busy
   |
   |                            MsgReceive()
   |------------------------->  message copied
   v
REPLY blocked                   process request
                                MsgReply()
   ^
   | reply copied back
   |
runnable again
```

---

## 10. SEND Blocked vs REPLY Blocked

```text
SEND blocked:
    client is waiting for the server to receive the message

REPLY blocked:
    server already received the message
    client is waiting for the server to reply
```

This is one of the most important QNX IPC debugging ideas.

---

## 11. Channel and Connection

For message passing to work:

```text
Server needs a channel.
Client needs a connection to that channel.
```

Server side:

```c
chid = ChannelCreate(0);
```

Client side:

```c
coid = ConnectAttach(0, server_pid, server_chid, _NTO_SIDE_CHANNEL, 0);
```

Then the client sends using:

```c
MsgSend(coid, &request, sizeof(request), &reply, sizeof(reply));
```

---

## 12. Important IDs

| ID      | Exists At | Purpose                                                     |
| ------- | --------- | ----------------------------------------------------------- |
| `chid`  | Server    | Channel ID used by `MsgReceive()`                           |
| `coid`  | Client    | Connection ID used by `MsgSend()`                           |
| `rcvid` | Server    | Receive ID returned by `MsgReceive()`, used by `MsgReply()` |

Diagram:

```text
Server:
    chid = ChannelCreate()

Client:
    coid = ConnectAttach(server_pid, chid)

Client:
    MsgSend(coid, ...)

Server:
    rcvid = MsgReceive(chid, ...)

Server:
    MsgReply(rcvid, ...)
```

---

## 13. ChannelCreate()

`ChannelCreate()` creates a receive endpoint for the server.

```c
chid = ChannelCreate(0);
```

Meaning:

```text
Create a channel where clients can send messages.
```

Server loop:

```c
chid = ChannelCreate(0);

while (1)
{
    rcvid = MsgReceive(chid, &msg, sizeof(msg), &info);

    /* Process message */

    MsgReply(rcvid, 0, &reply, sizeof(reply));
}
```

---

## 14. Multiple Server Threads

A server process can have multiple threads waiting on the same channel.

```text
Server Process
    |
    +-- Channel
          |
          +-- Thread 1 waiting in MsgReceive()
          +-- Thread 2 waiting in MsgReceive()
          +-- Thread 3 waiting in MsgReceive()
```

When a message arrives, the kernel can choose an available server thread to receive it.

---

## 15. ChannelCreate Flags

Common flags:

| Flag                       | Meaning                                              |
| -------------------------- | ---------------------------------------------------- |
| `_NTO_CHF_PRIVATE`         | Private/internal channel                             |
| `_NTO_CHF_DISCONNECT`      | Notify server when a client goes away                |
| `_NTO_CHF_COID_DISCONNECT` | Notify process when one of its coids becomes invalid |
| `_NTO_CHF_UNBLOCK`         | Notify server when a blocked client wants to unblock |

---

## 16. _NTO_CHF_PRIVATE

Used for internal/private channels.

```c
chid = ChannelCreate(_NTO_CHF_PRIVATE);
```

Meaning:

```text
This channel is for internal use.
Other processes should not connect to it as a public service.
```

---

## 17. _NTO_CHF_DISCONNECT

Used when the server wants to know that a client disappeared.

```c
chid = ChannelCreate(_NTO_CHF_DISCONNECT);
```

Example:

```text
Client connects
Server stores client data
Client dies
Kernel sends disconnect pulse
Server cleans client data
```

---

## 18. _NTO_CHF_COID_DISCONNECT

Used when a process is also a client and one of its connection IDs becomes invalid.

```text
DISCONNECT:
    someone connected to me went away

COID_DISCONNECT:
    my connection to another server became invalid
```

---

## 19. _NTO_CHF_UNBLOCK

Used when a client is blocked waiting for a reply and wants to unblock.

Possible reasons:

```text
timeout
signal
cancel request
client no longer wants to wait
```

Flow:

```text
Client sends message
Server receives it
Client becomes REPLY blocked
Client gets timeout or signal
Kernel sends unblock pulse to server
```

---

## 20. Pulses

A pulse is a small notification message.

`MsgReceive()` may receive:

```text
normal client message
or
pulse notification
```

Rule:

```text
rcvid > 0:
    normal message

rcvid == 0:
    pulse
```

Server skeleton:

```c
rcvid = MsgReceive(chid, &msg, sizeof(msg), &info);

if (rcvid == 0)
{
    /* Handle pulse */
}
else
{
    /* Handle normal message */
    MsgReply(rcvid, 0, &reply, sizeof(reply));
}
```

---

## 21. ConnectAttach()

`ConnectAttach()` is used by the client to connect to a server channel.

```c
coid = ConnectAttach(0, server_pid, server_chid, _NTO_SIDE_CHANNEL, 0);
```

Parameters:

```text
0:
    local node

server_pid:
    PID of server process

server_chid:
    channel ID of server

_NTO_SIDE_CHANNEL:
    create a side-channel connection ID

0:
    extra flags
```

---

## 22. Why _NTO_SIDE_CHANNEL Is Important

QNX has two connection ID ranges:

```text
1. file descriptor range
2. side channel range
```

`_NTO_SIDE_CHANNEL` tells QNX:

```text
Create the coid in the side-channel range.
Avoid the file descriptor range.
```

This avoids unwanted interaction with:

```text
open()
close()
dup()
fork()
library functions
```

---

## 23. File Descriptor Range vs Side Channel Range

File descriptor range is used by POSIX-style APIs:

```text
open()
socket()
pipe()
stdin / stdout / stderr
```

Side channel range is used for QNX message passing connections created with:

```c
ConnectAttach(..., _NTO_SIDE_CHANNEL, ...);
```

Important:

```text
A standard POSIX open() call returns a connection ID in the file descriptor range.
```

---

## 24. MsgSend()

`MsgSend()` is called by the client.

```c
status = MsgSend(coid, &request, sizeof(request), &reply, sizeof(reply));
```

Parameters:

| Parameter         | Meaning                         |
| ----------------- | ------------------------------- |
| `coid`            | Connection ID to server channel |
| `&request`        | Send buffer                     |
| `sizeof(request)` | Number of bytes to send         |
| `&reply`          | Reply buffer                    |
| `sizeof(reply)`   | Size of reply buffer            |

Meaning:

```text
Send request to server.
Block until server replies or errors.
Store reply data in reply buffer.
Return server status.
```

---

## 25. MsgSend Return Value

`MsgSend()` returns the status provided by the server in `MsgReply()`.

Server:

```c
MsgReply(rcvid, 0, &reply, sizeof(reply));
```

Client:

```text
MsgSend returns 0
```

If server uses `MsgError()`:

```c
MsgError(rcvid, ENOSYS);
```

Client:

```text
MsgSend returns -1
errno = ENOSYS
```

---

## 26. MsgReceive()

`MsgReceive()` is called by the server.

```c
rcvid = MsgReceive(chid, &msg, sizeof(msg), &info);
```

Parameters:

| Parameter     | Meaning                          |
| ------------- | -------------------------------- |
| `chid`        | Server channel ID                |
| `&msg`        | Receive buffer                   |
| `sizeof(msg)` | Receive buffer size              |
| `&info`       | Extra client/message information |

Return value:

```text
rcvid:
    receive ID used to reply to this client message
```

---

## 27. MsgReply()

`MsgReply()` is called by the server.

```c
MsgReply(rcvid, status, &reply, sizeof(reply));
```

Parameters:

| Parameter       | Meaning                        |
| --------------- | ------------------------------ |
| `rcvid`         | Receive ID from `MsgReceive()` |
| `status`        | Value returned by `MsgSend()`  |
| `&reply`        | Reply data buffer              |
| `sizeof(reply)` | Number of bytes to reply       |

If there is no reply data:

```c
MsgReply(rcvid, 0, NULL, 0);
```

Important:

```text
Even if there is no reply data, the server must reply to unblock the client.
```

---

## 28. MsgError()

`MsgError()` is used when the server wants to return an error.

```c
MsgError(rcvid, ENOSYS);
```

Client result:

```text
MsgSend returns -1
errno is set to ENOSYS
```

Use this when:

```text
unknown command
unsupported operation
invalid request
server cannot handle the message
```

---

## 29. Important Rule: Always Reply or Error

Every normal message received by the server must eventually get:

```text
MsgReply()
or
MsgError()
```

Bad code:

```c
rcvid = MsgReceive(chid, &msg, sizeof(msg), NULL);

if (msg.type == UNKNOWN)
{
    continue;   /* Wrong: client remains REPLY blocked */
}
```

Correct code:

```c
rcvid = MsgReceive(chid, &msg, sizeof(msg), NULL);

if (msg.type == UNKNOWN)
{
    MsgError(rcvid, ENOSYS);
    continue;
}
```

---

## 30. Data Copying: No Pointers Between Processes

QNX does not pass client pointers directly to the server.

The kernel copies data:

```text
Client send buffer ----copy----> Server receive buffer
Server reply buffer ---copy----> Client reply buffer
```

Reason:

```text
Each process has its own virtual address space.
A pointer in the client may not be valid in the server.
```

---

## 31. Copy Size Rule

If client sends more bytes than the server buffer can hold:

```text
copied bytes = min(client_send_size, server_receive_size)
```

Example:

```text
client sends 100 bytes
server receive buffer is 40 bytes
kernel copies 40 bytes
```

The remaining data can be read later using APIs such as `MsgRead()`.

Same idea applies to reply data:

```text
copied bytes = min(server_reply_size, client_reply_buffer_size)
```

---

## 32. Delayed Reply

A server does not always need to reply immediately.

It can:

```text
receive message
save rcvid
start long operation
reply later when data is ready
```

But it must not forget to reply.

Example:

```text
Client requests slow sensor read
Server receives request
Server stores rcvid
Server starts hardware operation
Later, when data is ready:
    Server calls MsgReply(saved_rcvid, ...)
```

Client stays:

```text
REPLY blocked
```

until reply/error happens.

---

## 33. Debugging With pidin

Use:

```sh
pidin
```

To see:

```text
processes
threads
priorities
states
blocking info
```

Important states:

```text
RECEIVE blocked:
    server waiting for message

SEND blocked:
    client waiting for server to receive

REPLY blocked:
    client waiting for server reply
```

---

## 34. Debugging File Descriptors and Side Channels

Use:

```sh
pidin fd | less
```

This shows:

```text
file descriptors
side-channel connections
connection IDs
```

Use it to check whether your `ConnectAttach()` created side-channel coids.

---

## 35. Full Server Pseudo-code

```c
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/neutrino.h>
#include <unistd.h>

#define CMD_ADD 1

typedef struct
{
    int command;     /* Command ID sent by client */
    int x;           /* First input value */
    int y;           /* Second input value */
} request_t;

typedef struct
{
    int result;      /* Result returned to client */
} reply_t;

int main(void)
{
    int chid;                    /* Channel ID used by server */
    int rcvid;                   /* Receive ID returned by MsgReceive */
    request_t request;           /* Buffer for client request */
    reply_t reply;               /* Buffer for server reply */

    chid = ChannelCreate(0);     /* Create server channel */

    if (chid == -1)
    {
        perror("ChannelCreate");
        return 1;
    }

    printf("Server PID = %d\n", getpid());
    printf("Server CHID = %d\n", chid);

    while (1)
    {
        rcvid = MsgReceive(chid, &request, sizeof(request), NULL);  /* Wait for client message */

        if (rcvid == -1)
        {
            perror("MsgReceive");
            continue;
        }

        if (rcvid == 0)
        {
            /* This is a pulse, not a normal message */
            continue;
        }

        if (request.command == CMD_ADD)
        {
            reply.result = request.x + request.y;                   /* Process request */
            MsgReply(rcvid, 0, &reply, sizeof(reply));              /* Reply to client */
        }
        else
        {
            MsgError(rcvid, ENOSYS);                                /* Unsupported command */
        }
    }

    return 0;
}
```

---

## 36. Full Client Pseudo-code

```c
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/neutrino.h>

#define CMD_ADD 1

typedef struct
{
    int command;     /* Command ID sent to server */
    int x;           /* First input value */
    int y;           /* Second input value */
} request_t;

typedef struct
{
    int result;      /* Result received from server */
} reply_t;

int main(int argc, char *argv[])
{
    int server_pid;              /* Server process ID */
    int server_chid;             /* Server channel ID */
    int coid;                    /* Connection ID returned by ConnectAttach */
    int status;                  /* Status returned by MsgSend */
    request_t request;           /* Request sent to server */
    reply_t reply;               /* Reply received from server */

    if (argc != 3)
    {
        printf("Usage: client <server_pid> <server_chid>\n");
        return 1;
    }

    server_pid = atoi(argv[1]);  /* Convert server PID argument */
    server_chid = atoi(argv[2]); /* Convert server channel ID argument */

    coid = ConnectAttach(0, server_pid, server_chid, _NTO_SIDE_CHANNEL, 0); /* Connect to server channel */

    if (coid == -1)
    {
        perror("ConnectAttach");
        return 1;
    }

    request.command = CMD_ADD;   /* Select command */
    request.x = 10;              /* Set first input */
    request.y = 20;              /* Set second input */

    status = MsgSend(coid, &request, sizeof(request), &reply, sizeof(reply)); /* Send request and wait for reply */

    if (status == -1)
    {
        perror("MsgSend");
        return 1;
    }

    printf("Status = %d\n", status);
    printf("Result = %d\n", reply.result);

    return 0;
}
```

---

# Check Your Knowledge Questions

## Question 1

**Which characteristics are true of QNX message passing?**

Select all that apply.

### Options

1. It is based on a mailbox design pattern.
2. It is based on a client-server design pattern.
3. The client calls to the server are asynchronous; in other words, the client continues running after making the call to the server.
4. It includes bidirectional communication.
5. It uses fully POSIX compliant APIs.

### Correct Answers

```text
2. It is based on a client-server design pattern.
4. It includes bidirectional communication.
```

### Explanation

QNX message passing is based on a client-server design pattern and includes bidirectional communication.

```text
Client sends request.
Server sends reply.
```

The calls are synchronous, so the client blocks until the server returns a response.

Why the others are wrong:

```text
Mailbox design pattern:
    wrong, QNX message passing is client-server.

Asynchronous calls:
    wrong, MsgSend() is synchronous.

Fully POSIX compliant APIs:
    wrong, MsgSend(), MsgReceive(), MsgReply(), ChannelCreate(), and ConnectAttach() are QNX native APIs.
```

---

## Question 2

**Which of the following facts about connection IDs are true?**

Select all that apply.

### Options

1. There are two types, file descriptors and side channel connections.
2. There are three types, file descriptors, side channel connections, and servers.
3. Passing `_NTO_SIDE_CHANNEL` to `ConnectAttach()` ensures the coid will end up in the file descriptor range.
4. A standard POSIX `open()` call will return a connection id that is in the file descriptor range.

### Correct Answers

```text
1. There are two types, file descriptors and side channel connections.
4. A standard POSIX open() call will return a connection id that is in the file descriptor range.
```

### Explanation

There are two connection ID ranges:

```text
1. file descriptor range
2. side channel range
```

A standard POSIX `open()` call returns a coid in the file descriptor range.

Passing `_NTO_SIDE_CHANNEL` to `ConnectAttach()` does the opposite of option 3.

It means:

```text
Create the coid in the side-channel range.
Avoid the file descriptor range.
```

---

## Common Quiz Questions

### Q1: What design pattern is QNX message passing based on?

```text
Client-server design pattern.
```

### Q2: Is QNX MsgSend asynchronous?

```text
No.
MsgSend is synchronous.
The client blocks until the server replies or errors.
```

### Q3: What does RECEIVE blocked mean?

```text
The server is waiting for a message on a channel.
```

### Q4: What does SEND blocked mean?

```text
The client is waiting for the server to receive the message.
```

### Q5: What does REPLY blocked mean?

```text
The server already received the message, and the client is waiting for the server reply.
```

### Q6: What is a chid?

```text
Channel ID created by the server using ChannelCreate().
```

### Q7: What is a coid?

```text
Connection ID created by the client using ConnectAttach().
The client uses it with MsgSend().
```

### Q8: What is an rcvid?

```text
Receive ID returned to the server by MsgReceive().
The server uses it with MsgReply() or MsgError().
```

### Q9: Why use _NTO_SIDE_CHANNEL?

```text
To create the coid in the side-channel range and avoid the file descriptor range.
```

### Q10: Does QNX pass pointers between client and server?

```text
No.
The kernel copies data between buffers.
```

### Q11: What must the server do after receiving a normal message?

```text
It must eventually call MsgReply() or MsgError().
```

---

## Final Summary

```text
QNX Message Passing:
    native QNX IPC
    client-server
    synchronous
    bidirectional
    RPC-like
```

Main calls:

```text
Server:
    ChannelCreate()
    MsgReceive()
    MsgReply() / MsgError()

Client:
    ConnectAttach()
    MsgSend()
```

Main IDs:

```text
chid:
    server receive point

coid:
    client send handle

rcvid:
    server reply handle
```

Main states:

```text
RECEIVE blocked:
    server waits for message

SEND blocked:
    client waits for server to receive

REPLY blocked:
    client waits for server to reply
```

Final key sentence:

```text
The client sends using a coid, the server receives on a chid, and the server replies using the rcvid.
```