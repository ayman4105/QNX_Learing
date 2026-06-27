# QNX Timers — Quick README

## Core Idea

QNX timer handling is **tickless**.

This means QNX does **not** normally fire a fixed timer interrupt every 1 ms just for the sake of ticking.

Instead, QNX programs the hardware timer to interrupt only when a timer event is actually needed.

```text
No active timer needed
    -> no regular timer interrupt

Timer needed
    -> hardware timer is programmed for the next expiry time
```

---

## 1. Tickless Timer Model

In a traditional tick-based system:

```text
tick
tick
tick
tick
```

A timer interrupt may happen at a fixed interval, such as every 1 ms.

In QNX:

```text
Timer interrupt happens only when needed
```

This reduces unnecessary interrupts and improves efficiency.

---

## 2. What Is `ticksize`?

Even though QNX timers are tickless, most normal timers still align to a system resolution called:

```text
ticksize
```

The default ticksize is usually:

```text
1 ms
```

Think of `ticksize` as the normal timer resolution.

---

## 3. High-Resolution Timers

Most timers are rounded based on the ticksize.

However, high-resolution timers are an exception.

```text
Normal timers:
    Rounded to ticksize

High-resolution timers:
    Can use finer resolution
```

---

## 4. Round-Robin Timeslice

The round-robin scheduling timeslice is based on the ticksize.

```text
Round-robin timeslice = 4 * ticksize
```

If:

```text
ticksize = 1 ms
```

Then:

```text
Round-robin timeslice = 4 ms
```

The multiplier cannot be changed, but the ticksize can be changed.

---

## 5. Changing the Ticksize

The ticksize can be changed in the startup code using the `procnto` command-line option:

```text
-c
```

This is done during system startup.

---

## 6. Getting the Current Ticksize

Use:

```c
ClockPeriod()
```

With:

```c
CLOCK_REALTIME
```

This can be used to get the current system ticksize in nanoseconds.

---

## 7. Timer Rounding

Most timers round up to the ticksize.

Example:

```text
ticksize = 1 ms
requested sleep = 3.5 ms
```

The thread cannot wake up after 3 ms, because that would be less than requested.

So it wakes after the next valid tick:

```text
actual wake target = 4 ms
```

Important rule:

```text
A sleep should not finish earlier than requested.
It may finish later.
```

---

## 8. Wakeup Time vs Run Time

There is a difference between:

```text
Thread becomes READY
```

And:

```text
Thread actually RUNS
```

A timer may expire and make the thread ready, but the thread may not run immediately if a higher-priority thread is already running.

So there are two possible delays:

```text
1. Timer rounding delay
2. Scheduling delay
```

---

## 9. Timers Are Managed Per Core

QNX manages active timers on a per-core basis.

```text
Core 0 -> timer list
Core 1 -> timer list
Core 2 -> timer list
```

When a thread creates a timer, the timer is added to the timer list of the core where the thread is running.

The hardware timer for that core is programmed for the next required timer expiry.

---

## 10. Core Affinity and Timers

If your thread must run on a specific CPU core, set its core affinity before creating timers.

Recommended order:

```text
1. Set core affinity / runmask
2. Create and configure the timer
```

This helps avoid unnecessary scheduling overhead.

---

## 11. Timer Types

A timer can be:

```text
One-shot
Periodic
```

---

### One-Shot Timer

A one-shot timer fires once and then stops.

```text
Start timer
    |
    v
Timer fires once
    |
    v
Stops
```

Use it for:

```text
Single timeout
Delayed one-time action
```

---

### Periodic Timer

A periodic timer fires repeatedly.

```text
Timer fires
wait interval
Timer fires again
wait interval
Timer fires again
```

Use it for:

```text
Periodic checks
Maintenance tasks
Watchdog-style logic
Status updates
```

---

## 12. Clock Source for Timers

When creating a timer, choose which clock drives it.

Common choices:

```text
CLOCK_REALTIME
CLOCK_MONOTONIC
```

---

### `CLOCK_REALTIME`

Represents real calendar time.

It can change if the system time is adjusted.

Use it for:

```text
Calendar-based alarms
Real date/time events
```

---

### `CLOCK_MONOTONIC`

Represents time since boot.

It does not go backward.

Use it for:

```text
Timeouts
Periodic maintenance
Relative intervals
Performance timing
```

For most timeout and periodic driver logic, `CLOCK_MONOTONIC` is usually the safer choice.

---

## 13. Relative vs Absolute Timers

### Relative Timer

A relative timer fires after a duration from now.

Example:

```text
Fire after 1.5 seconds
```

In `timer_settime()`, using flags `0` means a relative timer.

---

### Absolute Timer

An absolute timer fires at a specific clock value.

Example:

```text
Fire when CLOCK_REALTIME reaches a specific timestamp
```

This usually uses:

```text
TIMER_ABSTIME
```

---

## 14. POSIX Timer APIs

QNX supports POSIX timer APIs.

Important calls:

```c
timer_create()
timer_settime()
timer_delete()
```

---

### `timer_create()`

