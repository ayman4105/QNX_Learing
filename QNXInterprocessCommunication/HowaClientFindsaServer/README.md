# QNX IPC — How a Client Finds a Server

## Core Idea

A QNX client needs a **connection ID** (`coid`) before it can call `MsgSend()`.

Training/simple demo way:

```text
server prints PID and CHID
client receives PID and CHID as command-line arguments
client calls ConnectAttach()
```

Real design:

```text
server registers a stable name
client opens that name
QNX creates the connection automatically
client gets coid
```

Golden sentence:

```text
Instead of giving the client the server PID and channel ID manually, the server registers a name and the client opens that name to get a connection ID.
```

---

## 1. Why the Client Needs to Find the Server

To send a message, the client calls:

```c
MsgSend(coid, &msg, sizeof(msg), &reply, sizeof(reply));
```

So the client needs:

```text
coid = connection ID
```

Normally, `coid` can be created by:

```c
ConnectAttach()
```

But `ConnectAttach()` needs:

```text
server PID
server channel ID
```

This is not practical because PID and CHID can change every time the server restarts.

---

## 2. Two Ways to Find a Server

| Server Type            | Server Registers With | Client Finds With |
| ---------------------- | --------------------- | ----------------- |
| Resource Manager       | `resmgr_attach()`     | `open()`          |
| Simple MsgReceive loop | `name_attach()`       | `name_open()`     |

---

## 3. Resource Manager Case

If the server is a resource manager, it registers a pathname.

Example:

```text
/dev/sound
/dev/my_sensor
```

Server:

```c
resmgr_attach(..., "/dev/sound", ...);
```

Client:

```c
fd = open("/dev/sound", O_RDWR);
```

In QNX:

```text
a file descriptor is a connection ID in the file descriptor range
```

So `read()` and `write()` internally format messages and send them to the resource manager.

---

## 4. Simple Server Case

For a normal `MsgReceive()` loop server:

Server uses:

```c
name_attach()
```

Client uses:

```c
name_open()
```

Server:

```c
name_attach_t *attach;
attach = name_attach(NULL, "my_server", 0);
```

Client:

```c
coid = name_open("my_server", 0);
```

Then the client sends normally:

```c
MsgSend(coid, &msg, sizeof(msg), &reply, sizeof(reply));
```

---

## 5. `name_attach()`

`name_attach()` does two important things:

```text
1. registers a name in the pathname space
2. creates a channel internally
```

So after `name_attach()`, the server does not need to call `ChannelCreate()` manually.

The channel ID is inside:

```c
attach->chid
```

The server receives using:

```c
rcvid = MsgReceive(attach->chid, &msg, sizeof(msg), &info);
```

---

## 6. `name_detach()`

When the server stops being a server:

```c
name_detach(attach, 0);
```

This:

```text
removes the name from pathname space
destroys the channel created by name_attach()
```

---

## 7. `name_open()`

The client uses:

```c
coid = name_open("my_server", 0);
```

This:

```text
finds the registered name
creates a connection
returns a coid
```

The client does not need to know:

```text
server PID
server CHID
```

---

## 8. `name_close()`

When the client no longer needs the server:

```c
name_close(coid);
```

This closes the connection created by `name_open()`.

---

## 9. Channel Flags Created by `name_attach()`

`name_attach()` internally creates a channel with useful flags.

Important flags:

```text
_NTO_CHF_DISCONNECT
_NTO_CHF_COID_DISCONNECT
_NTO_CHF_UNBLOCK
```

This means a server using `name_attach()` must handle pulses in its `MsgReceive()` loop.

---

## 10. `_NTO_CHF_DISCONNECT`

This asks the kernel to notify the server when a client goes away.

Pulse code:

```text
_PULSE_CODE_DISCONNECT
```

Use case:

```text
client dies
client closes connection
client disconnects
server must clean per-client data
```

---

## 11. `_NTO_CHF_COID_DISCONNECT`

This asks the kernel to notify when one of the process's connection IDs becomes invalid.

Pulse code:

```text
_PULSE_CODE_COIDDEATH
```

The pulse value tells which connection ID or file descriptor became invalid.

Use case:

```text
this process is a client to another server
that other server goes away
my coid becomes invalid
kernel sends COIDDEATH pulse
```

