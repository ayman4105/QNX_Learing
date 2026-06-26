# QNX IPC — Event Delivery Quick README

## 1. Core Idea

An **event** in QNX is a notification description.

It tells the kernel or another process:

```text
When something happens, notify me using this method.
```

The event itself is stored in:

```c
struct sigevent
```

Important idea:

```text
struct sigevent does not mean the event already happened.
It describes how notification should be delivered when the event happens.
```

---

## 2. What Can an Event Be?

An event can deliver notification as:

```text
pulse
signal
interrupt notification
timer notification
```

| Event Type | Delivered As            | Waiting / Unblock Call         |
| ---------- | ----------------------- | ------------------------------ |
| Pulse      | pulse message           | `MsgReceive()`                 |
| Signal     | signal                  | signal handler / signal wait   |
| Interrupt  | interrupt event         | `InterruptWait()`              |
| Timer      | pulse or signal usually | depends on timer configuration |

---

## 3. `struct sigevent`

`struct sigevent` stores the event delivery information.

It can describe:

```text
notification type
connection ID
priority
pulse code
pulse value
signal number
interrupt notification style
```

Usually, the **client** initializes the event because the client knows how it wants to be notified.

---

## 4. Event Initialization Macros

QNX provides macros to initialize `struct sigevent` safely.

### `SIGEV_PULSE_INIT()`

Used when the notification should be a pulse.

```c
SIGEV_PULSE_INIT(&event, coid, priority, code, value);
```

Meaning:

```text
When this event is delivered,
send a pulse to this connection ID
with this priority, code, and value.
```

### `SIGEV_SIGNAL_INIT()`

Used when the notification should be a signal.

```c
SIGEV_SIGNAL_INIT(&event, SIGUSR1);
```

### `SIGEV_INTR_INIT()`

Used for interrupt-style events.

```c
SIGEV_INTR_INIT(&event);
```

This is used with interrupt handling, where the waiting call is usually:

```c
InterruptWait();
```

---

## 5. Pulse Event Requirements

A pulse event needs:

```text
channel to receive the pulse
connection ID to that channel
struct sigevent
SIGEV_PULSE_INIT()
```

Typical setup:

```c
chid = ChannelCreate(0);
self_coid = ConnectAttach(0, 0, chid, _NTO_SIDE_CHANNEL, 0);
SIGEV_PULSE_INIT(&event, self_coid, SIGEV_PULSE_PRIO_INHERIT, MY_CODE, MY_VALUE);
```

Receiving the pulse:

```c
rcvid = MsgReceive(chid, &msg, sizeof(msg), NULL);

if (rcvid == 0)
{
    /* pulse received */
    code = msg.pulse.code;
    value = msg.pulse.value.sival_int;
}
```

Important:

```text
SIGEV_PULSE_INIT() does not send a pulse.
It only prepares the event description.
```

---

## 6. Thread-to-Thread / Process-to-Process Event Flow

Typical flow:

```text
Client creates event
Client sends event to server
Server verifies event
Server stores event
Server replies immediately
Later, server delivers event
Client receives notification
```

Diagram:

```text
Client                                      Server
------                                      ------

ChannelCreate()
ConnectAttach()
SIGEV_PULSE_INIT()
MsgRegisterEvent()

MsgSend(register event) -----------------> MsgReceive()
                                            MsgVerifyEvent()
                                            save event
MsgReply() <------------------------------ reply OK

Client continues running

Later...

                                            MsgDeliverEvent(saved event)
Pulse arrives <--------------------------- notification
MsgReceive() returns rcvid == 0
```

---

## 7. Event Registration vs Delayed Reply

Do not confuse **event notification** with **delayed reply**.

| Concept            | Client State                          | Server Stores     | Server Later Uses       |
| ------------------ | ------------------------------------- | ----------------- | ----------------------- |
| Delayed Reply      | client stays `REPLY blocked`          | `rcvid`           | `MsgReply(saved_rcvid)` |
| Event Notification | client is released after registration | `struct sigevent` | `MsgDeliverEvent()`     |

Event notification means:

```text
client registers notification
server replies now
client continues running
server notifies later
```

---

## 8. Updateable Events

Usually, the server does not modify the client's event.

But sometimes the server may need to update a value at delivery time.

Example:

```text
pulse.value = latest temperature
pulse.value = queue ID
pulse.value = available bytes
```

The client must allow this using:

```c
SIGEV_FLAG_UPDATEABLE
```

or the convenience macro:

```c
SIGEV_MAKE_UPDATEABLE(&event);
```

Rule:

```text
Without updateable flag:
    server should deliver original event values

With updateable flag:
    server may update allowed values at delivery time
```

---

## 9. `MsgRegisterEvent()`

For interprocess event delivery, the client should register the event.

```c
MsgRegisterEvent(server_coid, &event);
```

