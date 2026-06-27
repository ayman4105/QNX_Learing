# QNX Timing Design Considerations — Quick README

## Core Idea

This section focuses on two important timing design issues in QNX:

```text
1. Running periodically without accumulating timing error
2. High-frequency timer issues and jitter
```

The main idea is:

```text
Do not build accurate periodic tasks using repeated delay() calls.
Use a periodic timer managed by the kernel instead.
```

---

## 1. The Problem With Delay Loops

A common but bad pattern for accurate periodic work is:

```c
while (1) {
    delay(5_ms);
    do_something();
}
```

At first, this looks correct:

```text
sleep 5 ms
run work
sleep 5 ms
run work
```

But this can accumulate timing error.

---

## 2. Why Delay Loops Accumulate Error

When you call:

```text
delay(5 ms)
```

QNX wakes your thread at a suitable timer boundary.

However, when the timer expires, your thread does not always run immediately.

Example:

```text
Timer expires
Thread becomes READY
Higher-priority thread is running
Your thread waits
Higher-priority thread blocks
Your thread finally runs
```

So there is a difference between:

```text
Timer expiry time
```

and:

```text
Actual thread execution time
```

That scheduling delay becomes part of the next period if you call `delay()` again after your work.

---

## 3. Accumulated Error Example

Suppose the loop asks for 5 ms delay.

Expected behavior:

```text
Wake every 5 ms
```

But actual behavior can be:

```text
Wake after 5 ms timer expiry
Wait 3 ms because a higher-priority thread is running
Run do_something()
Call delay(5 ms) again from this late time
```

Now the next 5 ms delay starts from the late point.

This means the error is carried forward.

```text
Small delay once
    -> becomes part of the next period
    -> error accumulates over time
```

---

## 4. Bad Pattern Timeline

```text
Start
  |
  |--- delay 5 ms ---| scheduling delay | work |
                                             |
                                             +--- next delay starts here
```

The next delay starts after the previous delay, scheduling delay, and work time.

This shifts the periodic schedule over time.

---

## 5. Correct Pattern: Periodic Timer

A better design is to let the kernel manage the periodic schedule.

Use:

```c
timer_create()
timer_settime()
```

Then block waiting for the timer event.

The blocking function may be:

```text
MsgReceive()
sigwaitinfo()
sem_wait()
other event wait mechanism
```

Example idea:

```c
timer_settime(timerid, 0, &periodic_spec, NULL);

while (1) {
    wait_for_timer_event();
    do_something();
}
```

---

## 6. Why Periodic Timers Avoid Accumulated Error

With a periodic timer, the kernel keeps the schedule fixed.

If the period is 5 ms, the kernel schedules timer expiries like:

```text
T0 + 5 ms
T0 + 10 ms
T0 + 15 ms
T0 + 20 ms
```

Even if your thread runs late once, the next timer expiry is still based on the fixed periodic schedule.

The error does not get added into the next period.

---

## 7. Good Pattern Timeline

```text
T0
 |---- 5 ms ----|---- 5 ms ----|---- 5 ms ----|---- 5 ms ----|
 timer fires    timer fires    timer fires    timer fires
```

Your thread may run slightly late because of scheduling, but the timer itself does not drift.

---

## 8. Delay Loop vs Periodic Timer

| Design                  | Result                 |
| ----------------------- | ---------------------- |
| Repeated `delay()` loop | Accumulates error      |
| Kernel periodic timer   | Keeps a fixed schedule |

Use periodic timers when accurate periodic behavior matters.

---

## 9. High-Frequency Timer Issue

A second issue appears when the timer period does not align with the system ticksize.

Example:

```text
ticksize = 1 ms
timer period = 1.5 ms
```

Normal timers align to tick boundaries.

Tick boundaries are:

```text
1 ms, 2 ms, 3 ms, 4 ms, 5 ms, 6 ms, ...
```

But a 1.5 ms timer wants to expire at:

```text
1.5 ms, 3.0 ms, 4.5 ms, 6.0 ms, 7.5 ms, ...
```

Some of those times fall between tick boundaries.

---

## 10. 1.5 ms Timer With 1 ms Ticksize

For a normal timer, QNX cannot wake the thread before the requested time.

So it aligns the wakeup to the next tick boundary.

Expected expiry points:

```text
1.5 ms -> wakes at 2 ms
3.0 ms -> wakes at 3 ms
4.5 ms -> wakes at 5 ms
6.0 ms -> wakes at 6 ms
```

This creates this pattern:

```text
2 ms, 1 ms, 2 ms, 1 ms, 2 ms, 1 ms
```

The average is still correct:

```text
(2 ms + 1 ms) / 2 = 1.5 ms
```

But the interval is not stable.

This is called jitter.

---

## 11. Average Timing vs Jitter

The average period may be correct, but the wakeup spacing may vary.

