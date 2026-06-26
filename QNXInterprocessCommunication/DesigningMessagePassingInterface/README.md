# QNX IPC — Designing Message Passing Interface Quick README

## Core Idea

Before writing the client and server code, you should first design the **message interface**.

This means defining:

```text
message types
message structures
reply structures
shared constants
```

These definitions should be placed in a shared header file used by both the client and the server.

Golden sentence:

```text
The client and server must include the same header file so both sides understand the same message format.
```

---

## 1. Why We Need a Shared Header File

In QNX message passing, the client sends a binary message and the server receives it.

So both sides must agree on:

```text
what the message type means
what fields exist in the message
how large the message is
what reply format is expected
```

Good design:

```text
ipc_messages.h
    included by client.c
    included by server.c
```

Diagram:

```text
        ipc_messages.h
        /            \
       v              v
  client.c         server.c
```

This prevents the client and server from using different message formats.

---

## 2. What Goes in the Header File?

The shared header file should contain:

```text
message type numbers
message structs
reply structs
common enums
common constants
possibly unions for multiple message formats
```

Example file:

```text
ipc_messages.h
```

---

## 3. Every Message Starts With uint16_t type

All messages should start with:

```c
uint16_t type;
```

This field tells the server what kind of request this is.

Example:

```c
typedef struct
{
    uint16_t type;
    int speed;
} set_speed_msg_t;
```

Why first field?

```text
The server can receive any message and read the first field to know the message type.
```

---

## 4. Message Type Example

Example message types:

```c
enum
{
    MSG_GET_SPEED = _IO_MAX + 1,
    MSG_SET_SPEED,
    MSG_GET_TEMP
};
```

Meaning:

```text
MSG_GET_SPEED:
    client wants to read speed

MSG_SET_SPEED:
    client wants to set speed

MSG_GET_TEMP:
    client wants to read temperature
```

---

## 5. Why Use Values Larger Than _IO_MAX?

QNX system messages use the range:

```text
0 to _IO_MAX
```

`_IO_MAX` is up to:

```text
511
```

So user-defined messages should start after that range.

Good:

```c
MSG_GET_SPEED = _IO_MAX + 1
```

or:

```text
512 and above
```

Avoid:

```text
0 to 511
```

because this range may overlap with QNX system messages.

---

## 6. Why Avoid QNX System Message Range?

There are two main reasons.

### Reason 1: Debugging Tools

QNX tools and event tracing may interpret low message numbers as system messages.

If your custom message uses a system range value, debugging output may look wrong.

```text
wrong message number
        |
        v
debug tools may think it is a system message
```

### Reason 2: Accidental System Messages

If a system message accidentally reaches your server, you can safely identify that it is not one of your custom messages.

Then you can ignore it or reply with an error.

---

## 7. Basic Message Structure

Example request:

```c
typedef struct
{
    uint16_t type;
} get_speed_msg_t;
```

This request only needs a type because it does not send extra data.

---

## 8. Message With Data

Example request:

```c
typedef struct
{
    uint16_t type;
    int speed;
} set_speed_msg_t;
```

Meaning:

```text
type:
    what command this is

speed:
    data needed by the command
```

---

## 9. Reply Structures

If the server needs to return data, define a reply structure too.

Example:

```c
typedef struct
{
    int speed;
} get_speed_reply_t;
```

Client sends:

```text
MSG_GET_SPEED
```

Server replies with:

```text
get_speed_reply_t
```

---

## 10. When There Is No Reply Data

Sometimes the server only needs to acknowledge success.

Example:

```text
Client sends SET_SPEED.
Server sets the speed.
No data is needed in the reply.
```

Server can reply with:

```c
MsgReply(rcvid, 0, NULL, 0);
```

Meaning:

```text
success status
no reply data
```

---

## 11. Related Messages: Type and Subtype

If you have many related messages, you can use:

```text
type
subtype
```

Example:

```c
typedef struct
{
    uint16_t type;
    uint16_t subtype;
    int value;
} vehicle_msg_t;
```

Example values:

```text
type = MSG_VEHICLE
subtype = VEHICLE_GET_SPEED

type = MSG_VEHICLE
subtype = VEHICLE_SET_LIGHT
```

This is useful when messages belong to the same category.

---

## 12. Unrelated Messages: Use a Union

If messages have different structures, use a `union`.

Example:

```c
typedef struct
{
    uint16_t type;
} get_speed_msg_t;

typedef struct
{
    uint16_t type;
    int speed;
} set_speed_msg_t;

typedef struct
{
    uint16_t type;
    int light_id;
    int state;
} set_light_msg_t;

typedef union
{
    uint16_t type;
    get_speed_msg_t get_speed;
    set_speed_msg_t set_speed;
    set_light_msg_t set_light;
} ipc_msg_t;
```

Why this works:

```text
All structs start with uint16_t type.
The server can read msg.type first.
Then it knows which union member to use.
```

---

## 13. Full Header File Example

```c
#ifndef IPC_MESSAGES_H
#define IPC_MESSAGES_H

#include <stdint.h>
#include <sys/iomsg.h>

enum
{
    MSG_GET_SPEED = _IO_MAX + 1,
    MSG_SET_SPEED,
    MSG_SET_LIGHT
};

typedef struct
{
    uint16_t type;             /* Message type */
} get_speed_msg_t;

typedef struct
{
    uint16_t type;             /* Message type */
    int speed;                 /* Speed value to set */
} set_speed_msg_t;

typedef struct
{
    uint16_t type;             /* Message type */
    int light_id;              /* Light ID */
    int state;                 /* Light state */
} set_light_msg_t;

typedef struct
{
    int speed;                 /* Speed value returned by server */
} get_speed_reply_t;

typedef union
{
    uint16_t type;             /* Common first field */
    get_speed_msg_t get_speed; /* Get speed request */
    set_speed_msg_t set_speed; /* Set speed request */
    set_light_msg_t set_light; /* Set light request */
} ipc_msg_t;

#endif
```

