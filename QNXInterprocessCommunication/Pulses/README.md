# QNX IPC — Pulses Quick Study README

## Core Idea

A **pulse** in QNX is a small, non-blocking notification message.

Golden sentence:

```text
A pulse is a small non-blocking notification received through MsgReceive(); it has a code and value, returns rcvid == 0, and must never be replied to.
```

---

## 1. What Is a Pulse?

A pulse is a lightweight IPC notification.

```text
Pulse:
    small
    non-blocking for sender
    unidirectional
    no reply
    send-and-forget
```

Meaning:

```text
Sender sends pulse
Sender continues immediately
Receiver handles pulse later
```

Diagram:

```text
Sender
  |
  | MsgSendPulse()
  v
Receiver Channel

Sender continues immediately
```

---

## 2. Pulse vs Normal Message

| Item         | Normal Message          | Pulse                 |
| ------------ | ----------------------- | --------------------- |
| Send call    | `MsgSend()`             | `MsgSendPulse()`      |
| Blocking     | Client blocks           | Sender does not block |
| Reply        | Required                | No reply              |
| Direction    | Bidirectional           | Unidirectional        |
| Receive call | `MsgReceive()`          | `MsgReceive()`        |
| Use case     | Request/response        | Notification/event    |
| Data         | Message + reply buffers | Code + small value    |

Normal message:

```text
Client sends request
Client blocks
Server receives
Server replies
Client continues
```

Pulse:

```text
Sender sends notification
Sender does not wait
Receiver receives later
No reply
```

---

## 3. Why Pulses Are Useful

Pulses are good when you only need to notify another process or thread that something happened.

Examples:

```text
timer expired
data ready
shutdown requested
hardware event happened
wake up worker thread
client disconnected
state changed
```

Pulse means:

```text
Wake up, something changed.
```

---

## 4. When NOT To Use Pulses

Do not use pulses for:

```text
large data
large buffers
request/response operations
operations that need a returned result
complex structures
data transfer like 4KB payloads
```

Bad idea:

```text
Send 4KB data using many pulses.
```

Better design:

```text
Large data:
    use normal message or shared memory

Notification:
    use pulse
```

Common design:

```text
Shared memory:
    carries large data

Pulse:
    says "new data is ready"
```

---

## 5. Pulse Payload

A pulse carries a small fixed amount of data:

```text
1. code field
2. value field
```

### Code

The code is an 8-bit field.

It is usually used as the pulse type.

Example:

```text
PULSE_TIMER
PULSE_DATA_READY
PULSE_SHUTDOWN
```

### Value

The value is a 64-bit field.

It can be treated as:

```text
64-bit integer
32-bit integer
pointer
```

So the payload is approximately:

```text
64-bit value + 7-bit usable code
```

That is one bit less than 9 bytes of usable application information.

---

## 6. Pulse Code Range

The pulse code is an 8-bit signed value:

```text
-128 to +127
```

But QNX reserves negative pulse codes for system/kernel use.

```text
negative pulse codes:
    reserved for QNX/kernel/system pulses

positive pulse codes:
    available for user/application pulses
```

Use:

```text
_PULSE_CODE_MINAVAIL
to
_PULSE_CODE_MAXAVAIL
```

Example:

```c
#define PULSE_DATA_READY  (_PULSE_CODE_MINAVAIL + 0)
#define PULSE_SHUTDOWN    (_PULSE_CODE_MINAVAIL + 1)
#define PULSE_TIMER       (_PULSE_CODE_MINAVAIL + 2)
```

---

## 7. System Pulses

QNX can send system pulses to a process.

Examples:

```text
client died
disconnect happened
unblock requested
timer event
kernel notification
```

These usually use negative pulse codes.

So avoid defining your own pulse codes as negative values.

Bad:

```c
#define MY_PULSE -5
```

Good:

```c
#define MY_PULSE (_PULSE_CODE_MINAVAIL + 0)
```

---

## 8. Sending a Pulse

Use:

```c
MsgSendPulse(coid, priority, code, value);
```

Parameters:

| Parameter  | Meaning                           |
| ---------- | --------------------------------- |
| `coid`     | Connection ID to receiver channel |
| `priority` | Delivery/server handling priority |
| `code`     | Pulse type                        |
| `value`    | Small 64-bit data value           |

Example:

```c
MsgSendPulse(coid, -1, PULSE_DATA_READY, sensor_id);
```

Meaning:

```text
Send pulse to the channel referenced by coid.
Use caller thread priority.
Pulse type is PULSE_DATA_READY.
Pass sensor_id as value.
```

---

## 9. Pulse Priority

For normal messages, the client is blocked, so QNX can know the client priority while the server handles the message.

For pulses, the sender does not block.