---

## 12. `_NTO_CHF_UNBLOCK`

This asks the kernel to notify the server when a REPLY-blocked client wants to unblock.

Pulse code:

```text
_PULSE_CODE_UNBLOCK
```

Possible reasons:

```text
signal
timeout
cancel request
client no longer wants to wait
```

---

## 13. Receive Loop With Pulses

Because `name_attach()` requests pulses, the server must check `rcvid`.

```c
rcvid = MsgReceive(attach->chid, &msg, sizeof(msg), &info);

if (rcvid == 0)
{
    /* pulse */
}
else if (rcvid > 0)
{
    /* normal message */
}
else
{
    /* error */
}
```

Rule:

```text
rcvid > 0:
    normal message, must MsgReply() or MsgError()

rcvid == 0:
    pulse, must not reply
```

---

## 14. Per-client Data Tracking

A server often needs a client list or client table.

QNX does not create this list for you.

You create it using:

```text
array
linked list
hash table
custom table
```

Why track clients?

```text
pending requests
hardware data gathering
notification subscriptions
client configuration
authentication state
buffers per client
```

---

## 15. Using `scoid` as Client ID

When the server receives a normal message with `_msg_info`:

```c
struct _msg_info info;
rcvid = MsgReceive(attach->chid, &msg, sizeof(msg), &info);
```

`info.scoid` identifies the client connection from the server side.

Use it as the key in your client list.

```text
client side:
    coid

server side:
    scoid
```

---

## 16. Adding a New Client

When a normal message arrives:

```text
read info.scoid
search client list
if not found:
    add new client entry
process message
reply
```

Pseudo-code:

```c
client = find_client(info.scoid);

if (client == NULL)
{
    client = add_client(info.scoid);
}

process_client_message(client, &msg);
MsgReply(rcvid, 0, NULL, 0);
```

---

## 17. Disconnect Pulse Cleanup

When a client goes away, the server receives:

```text
_PULSE_CODE_DISCONNECT
```

The pulse contains the `scoid` of the client that went away.

Server must:

```text
1. read pulse.scoid
2. find client entry by scoid
3. clean client data
4. free buffers/resources
5. cancel pending work if needed
6. call ConnectDetach(scoid)
```

Important rule:

```text
When you request disconnect pulses, you must call ConnectDetach(scoid) for the scoid in the disconnect pulse.
```

---

## 18. Why `ConnectDetach(scoid)` Is Required

When the kernel notifies you that a client connection disappeared, the server must detach the server-side connection ID.

```c
ConnectDetach(scoid);
```

This completes the cleanup of that connection.

Do not forget it.

---

## 19. Disconnect Pulse Example

```c
if (rcvid == 0)
{
    switch (msg.pulse.code)
    {
        case _PULSE_CODE_DISCONNECT:
        {
            int scoid = msg.pulse.scoid;

            client = find_client(scoid);

            if (client != NULL)
            {
                cleanup_client(client);
                remove_client(client);
            }

            ConnectDetach(scoid);
            break;
        }

        default:
            break;
    }

    continue;
}
```

---

## 20. Full Server Skeleton

```c
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <sys/neutrino.h>
#include <sys/dispatch.h>

typedef struct
{
    uint16_t type;
    int value;
} my_msg_t;

typedef union
{
    struct _pulse pulse;
    uint16_t type;
    my_msg_t msg;
} recv_msg_t;

int main(void)
{
    name_attach_t *attach;
    recv_msg_t msg;
    struct _msg_info info;
    int rcvid;

    attach = name_attach(NULL, "my_server", 0);

    if (attach == NULL)
    {
        perror("name_attach");
        return 1;
    }

    while (1)
    {
        rcvid = MsgReceive(attach->chid, &msg, sizeof(msg), &info);

        if (rcvid == -1)
        {
            perror("MsgReceive");
            continue;
        }

        if (rcvid == 0)
        {
            switch (msg.pulse.code)
            {
                case _PULSE_CODE_DISCONNECT:
                    cleanup_client_by_scoid(msg.pulse.scoid);
                    ConnectDetach(msg.pulse.scoid);
                    break;

                case _PULSE_CODE_COIDDEATH:
                    /* msg.pulse.value contains the dead coid/fd */
                    break;

                case _PULSE_CODE_UNBLOCK:
                    /* client wants to unblock */
                    break;

                default:
                    break;
            }

            continue;
        }

        track_client_if_new(info.scoid);
        MsgReply(rcvid, 0, NULL, 0);
    }

    name_detach(attach, 0);
    return 0;
}
```