---

## 14. Server Side Logic

The server receives a message and switches on the message type.

```c
ipc_msg_t msg;
get_speed_reply_t speed_reply;

rcvid = MsgReceive(chid, &msg, sizeof(msg), NULL);

switch (msg.type)
{
    case MSG_GET_SPEED:
        speed_reply.speed = 120;
        MsgReply(rcvid, 0, &speed_reply, sizeof(speed_reply));
        break;

    case MSG_SET_SPEED:
        /* Use msg.set_speed.speed */
        MsgReply(rcvid, 0, NULL, 0);
        break;

    case MSG_SET_LIGHT:
        /* Use msg.set_light.light_id and msg.set_light.state */
        MsgReply(rcvid, 0, NULL, 0);
        break;

    default:
        MsgError(rcvid, ENOSYS);
        break;
}
```

Important:

```text
Unknown message type should not be ignored.
Use MsgError().
```

---

## 15. Client Side Logic

The client fills the correct structure and sends it.

Example: get speed.

```c
ipc_msg_t msg;
get_speed_reply_t reply;
int status;

msg.get_speed.type = MSG_GET_SPEED;

status = MsgSend(coid, &msg.get_speed, sizeof(msg.get_speed), &reply, sizeof(reply));

if (status == -1)
{
    perror("MsgSend");
}
else
{
    printf("Speed = %d\n", reply.speed);
}
```

Example: set speed.

```c
ipc_msg_t msg;
int status;

msg.set_speed.type = MSG_SET_SPEED;
msg.set_speed.speed = 100;

status = MsgSend(coid, &msg.set_speed, sizeof(msg.set_speed), NULL, 0);

if (status == -1)
{
    perror("MsgSend");
}
```

---

## 16. Server Flow

```text
Server:
    MsgReceive()
        |
        v
    read msg.type
        |
        v
    switch(msg.type)
        |
        +--> MSG_GET_SPEED
        |       prepare reply
        |       MsgReply()
        |
        +--> MSG_SET_SPEED
        |       process data
        |       MsgReply()
        |
        +--> default
                MsgError()
```

---

## 17. Why the Server Uses switch()

The server may receive different message types on the same channel.

So it branches based on:

```c
msg.type
```

Example:

```text
MSG_GET_SPEED:
    return speed

MSG_SET_SPEED:
    update speed

MSG_SET_LIGHT:
    control light
```

This keeps the server organized.

---

## 18. Design Rules

### Rule 1: Create a shared header file

```text
client and server must include the same definitions
```

### Rule 2: Start every message with uint16_t type

```text
the server can identify the message type quickly
```

### Rule 3: Use message numbers above _IO_MAX

```text
avoid QNX system message range
```

### Rule 4: Define reply structs when needed

```text
if server returns data, define reply format clearly
```

### Rule 5: Use union for unrelated message formats

```text
one receive buffer can hold many message types
```

### Rule 6: Unknown message type must get MsgError()

```text
do not leave the client REPLY blocked
```

---

## 19. Common Mistakes

### Mistake 1: Client and server use different structs

Bad:

```text
client has one struct format
server expects another struct format
```

Result:

```text
wrong data interpretation
bugs
wrong message sizes
```

Fix:

```text
put all structs in one shared header file
```

---

### Mistake 2: Message does not start with type

Bad:

```c
typedef struct
{
    int speed;
    uint16_t type;
} bad_msg_t;
```

Good:

```c
typedef struct
{
    uint16_t type;
    int speed;
} good_msg_t;
```

---

### Mistake 3: Using message numbers below _IO_MAX

Bad:

```c
#define MSG_GET_SPEED 10
```

Good:

```c
#define MSG_GET_SPEED (_IO_MAX + 1)
```

---

### Mistake 4: Ignoring unknown messages

Bad:

```c
default:
    break;
```

Good:

```c
default:
    MsgError(rcvid, ENOSYS);
    break;
```

---

## 20. Quick Quiz Questions

### Q1: Where should message types and structures be defined?

```text
In a shared header file included by both client and server.
```

### Q2: What should every message start with?

```text
uint16_t type
```

### Q3: Why does every message start with a type?

```text
So the server can identify which message was received.
```

### Q4: What range should user-defined message types use?

```text
Values larger than _IO_MAX.
Usually 512 and above.
```

### Q5: Why avoid the range 0 to _IO_MAX?

```text
Because it overlaps with QNX system messages.
```

### Q6: When can you use type and subtype?

```text
When messages are related or share a common structure.
```

### Q7: When can you use a union?

```text
When messages are unrelated and have different structures.
```

### Q8: What should the server do after MsgReceive()?

```text
Switch on the message type and process the matching request.
```

### Q9: What should the server do for an unknown message type?

```text
Call MsgError(), for example MsgError(rcvid, ENOSYS).
```

### Q10: Why define reply structures?

```text
So the server and client agree on the reply data format.
```

---

## Final Summary

```text
Designing a QNX message passing interface means defining the protocol between client and server.
```

The protocol should include:

```text
message type numbers
request structures
reply structures
common constants
optional unions
```

The shared header file is the contract:

```text
client sends according to the header
server receives according to the header
server replies according to the header
```

Final key sentence:

```text
Put all message types and structures in a shared header file, start every message with uint16_t type, and use message values above _IO_MAX to avoid conflicts with QNX system messages.
```