So QNX stores the priority inside the pulse.

If priority is:

```text
-1
```

then QNX uses the priority of the calling thread.

Example:

```c
MsgSendPulse(coid, -1, PULSE_TIMER, 0);
```

You can also pass a valid priority explicitly.

Typical valid user priority range:

```text
1 to 253
```

Example:

```c
MsgSendPulse(coid, 20, PULSE_TIMER, 0);
```

---

## 10. Pulse Security / Privileges

Sending pulses to another process can require the correct privilege.

Reason:

```text
Pulses are non-blocking.
A bad process could flood another process with pulses.
```

This can cause denial-of-service behavior.

General idea:

```text
same user ID:
    may be allowed

different user ID:
    may require root privilege or proper QNX ability
```

---

## 11. Receiving Pulses

Pulses are received with the same function used for normal messages:

```c
rcvid = MsgReceive(chid, &msg, sizeof(msg), NULL);
```

The server receives both:

```text
normal messages
pulses
```

from the same receive point.

---

## 12. How To Distinguish Message vs Pulse

Look at the return value from `MsgReceive()`.

```text
rcvid > 0:
    normal message

rcvid == 0:
    pulse

rcvid == -1:
    error
```

Example:

```c
rcvid = MsgReceive(chid, &msg, sizeof(msg), NULL);

if (rcvid == 0)
{
    /* Pulse received */
}
else if (rcvid > 0)
{
    /* Normal message received */
}
else
{
    /* Error */
}
```

Important:

```text
Do not distinguish pulse vs message using msg.type.
Use the return value of MsgReceive().
```

---

## 13. No Reply For Pulses

A pulse has no receive ID.

For pulse:

```text
rcvid == 0
```

So you cannot reply to it.

Wrong:

```c
if (rcvid == 0)
{
    MsgReply(rcvid, 0, NULL, 0);   /* Wrong */
}
```

Correct:

```c
if (rcvid == 0)
{
    handle_pulse();
    continue;
}
```

Rule:

```text
Normal message:
    rcvid > 0
    must MsgReply() or MsgError()

Pulse:
    rcvid == 0
    no reply
```

---

## 14. Receive Buffer Size

A pulse is stored in:

```c
struct _pulse
```

The receive buffer must be large enough to hold `struct _pulse`.

If the buffer is too small:

```text
MsgReceive() returns -1
errno = EFAULT
pulse is lost
```

Why?

```text
Kernel dequeues the pulse.
Kernel tries to copy it into your buffer.
Buffer is too small.
Receive fails.
Pulse is already gone.
```

---

## 15. Best Receive Buffer Design: union

Use a union that contains:

```c
struct _pulse
```

and all normal message types.

Example:

```c
typedef struct
{
    uint16_t type;
    int value;
} my_msg_t;

typedef union
{
    struct _pulse pulse;
    uint16_t type;
    my_msg_t my_msg;
} recv_msg_t;
```

Why union?

```text
C guarantees a union is at least as large as its largest member.
If struct _pulse is inside the union, the buffer can hold a pulse.
```

Usage:

```c
recv_msg_t msg;

rcvid = MsgReceive(chid, &msg, sizeof(msg), NULL);
```

---

## 16. Pulse Data Fields

When `rcvid == 0`, use:

```c
msg.pulse
```

Important fields:

```text
code
value
scoid
```

### code

Pulse type.

```c
switch (msg.pulse.code)
{
    case PULSE_TIMER:
        break;

    case PULSE_DATA_READY:
        break;
}
```

### value

Small data value.

It can be viewed as:

```text
sival_int
sival_long
sival_ptr
```

Example:

```c
int sensor_id = msg.pulse.value.sival_int;
```

### scoid

Server connection ID.

It identifies the client/source connection.

If you need more information about the client, you may use APIs such as:

```c
ConnectClientInfo()
```

---

## 17. MsgSendPulsePtr()

QNX also provides:

```c
MsgSendPulsePtr()
```

This treats the value field as a pointer.

Example:

```c
MsgSendPulsePtr(coid, -1, PULSE_WORK_READY, ptr);
```

Be careful:

```text
Pointers are usually not meaningful across different processes.
Each process has its own virtual address space.
```

Better for cross-process designs:

```text
send an ID
or
use shared memory and send a pulse as notification
```

---

## 18. Pulse Queue Reliability

Pulses are queued until:

```text
they are received
or
the channel/connection relationship is destroyed
or
the system cannot continue holding them
```

In normal operation, delivery is reliable once queued.

But there is a concern:

```text
If receiver does not drain pulses,
pulse queue can grow
and consume memory.
```

---

## 19. Pulse Pool

For safety/resource control, QNX supports pre-allocating pulse queue entries.

Instead of:

```c
ChannelCreate()
```

you can use:

```c
ChannelCreatePulsePool()
```

with a configuration structure:

```c
nto_channel_config
```

Purpose:

```text
reserve pulse queue entries
guarantee minimum queue capacity
control memory behavior
```

This is useful in safety or real-time designs.

---

## 20. Pulse Queue Optimization

QNX can combine identical pulses into one queue entry with a count.

Pulses must match:

```text
same code
same value
same priority
same origin
```

If they are identical, QNX may merge them.

If they are different, each may consume a separate queue entry.

---

## 21. Server Receive Loop With Pulses

Example pattern:

```c
while (1)
{
    rcvid = MsgReceive(chid, &msg, sizeof(msg), NULL);

    if (rcvid == -1)
    {
        perror("MsgReceive");
        continue;
    }

    if (rcvid == 0)
    {
        switch (msg.pulse.code)
        {
            case PULSE_TIMER:
                /* Handle timer */
                break;

            case PULSE_DATA_READY:
                /* Handle data-ready event */
                break;

            case PULSE_SHUTDOWN:
                /* Handle shutdown */
                break;

            default:
                /* Unknown pulse */
                break;
        }

        /* No MsgReply() for pulses */
        continue;
    }

    switch (msg.type)
    {
        case MSG_GET_DATA:
            /* Process normal message */
            MsgReply(rcvid, 0, &reply, sizeof(reply));
            break;

        default:
            MsgError(rcvid, ENOSYS);
            break;
    }
}
```

---

## 22. Full Receive Buffer Example

```c
#include <stdint.h>
#include <sys/neutrino.h>
#include <sys/iomsg.h>
#include <errno.h>
#include <stdio.h>

#define MSG_GET_DATA       (_IO_MAX + 1)
#define PULSE_DATA_READY   (_PULSE_CODE_MINAVAIL + 0)
#define PULSE_SHUTDOWN     (_PULSE_CODE_MINAVAIL + 1)

typedef struct
{
    uint16_t type;              /* Normal message type */
    int id;                     /* Example message data */
} get_data_msg_t;

typedef struct
{
    int result;                 /* Reply data */
} get_data_reply_t;

typedef union
{
    struct _pulse pulse;        /* Makes buffer large enough for pulses */
    uint16_t type;              /* Allows checking normal message type */
    get_data_msg_t get_data;    /* Normal message payload */
} recv_msg_t;

int main(void)
{
    int chid;                   /* Channel ID */
    int rcvid;                  /* Receive ID */
    recv_msg_t msg;             /* Receive buffer */
    get_data_reply_t reply;     /* Reply buffer */

    chid = ChannelCreate(0);    /* Create channel */

    if (chid == -1)
    {
        perror("ChannelCreate");
        return 1;
    }

    while (1)
    {
        rcvid = MsgReceive(chid, &msg, sizeof(msg), NULL);  /* Receive message or pulse */

        if (rcvid == -1)
        {
            perror("MsgReceive");
            continue;
        }

        if (rcvid == 0)
        {
            switch (msg.pulse.code)
            {
                case PULSE_DATA_READY:
                    printf("Data ready pulse, value=%d\n", msg.pulse.value.sival_int);
                    break;

                case PULSE_SHUTDOWN:
                    printf("Shutdown pulse received\n");
                    return 0;

                default:
                    printf("Unknown pulse code=%d\n", msg.pulse.code);
                    break;
            }

            continue;           /* Do not reply to pulses */
        }

        switch (msg.type)
        {
            case MSG_GET_DATA:
                reply.result = msg.get_data.id + 100;
                MsgReply(rcvid, 0, &reply, sizeof(reply));
                break;

            default:
                MsgError(rcvid, ENOSYS);
                break;
        }
    }

    return 0;
}
```

---

## 23. Common Mistakes

### Mistake 1: Replying to a pulse

Wrong:

```c
if (rcvid == 0)
{
    MsgReply(rcvid, 0, NULL, 0);
}
```

Correct:

```c
if (rcvid == 0)
{
    handle_pulse();
    continue;
}
```

---

### Mistake 2: Using a small receive buffer

Wrong:

```c
char buffer[4];

MsgReceive(chid, buffer, sizeof(buffer), NULL);
```

Correct:

```c
typedef union
{
    struct _pulse pulse;
    my_msg_t msg;
} recv_msg_t;
```

---

### Mistake 3: Using negative pulse codes

Wrong:

```c
#define MY_PULSE -3
```

Correct:

```c
#define MY_PULSE (_PULSE_CODE_MINAVAIL + 0)
```

---

### Mistake 4: Using pulses for large data

Wrong:

```text
Send large payload using many pulses.
```

Correct:

```text
Use normal message or shared memory.
Use pulse only as notification.
```