Purpose:

```text
Only the server connected through this server_coid may deliver this event.
```

This is a security feature.

### Allow Any Server

There is an option:

```c
_NTO_REGEVENT_ALLOW_ANY_SERVER
```

Meaning:

```text
Any server may deliver this event.
```

But this is less secure.

### Process Manager Case

If the event is expected from the process manager, the special coid can be:

```c
SYSMGR_COID
```

### Unregister

When the event is no longer needed:

```c
MsgUnregisterEvent(...);
```

---

## 10. `MsgVerifyEvent()`

The server should verify the event before storing it.

```c
MsgVerifyEvent(rcvid, &event);
```

Purpose:

```text
Check if the event is well-formed.
Check if the event is registered correctly.
Check if this server is allowed to deliver it.
```

If verification fails:

```text
Do not store the event.
Return error to the client.
```

Important:

```text
The server delivers the event only if MsgVerifyEvent() succeeds.
```

Wrong idea:

```text
The server delivers only if MsgVerifyEvent() fails.
```

That is false.

---

## 11. `MsgDeliverEvent()`

Later, when the condition happens, the server delivers the saved event.

```c
MsgDeliverEvent(saved_rcvid, &saved_event);
```

If the event was initialized as a pulse, the client receives a pulse.

If the event was initialized as a signal, the client receives a signal.

The delivery type depends on the `struct sigevent` content.

---

## 12. Cleanup

If the client disconnects, the server must discard the saved event.

Why?

```text
client is gone
saved event is stale
server must not deliver to dead client
resources must be freed
```

Typical cleanup with disconnect pulse:

```text
receive _PULSE_CODE_DISCONNECT
get scoid
remove all events for this client
free resources
ConnectDetach(scoid)
```

---

## 13. Big Picture Code Sketch

### Client Side

```c
struct sigevent event;

chid = ChannelCreate(0);
self_coid = ConnectAttach(0, 0, chid, _NTO_SIDE_CHANNEL, 0);

SIGEV_PULSE_INIT(&event, self_coid, SIGEV_PULSE_PRIO_INHERIT, DATA_READY_CODE, 123);

/* Optional */
SIGEV_MAKE_UPDATEABLE(&event);

MsgRegisterEvent(server_coid, &event);

msg.type = REGISTER_EVENT;
msg.event = event;

MsgSend(server_coid, &msg, sizeof(msg), NULL, 0);

/* Later */
rcvid = MsgReceive(chid, &recv_msg, sizeof(recv_msg), NULL);

if (rcvid == 0)
{
    /* pulse notification arrived */
}
```

### Server Side

```c
rcvid = MsgReceive(chid, &msg, sizeof(msg), &info);

if (msg.type == REGISTER_EVENT)
{
    if (MsgVerifyEvent(rcvid, &msg.event) == -1)
    {
        MsgError(rcvid, errno);
        continue;
    }

    save_event(info.scoid, rcvid, &msg.event);

    MsgReply(rcvid, 0, NULL, 0);
}

/* Later, when condition happens */
MsgDeliverEvent(saved_rcvid, &saved_event);
```

---

## 14. Common Mistakes

### Mistake 1: Thinking `SIGEV_PULSE_INIT()` sends the pulse

It does not. It only initializes the event structure.

### Mistake 2: Not creating a channel for pulse events

Pulse events need a channel to arrive on.

### Mistake 3: Not registering interprocess events

For process-to-process delivery, use `MsgRegisterEvent()`.

### Mistake 4: Storing event without verification

The server should call `MsgVerifyEvent()` before storing the event.

### Mistake 5: Delivering event when `MsgVerifyEvent()` fails

Wrong. If verification fails, the event should not be delivered.

### Mistake 6: Forgetting cleanup

If the client disconnects, discard the event and free resources.

---

## 15. Quiz Answers

### Question

In a typical thread-to-thread event delivery scenario, which are true?

Correct answers:

```text
1. The event must be registered by using MsgRegisterEvent.
2. The server usually won't need to modify the client's event.
3. The server should discard the event when the client disconnects.
```

Wrong answer:

```text
The server will only deliver the event if MsgVerifyEvent() fails.
```

Correct explanation:

```text
If MsgVerifyEvent() fails, the event cannot be delivered.
The server should only store and later deliver the event if verification succeeds.
```

---

## Final Summary

```text
Event:
    notification description

struct sigevent:
    stores how notification should be delivered

Client:
    creates channel
    initializes event
    registers event
    sends event to server

Server:
    verifies event
    stores event
    replies immediately
    later delivers event
    cleans up when client disconnects
```

Golden sentence:

```text
In QNX, struct sigevent lets a client describe how it wants to be notified later; the server verifies and stores that event, replies immediately, and later delivers the saved event with MsgDeliverEvent when the condition happens.
```