```text
Average timing:
    correct over time

Jitter:
    individual wakeups are uneven
```

For some applications, average timing is enough.

For others, jitter is a problem.

---

## 12. Why the Jitter Happens

Normal timers are aligned to the system tick boundary.

If:

```text
ticksize = 1 ms
requested period = 1.5 ms
```

then every other expiry falls between ticks.

QNX must round to the next tick boundary, so the wakeup spacing alternates.

---

## 13. Solution 1: Use a Different Hardware Timer

If the chip has another hardware timer, a driver can program that timer directly.

```text
Use a hardware timer interrupt
Program the timer yourself
Handle the interrupt in your driver
```

This gives the most control, but it is more hardware-specific and requires driver work.

---

## 14. Solution 2: Reduce the Ticksize

A smaller ticksize reduces rounding error.

Example:

```text
Instead of ticksize = 1 ms
Use ticksize = 0.25 ms
```

Then:

```text
1.5 ms = 6 * 0.25 ms
```

This aligns better.

### Tradeoff

A smaller ticksize can increase system overhead.

```text
Smaller ticksize
    -> more timer interrupts
    -> more kernel overhead
```

---

## 15. Solution 3: Use a Timer Period That Is an Exact Multiple of Ticksize

If possible, choose a timer period that is an exact multiple of the system ticksize.

Example with 1 ms ticksize:

```text
Good periods:
    1 ms
    2 ms
    3 ms
    4 ms
    5 ms
```

Avoid periods such as:

```text
1.5 ms
2.3 ms
0.99847 ms
```

unless you can tolerate jitter or use high-resolution timers.

---

## 16. Use `ClockPeriod()` to Query the Real Ticksize

Do not assume the ticksize is exactly 1 ms.

Use:

```c
ClockPeriod()
```

This lets you query the actual current ticksize.

Then you can choose a period that aligns properly with the real ticksize.

---

## 17. Use `CLOCK_MONOTONIC` for Periodic Timers

For periodic timers and timeouts, use:

```text
CLOCK_MONOTONIC
```

Why?

```text
CLOCK_MONOTONIC:
    - Time since boot
    - Does not go backward
    - Is not affected by system clock changes
```

Avoid using `CLOCK_REALTIME` for periodic intervals because it can be affected by system time changes, NTP, PTP, or `ClockAdjust()`.

---

## 18. Solution 4: Use High-Resolution Timers

High-resolution timers avoid normal tick alignment.

They try to fire as close as possible to the requested expiry time.

Example:

```text
Requested period = 1.5 ms
High-resolution timer tries to expire near 1.5 ms
```

### Tradeoff

High-resolution timers can increase overhead.

```text
More precise expiry
    -> more hardware timer programming
    -> more overhead
```

They may also require special privileges or abilities.

---

## 19. High-Resolution Timers Do Not Remove Scheduling Delay

A high-resolution timer improves expiry accuracy.

But it does not guarantee that your thread runs immediately.

The thread can still be delayed by:

```text
Higher-priority threads
Interrupt handling overhead
Scheduler behavior
System load
```

So high-resolution timers improve timer precision, not complete execution determinism.

---

## 20. Practical Design Rules

```text
Do not use repeated delay() loops for accurate periodic work.
Use timer_create() and timer_settime().
Use CLOCK_MONOTONIC for periodic timers.
Make your period an exact multiple of ticksize when possible.
Use ClockPeriod() to query the real ticksize.
Use high-resolution timers only when needed.
Use a dedicated hardware timer if you need full hardware-level control.
```

---

## 21. Quick Comparison

| Case                               | Behavior                                |
| ---------------------------------- | --------------------------------------- |
| Repeated delay loop                | Accumulates timing error                |
| Periodic kernel timer              | Avoids accumulated error                |
| 1.5 ms normal timer with 1 ms tick | Average correct, but jitter appears     |
| Smaller ticksize                   | Less rounding error, more overhead      |
| Exact multiple of ticksize         | Stable wakeups                          |
| High-resolution timer              | More precise expiry, more overhead      |
| Custom hardware timer              | Maximum control, more driver complexity |

---

## Final Summary

```text
Repeated delay loops are not good for accurate periodic timing because scheduling delays accumulate.

Periodic timers are better because the kernel maintains the timing schedule.

Normal timers align to tick boundaries, so periods that are not exact multiples of ticksize can cause jitter.

To reduce jitter, use exact tick multiples, reduce ticksize, use high-resolution timers, or program a dedicated hardware timer.

For periodic timers, prefer CLOCK_MONOTONIC so the timer is not affected by system clock changes.
```

Golden sentence:

```text
For periodic work in QNX, do not repeatedly call delay(), because scheduling delays accumulate error; instead, use a periodic timer with timer_settime(), preferably based on CLOCK_MONOTONIC, and be aware that normal timers align to tick boundaries unless you use high-resolution timers or your own hardware timer.
```