---

### Mistake 5: Distinguishing by msg.type

Wrong:

```c
if (msg.type == SOME_PULSE)
{
    /* pulse */
}
```

Correct:

```c
if (rcvid == 0)
{
    /* pulse */
}
else if (rcvid > 0)
{
    /* message */
}
```

---

## 24. Check Your Knowledge Questions

### Question 1

**What are the characteristics of a QNX pulse?**

Select all that apply.

#### Options

1. It is a way to send information that is non-blocking for the sender.
2. It has a small, fixed size payload around 9 bytes.
3. It has a small payload that is limited to 32K bytes.
4. The pulse recipient can return an error code that is passed back to the sender in the return value from the `MsgSendPulse()` call.
5. It is a fast and relatively inexpensive way to send notification between processes.

#### Correct Answers

```text
1. It is a way to send information that is non-blocking for the sender.
2. It has a small, fixed size payload around 9 bytes.
5. It is a fast and relatively inexpensive way to send notification between processes.
```

#### Explanation

Pulses are non-blocking for the sender and are used as fast, inexpensive notifications.

The payload is small:

```text
64-bit value + 7-bit usable code
```

Why the others are wrong:

```text
32K bytes:
    wrong, pulses are much smaller than that.

Recipient can return error:
    wrong, pulses have no reply.
    Any error from MsgSendPulse() comes from the kernel, not from the receiver.
```

---

### Question 2

**Which of the following statements about receiving pulses is true?**

Select the best response.

#### Options

1. The server calls `MsgCatchPulse()` and waits for a client to send a pulse with `MsgSendPulse()`.
2. Pulses are only received by the kernel; there is no mechanism for a user program to receive pulses.
3. The server calls `MsgReceive()` to receive either pulses or messages, which are distinguished by testing the return value of `MsgReceive()`.
4. The server calls `MsgReceive()` to receive either pulses or messages, which are distinguished by the value of `msg.type`.

#### Correct Answer

```text
3. The server calls MsgReceive() to receive either pulses or messages, which are distinguished by testing the return value of MsgReceive().
```

#### Explanation

`MsgReceive()` receives both normal messages and pulses.

Use the return value:

```text
rcvid > 0:
    normal message

rcvid == 0:
    pulse

rcvid == -1:
    error
```

Why the others are wrong:

```text
MsgCatchPulse():
    wrong, this is not the receiving API here.

Only kernel receives pulses:
    wrong, user programs receive pulses using MsgReceive().

Distinguished by msg.type:
    wrong, message vs pulse is distinguished by MsgReceive() return value.
```

---

## 25. Common Quiz Questions

### Q1: What is a pulse?

```text
A small non-blocking notification message in QNX.
```

### Q2: Does the sender block when sending a pulse?

```text
No.
MsgSendPulse() is non-blocking for the sender.
```

### Q3: Does a pulse have a reply?

```text
No.
A pulse is send-and-forget.
```

### Q4: Which function sends a pulse?

```text
MsgSendPulse()
```

### Q5: Which function receives pulses?

```text
MsgReceive()
```

### Q6: How do you know MsgReceive() received a pulse?

```text
The return value is 0.
```

### Q7: How do you know MsgReceive() received a normal message?

```text
The return value is greater than 0.
```

### Q8: What are the main pulse data fields?

```text
code
value
scoid
```

### Q9: What is the pulse code used for?

```text
It is usually used as the pulse type.
```

### Q10: What is the pulse value used for?

```text
It carries a small 64-bit data value.
```

### Q11: Why should the receive buffer contain struct _pulse?

```text
To guarantee the buffer is large enough to receive pulse data.
```

### Q12: What happens if the receive buffer is too small?

```text
MsgReceive() returns -1, errno becomes EFAULT, and the pulse is lost.
```

### Q13: Why are negative pulse codes avoided?

```text
They are reserved for QNX system/kernel pulses.
```

### Q14: When should pulses be used?

```text
For quick notifications such as timer events, data ready, shutdown, or wake-up events.
```

### Q15: When should pulses not be used?

```text
For large data or operations that require a reply/result.
```

---

## Final Summary

```text
Pulse:
    small QNX IPC notification
    non-blocking for sender
    unidirectional
    no reply
    fast and cheap
```

Main call:

```c
MsgSendPulse(coid, priority, code, value);
```

Receive call:

```c
MsgReceive(chid, &msg, sizeof(msg), NULL);
```

Important return values:

```text
rcvid > 0:
    normal message

rcvid == 0:
    pulse

rcvid == -1:
    error
```

Important rule:

```text
Never reply to a pulse.
```

Final key sentence:

```text
Use pulses for fast event notifications, not for large data transfer or request/response communication.
```