---

## 21. Full Client Skeleton

```c
#include <stdio.h>
#include <stdint.h>
#include <sys/neutrino.h>
#include <sys/dispatch.h>

typedef struct
{
    uint16_t type;
    int value;
} my_msg_t;

int main(void)
{
    int coid;
    my_msg_t msg;
    int status;

    coid = name_open("my_server", 0);

    if (coid == -1)
    {
        perror("name_open");
        return 1;
    }

    msg.type = 1;
    msg.value = 123;

    status = MsgSend(coid, &msg, sizeof(msg), NULL, 0);

    if (status == -1)
    {
        perror("MsgSend");
    }

    name_close(coid);
    return 0;
}
```

---

## 22. Common Mistakes

### Mistake 1: Passing PID and CHID manually in real projects

Training only:

```text
./client <pid> <chid>
```

Better:

```text
server uses name_attach()
client uses name_open()
```

### Mistake 2: Calling ChannelCreate() after name_attach()

`name_attach()` already creates a channel.

Use:

```c
attach->chid
```

### Mistake 3: Not handling pulses

A server using `name_attach()` can receive system pulses.

Always check:

```text
rcvid == 0
```

### Mistake 4: Replying to pulses

Pulses have no reply.

### Mistake 5: Forgetting ConnectDetach() on disconnect pulse

Must do:

```c
ConnectDetach(msg.pulse.scoid);
```

### Mistake 6: Not tracking clients when needed

If your server has pending requests, subscriptions, buffers, or per-client state, create a client list keyed by `scoid`.

---

## 23. Quick Quiz Questions

### Q1: Why is passing PID and CHID manually not a good real-life solution?

```text
PID and CHID can change, and manual setup is not practical.
```

### Q2: How does a resource manager register a name?

```text
Using resmgr_attach().
```

### Q3: How does a client find a resource manager?

```text
Using open().
```

### Q4: How does a simple MsgReceive-loop server register a name?

```text
Using name_attach().
```

### Q5: How does a client find a simple named server?

```text
Using name_open().
```

### Q6: Does name_attach() create a channel?

```text
Yes. It creates the channel internally.
```

### Q7: Which channel ID should the server pass to MsgReceive() after name_attach()?

```text
attach->chid
```

### Q8: What does name_open() return?

```text
A connection ID, coid.
```

### Q9: How does a client close a connection from name_open()?

```text
name_close(coid)
```

### Q10: How does the server remove the registered name and destroy the channel?

```text
name_detach(attach, 0)
```

### Q11: What pulse code means a client went away?

```text
_PULSE_CODE_DISCONNECT
```

### Q12: What pulse code means one of my coids became invalid?

```text
_PULSE_CODE_COIDDEATH
```

### Q13: What pulse code means a REPLY-blocked client wants to unblock?

```text
_PULSE_CODE_UNBLOCK
```

### Q14: What ID should the server use to track clients?

```text
scoid
```

### Q15: Where does the server get scoid for a normal message?

```text
From struct _msg_info, usually info.scoid.
```

### Q16: Where does the server get scoid when a client disconnects?

```text
From the disconnect pulse, usually msg.pulse.scoid.
```

### Q17: What must the server call after handling a disconnect pulse?

```text
ConnectDetach(scoid)
```

---

## Final Summary

```text
Client finding server:

Resource manager:
    server -> resmgr_attach()
    client -> open()

Simple MsgReceive server:
    server -> name_attach()
    client -> name_open()
```

Important flow:

```text
server registers name
client opens name
client gets coid
client sends MsgSend()
server receives on attach->chid
```

Important cleanup:

```text
server:
    name_detach()

client:
    name_close()

disconnect pulse:
    cleanup client list
    ConnectDetach(scoid)
```

Final key sentence:

```text
For a simple QNX message-passing server, name_attach() registers a stable name and creates a channel, name_open() gives the client a coid, and disconnect pulses must be handled by cleaning client state and calling ConnectDetach(scoid).
```