# QNX High-Resolution Timers — Quick README

## Core Idea

A high-resolution timer is used when you need a timer notification to happen more precisely than the normal system tick resolution.

Normal timers are usually aligned to the system `ticksize`, which is commonly:

```text
1 ms
```

A high-resolution timer tries to fire:

```text
At or just after the requested expiry time
```

Instead of rounding the expiry time to the next normal tick.

---

## 1. Normal Timer Behavior

With normal timers, timer expiry is usually aligned to the system tick.

Example:

```text
ticksize = 1 ms
requested timer = 4.5 ms
```

A normal timer may be rounded to:

```text
5 ms
```

This is acceptable for many normal timers, but not for precise timing requirements.

---

## 2. High-Resolution Timer Behavior

A high-resolution timer is not aligned to the normal tick.

If you request:

```text
4.5 ms
```

QNX tries to make the timer expire close to:

```text
4.5 ms
```

The final precision depends on:

```text
Hardware timer resolution
Kernel overhead
Scheduling delay
System load
Thread priority
```

---

## 3. Important Limitation

A high-resolution timer improves the timer expiry time, but it does not guarantee that your thread runs immediately.

Flow:

```text
Timer expires
    |
    v
Event or pulse is delivered
    |
    v
Thread becomes READY
    |
    v
Scheduler chooses when it actually RUNS
```

So there can still be scheduling delay after the timer expires.

---

## 4. How to Configure a High-Resolution Timer

To configure a high-resolution timer, use the QNX-specific tolerance feature.

You use the flag:

```text
TIMER_TOLERANCE
```

And specify a tolerance value.

The tolerance must be:

```text
Less than the system ticksize
```

Example:

```text
ticksize = 1 ms
tolerance = 5 ns
```

This is valid because:

```text
5 ns < 1 ms
```

---

## 5. What Is Timer Tolerance?

Tolerance means how much timing flexibility you allow the kernel when scheduling the timer expiry.

Smaller tolerance means more precise timing.

But high precision has a cost:

```text
More overhead
More calculations
Less timer coalescing
More hardware timer programming
```

Because of this extra overhead, high-resolution timers are privileged operations.

---

## 6. Required Ability

High-resolution timers require a special QNX ability.

A normal process cannot freely create high-resolution timers unless it has the required high-resolution timer ability.

This is because high-resolution timers can increase system overhead.

---

## 7. Initial Fire Time

For a high-resolution timer, QNX tries to make the first timer expiry happen as close as possible to the requested time.

Example:

```text
Requested first fire time = 4.5 ms
```

QNX tries to fire the timer close to:

```text
4.5 ms
```

Not simply at the next 1 ms tick boundary.

---

## 8. Repeating Timers

For repeating timers, tolerance also affects the reload or repeat timing.

QNX considers:

```text
The repeat interval
The tolerance value
```

The script explains that for repeating timers, the interval behavior is affected by the maximum of the repeat value and the tolerance value.

For one-shot timers, the tolerance mainly marks the timer as high-resolution, and QNX tries to fire as close as possible to the requested time.

---

## 9. POSIX vs QNX-Specific API

You may configure timer timing through POSIX-style APIs, but high-resolution tolerance is QNX-specific.

Because it is non-portable and QNX-specific, it is often clearer to use the QNX kernel call:

```c
TimerSettime()
```

Instead of only using:

```c
timer_settime()
```

This makes it clear to readers that the code depends on QNX-specific behavior.

---

## 10. Example Goal

The example in the script wants a timer to deliver a pulse as close as possible to:

```text
4.5 ms
```

From the moment the timer is configured.

The timer is one-shot, so it fires once and does not repeat.

---

## 11. Example Flow

```text
1. Create or use a channel
2. Create a self connection using ConnectAttach()
3. Initialize a pulse event using SIGEV_PULSE_INIT()
4. Create a timer using timer_create()
5. Set high-resolution tolerance using TimerSettime() with TIMER_TOLERANCE
6. Configure the timer to fire after 4.5 ms
7. Receive the pulse when the timer expires
```

---

## 12. Pulse Event Setup

A timer can deliver a pulse when it expires.

Concept:

```c
SIGEV_PULSE_INIT(&event, coid, priority, TIMER_PULSE_CODE, value);
```

Meaning:

```text
When the timer expires, deliver a pulse to this connection ID.
```

This is useful when a server already has a `MsgReceive()` loop.

---

## 13. Timer Creation

Example concept:

```c
timer_create(CLOCK_MONOTONIC, &event, &timerid);
```

`CLOCK_MONOTONIC` is usually a good choice for relative timing because it is based on time since boot and does not go backward.

---

## 14. Setting the High-Resolution Tolerance

Before starting the timer, configure the tolerance.

Concept:

```text
TimerSettime(timerid, TIMER_TOLERANCE, tolerance_spec, NULL);
```

Example tolerance:

```text
5 ns
```

The interval field is not used for the tolerance setup and should be zero.

---

## 15. Setting a 4.5 ms One-Shot Timer

4.5 milliseconds equals:

```text
4,500,000 ns
```

Timer value:

```text
it_value.tv_sec  = 0
it_value.tv_nsec = 4500000
```

Since this is a one-shot timer:

```text
it_interval.tv_sec  = 0
it_interval.tv_nsec = 0
```

Then start the timer with:

```c
timer_settime(timerid, 0, &spec, NULL);
```

The `0` flag means the timer is relative, not absolute.

---

## 16. Normal Scheduling Delay Still Applies

Even with a high-resolution timer, your thread may not run exactly at the expiry time.

Reasons include:

```text
Higher-priority thread is running
CPU load
Scheduler decisions
Interrupt/event delivery overhead
```

So high-resolution timers improve expiry precision, not guaranteed execution instant.

---

## 17. When to Use High-Resolution Timers

Use high-resolution timers when you need:

```text
Sub-millisecond timer precision
More accurate timer expiry
Precise pulse delivery
Low-latency timer notification
```

Do not use them everywhere, because they add overhead and require privilege.

---

## 18. Quick Comparison

| Timer Type            | Behavior                       | Overhead | Use Case                           |
| --------------------- | ------------------------------ | -------- | ---------------------------------- |
| Normal timer          | Aligned to ticksize            | Lower    | Normal sleeps, periodic tasks      |
| High-resolution timer | Fires closer to requested time | Higher   | Precise timing, low-latency events |

---

## Final Summary

```text
Normal timers:
    Usually aligned to system ticksize

High-resolution timers:
    Try to fire closer to the requested expiry time
    Use TIMER_TOLERANCE
    Tolerance must be less than ticksize
    Require special QNX ability
    Still have possible scheduling delay
```

Golden sentence:

```text
High-resolution timers in QNX are used when the timer expiry must be more precise than the normal system ticksize; they require TIMER_TOLERANCE, extra privilege, and still depend on scheduling after the event is delivered.
```