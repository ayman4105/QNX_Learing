# QNX Kernel Timeouts — Quick README

## Core Idea

Kernel timeouts are used to prevent a thread from staying blocked forever inside a QNX kernel call.

A common example is a client thread calling:

```c
MsgSend()
```

`MsgSend()` may block while waiting for:

```text
The server to receive the message
The server to send a reply
```

`TimerTimeout()` lets you limit how long the thread is allowed to stay blocked.

---

## 1. Why Kernel Timeouts Are Needed

In QNX client/server IPC, a client can block in different states.

For example:

```text
Client calls MsgSend()
        |
        v
SEND blocked
        |
        v
Server receives message
        |
        v
REPLY blocked
        |
        v
Server replies
        |
        v
MsgSend() returns
```

If the server is slow, stuck, deadlocked, or not running, the client could stay blocked for too long.

Kernel timeouts solve this problem.

---

## 2. `SEND blocked`

A thread becomes `SEND blocked` when:

```text
The client has sent a message,
but the server has not received it yet.
```

Example:

```text
Client ---> MsgSend() ---> Server

Server is not waiting in MsgReceive()

Client state:
    SEND blocked
```

If the server never receives the message, the client may block forever unless a timeout is used.

---

## 3. `REPLY blocked`

A thread becomes `REPLY blocked` when:

```text
The server has received the message,
but has not replied yet.
```

Example:

```text
Client ---> MsgSend() ---> Server
Server receives message
Server processes request

Client state:
    REPLY blocked
```

If the server forgets to call `MsgReply()` or takes too long, the client stays blocked.

---

## 4. `TimerTimeout()`

`TimerTimeout()` sets a timeout for the next blocking kernel call.

Conceptually:

```text
TimerTimeout(timeout value, blocking states)
Next kernel call
If the thread blocks too long -> timeout
```

Example use case:

```text
Set timeout = 2.5 seconds
Apply it to SEND blocked and REPLY blocked states
Call MsgSend()
```

If the operation does not complete within the timeout, the call returns an error.

---

## 5. Timeout Result

If the timeout expires:

```text
return value = -1
errno = ETIMEDOUT
```

Example:

```c
status = MsgSend(coid, &msg, sizeof(msg), &reply, sizeof(reply));

if (status == -1 && errno == ETIMEDOUT) {
    /* The operation timed out */
}
```

This lets the client recover instead of blocking forever.

---

## 6. `TimerTimeout()` Is Not a Normal Timer

Do not confuse `TimerTimeout()` with POSIX timers such as:

```c
timer_create()
timer_settime()
```

### POSIX Timer

A POSIX timer delivers an event when it expires.

```text
timer expires
    -> pulse / signal / event
```

### Kernel Timeout

A kernel timeout limits how long a thread may stay blocked in a kernel call.

```text
TimerTimeout()
Next blocking kernel call
If blocked too long -> ETIMEDOUT
```

---

## 7. Important Rule: It Applies to the Next Kernel Call

`TimerTimeout()` applies only to the next kernel call.

Correct usage:

```c
TimerTimeout(...);
MsgSend(...);
```

Incorrect usage:

```c
TimerTimeout(...);
clock_gettime(...);
MsgSend(...);
```

In the incorrect example, another kernel call happens between `TimerTimeout()` and `MsgSend()`.

That can cancel or consume the timeout before `MsgSend()`.

---

## 8. Timeout Is Relative

The timeout is relative to when `TimerTimeout()` is called.

Example:

```text
TimerTimeout(2.5 seconds)
```

The countdown starts at the time of the `TimerTimeout()` call.

Because of this, call `TimerTimeout()` immediately before the kernel call you want to protect.

---

## 9. Applying Timeout to Specific Blocking States

For `MsgSend()`, the useful states are commonly:

```text
SEND blocked
REPLY blocked
```

You can request the timeout to apply to one or both of these states.

Example idea:

```text
Timeout if the client waits too long for the server to receive
Timeout if the client waits too long for the server to reply
```

---

## 10. Immediate Timeout

A timeout can also be used in an immediate style.

This means:

```text
Do not block.
If the call would block, return immediately.
```

Conceptually:

```text
Try the operation
If it can complete now -> complete
If it would block -> return immediately
```

This is useful when you want non-blocking behavior.

---

## 11. Normal Timeout vs Immediate Timeout

| Type              | Meaning                     |
| ----------------- | --------------------------- |
| Normal timeout    | Wait up to a specified time |
| Immediate timeout | Do not wait at all          |

Example:

```text
Normal timeout:
    Wait up to 2.5 seconds

Immediate timeout:
    Return immediately if the call would block
```

---

## 12. Using Timeouts in a Loop

Because `TimerTimeout()` applies only to the next kernel call, call it before every operation that needs protection.

Correct:

```c
while (1) {
    TimerTimeout(...);
    MsgSend(...);
}
```

Incorrect:

```c
TimerTimeout(...);

while (1) {
    MsgSend(...);
}
```

The incorrect version only applies the timeout to the first kernel call.

---

## 13. Practical Client Recovery

When a timeout happens, the client can choose how to recover.

Possible recovery actions:

```text
Retry the request
Log an error
Notify a supervisor
Switch to a safe state
Restart the service
Use a fallback server
```

This is especially important in real-time and safety-related systems.

---

## 14. Typical Flow

```text
Client thread
    |
    v
TimerTimeout(2.5 seconds, SEND + REPLY)
    |
    v
MsgSend()
    |
    +--> SEND blocked too long?
    |       -> return -1, errno = ETIMEDOUT
    |
    +--> REPLY blocked too long?
            -> return -1, errno = ETIMEDOUT
```

---

## 15. Common Mistakes

### Mistake 1: Placing another kernel call between `TimerTimeout()` and the target call

Bad:

```c
TimerTimeout(...);
some_kernel_call();
MsgSend(...);
```

Good:

```c
TimerTimeout(...);
MsgSend(...);
```

---

### Mistake 2: Assuming the timeout stays active forever

`TimerTimeout()` is not persistent.

It applies to the next kernel call only.

---

### Mistake 3: Forgetting to handle `ETIMEDOUT`

Always check return values.

```c
if (status == -1 && errno == ETIMEDOUT) {
    /* timeout handling */
}
```

---

## Final Summary

```text
Kernel timeouts prevent threads from blocking forever.

TimerTimeout():
    Sets a timeout for the next kernel call

MsgSend() can block in:
    SEND blocked
    REPLY blocked

If the timeout expires:
    return = -1
    errno = ETIMEDOUT

Important:
    TimerTimeout is relative
    It applies only to the next kernel call
    Call it immediately before the call you want to protect

Immediate timeout:
    Used when you do not want the call to block at all
```

Golden sentence:

```text
TimerTimeout() is used before a blocking kernel call to limit how long the thread may stay blocked, and it must be called immediately before the kernel call you want to protect.
```