Creates a timer.

```c
timer_create(clock_id, event, timer_id);
```

It defines:

```text
Clock source
Event to deliver
Timer ID
```

---

### `timer_settime()`

Starts, updates, or cancels a timer.

```c
timer_settime(timerid, flags, &spec, NULL);
```

It defines:

```text
Initial fire time
Repeat interval
Relative or absolute mode
```

---

### `timer_delete()`

Destroys a timer completely.

```c
timer_delete(timerid);
```

Use this when the timer is no longer needed.

---

## 15. `struct itimerspec`

Timer settings are stored in:

```c
struct itimerspec
```

Basic structure:

```c
struct itimerspec {
    struct timespec it_interval;
    struct timespec it_value;
};
```

Meaning:

```text
it_value:
    Time until the first timer fire

it_interval:
    Repeat interval for periodic timers
```

---

### One-Shot Timer Setup

```text
it_value > 0
it_interval = 0
```

This fires once.

---

### Periodic Timer Setup

```text
it_value > 0
it_interval > 0
```

This fires repeatedly.

---

## 16. Timer Event Delivery

When a timer expires, it can deliver an event.

Common event types include:

```text
Pulse
Signal
Semaphore event
```

A common QNX server design is to use a pulse.

```text
Timer expires
    |
    v
QNX sends pulse
    |
    v
Server wakes from MsgReceive()
```

This lets one server thread handle both:

```text
Client messages
Timer events
```

---

## 17. Common Server Timer Pattern

A server may need to perform periodic maintenance while also handling client messages.

Example requirement:

```text
First timer pulse after 1.5 seconds
Then repeat every 1.2 seconds
```

Flow:

```text
Create channel
Create self connection
Initialize pulse event
Create timer using CLOCK_MONOTONIC
Set initial fire time and interval
Receive timer pulses in MsgReceive()
```

---

## 18. Creating a Pulse Event for a Timer

Typical setup:

```c
coid = ConnectAttach(0, 0, chid, _NTO_SIDE_CHANNEL, 0);

SIGEV_PULSE_INIT(&event, coid, priority, TIMER_PULSE_CODE, value);

timer_create(CLOCK_MONOTONIC, &event, &timerid);
```

Why self connection?

```text
A pulse is delivered through a connection ID.
The process creates a connection to its own channel so the timer can deliver a pulse back to it.
```

---

## 19. Example Timer Values

Initial fire after:

```text
1.5 seconds
```

Repeat every:

```text
1.2 seconds
```

Example:

```c
spec.it_value.tv_sec = 1;
spec.it_value.tv_nsec = 500000000;

spec.it_interval.tv_sec = 1;
spec.it_interval.tv_nsec = 200000000;

timer_settime(timerid, 0, &spec, NULL);
```

Here:

```text
flags = 0
```

Means the timer is relative.

---

## 20. Receiving Timer Pulses

A server can receive both pulses and messages using `MsgReceive()`.

```c
rcvid = MsgReceive(chid, &msg, sizeof(msg), NULL);

if (rcvid == 0) {
    /* pulse received */
} else {
    /* normal client message received */
}
```

Check the pulse code to identify the timer event:

```c
if (msg.pulse.code == TIMER_PULSE_CODE) {
    /* do periodic maintenance */
}
```

---

## 21. Why Use a `union` Receive Buffer?

`MsgReceive()` may receive:

```text
Normal message
Pulse
```

So a union is commonly used:

```c
typedef union {
    struct _pulse pulse;
    my_message_t msg;
} recv_buf_t;
```

This ensures the receive buffer is large enough for either type.

---

## 22. Canceling a Timer Without Deleting It

You do not need to destroy a timer just to stop it.

To cancel a timer, set its values to zero:

```c
struct itimerspec spec = {0};

timer_settime(timerid, 0, &spec, NULL);
```

This stops the timer but keeps the timer object.

---

## 23. Restarting a Timer

To restart a canceled timer:

```text
Fill it_value and it_interval again
Call timer_settime() again
```

---

## 24. Cancel vs Delete

| Action | Meaning                                  |
| ------ | ---------------------------------------- |
| Cancel | Stop the timer but keep the timer object |
| Delete | Destroy the timer object completely      |

Use cancel when you may need the timer again.

Use delete when the timer is no longer needed.

---

## Final Summary

```text
QNX timers are tickless:
    The hardware timer interrupt is programmed only when needed.

ticksize:
    Normal timer resolution
    Default is usually 1 ms
    Most timers round up to this value

timer_create():
    Creates a timer

timer_settime():
    Starts, updates, or cancels a timer

timer_delete():
    Destroys a timer

CLOCK_MONOTONIC:
    Best for timeouts and periodic maintenance

CLOCK_REALTIME:
    Best for calendar-based time

Pulse timer event:
    A common QNX pattern for waking a server MsgReceive loop
```

Golden sentence:

```text
QNX timers are tickless, but most timer expirations are rounded to the system ticksize; a common server design is to use a POSIX timer that delivers a pulse to the MsgReceive loop for periodic work.
```