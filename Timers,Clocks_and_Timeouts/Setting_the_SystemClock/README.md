# QNX Getting and Setting the System Clock — Quick README

## Core Idea

QNX handles time internally as **64-bit nanosecond values**.

There are two main clocks you will commonly use:

```text
CLOCK_MONOTONIC
CLOCK_REALTIME
```

They are used for different purposes.

---

## 1. `CLOCK_MONOTONIC`

`CLOCK_MONOTONIC` represents:

```text
Nanoseconds since system boot
```

It is derived from `ClockCycles()`, which reads a high-resolution hardware counter.

### Key Points

```text
CLOCK_MONOTONIC:
    - Starts from boot
    - Keeps increasing
    - Does not go backward
    - Cannot be set manually
```

### Use It For

```text
Elapsed time measurement
Timeouts
Latency measurement
Performance measurement
```

Example use case:

```text
Measure how long a function takes to execute.
```

---

## 2. `CLOCK_REALTIME`

`CLOCK_REALTIME` represents:

```text
Nanoseconds since January 1, 1970
```

This is the normal system clock or wall-clock time.

It is calculated like this:

```text
CLOCK_REALTIME = CLOCK_MONOTONIC + stored boot time
```

### Key Points

```text
CLOCK_REALTIME:
    - Represents real calendar time
    - Can be changed
    - Used by POSIX time functions
    - Depends on correct boot/system time
```

### Use It For

```text
Current date and time
Logs with real timestamps
File timestamps
POSIX time-based applications
```

---

## 3. What Happens If No Real Time Is Supplied?

During boot, the startup code may provide the current time to the kernel.

If no valid time is supplied, QNX may assume the boot time is:

```text
January 1, 1970
```

This means the system clock may appear to start from 1970 until something updates it.

---

## 4. Sources That Can Set Real Time

The system time can be updated from:

```text
RTC  - Real-Time Clock
NTP  - Network Time Protocol
PTP  - Precision Time Protocol
GPS  - Time from GPS satellites
```

These are used to correct `CLOCK_REALTIME`.

---

## 5. POSIX Time Types

### `time_t`

`time_t` stores:

```text
Seconds since January 1, 1970
```

In QNX, `time_t` is defined as a 64-bit integer.

---

### `struct timespec`

`struct timespec` provides higher resolution time.

```c
struct timespec {
    time_t tv_sec;
    long   tv_nsec;
};
```

Meaning:

```text
tv_sec:
    Seconds

tv_nsec:
    Nanoseconds after the last full second
```

Example:

```text
tv_sec  = 10
tv_nsec = 500000000
```

This means:

```text
10.5 seconds
```

---

## 6. Reading a Clock

Use:

```c
clock_gettime()
```

Example for real time:

```c
struct timespec ts;

clock_gettime(CLOCK_REALTIME, &ts);
```

Example for monotonic time:

```c
struct timespec ts;

clock_gettime(CLOCK_MONOTONIC, &ts);
```

---

## 7. Setting the System Clock

Use:

```c
clock_settime()
```

You can set:

```text
CLOCK_REALTIME
```

You cannot set:

```text
CLOCK_MONOTONIC
```

Example idea:

```c
struct timespec ts;

clock_gettime(CLOCK_REALTIME, &ts);

ts.tv_sec += 24L * 60L * 60L;

clock_settime(CLOCK_REALTIME, &ts);
```

This adds one day to the real-time clock.

### Important

Changing the system clock requires the proper QNX ability or privilege.

---

## 8. Why Not Set Time Suddenly?

Changing time suddenly can cause problems for applications.

Example problem:

```text
Time jumps forward
Time jumps backward
Timeout calculations break
Logs become confusing
```

For smoother correction, use:

```text
ClockAdjust()
```

---

## 9. `ClockAdjust()`

`ClockAdjust()` applies a time correction gradually over a period.

This is useful when you want to keep the clock synchronized without causing sudden time jumps.

Example idea from the script:

```text
Increment: 10,000 ns = 10 microseconds
Tick count: 100,000 ticks
Default tick: 1 ms
```

Total time period:

```text
100,000 ticks * 1 ms = 100 seconds
```

Total adjustment:

```text
10,000 ns * 100,000 = 1,000,000,000 ns = 1 second
```

So the system applies:

```text
1 second of correction over 100 seconds
```

### Positive and Negative Adjustment

```text
Positive increment:
    Speeds up the clock gradually

Negative increment:
    Slows down the clock gradually
```

Changing time with `ClockAdjust()` also requires the proper ability.

---

## 10. Timespec / Nanosecond Conversion Helpers

QNX provides helper functions to convert between `struct timespec` and 64-bit nanoseconds.

### `timespec2nsec()`

Converts:

```text
struct timespec -> nanoseconds
```

Example:

```text
2 seconds + 500,000,000 ns
= 2,500,000,000 ns
```

---

### `nsec2timespec()`

Converts:

```text
nanoseconds -> struct timespec
```

Example:

```text
2,500,000,000 ns
= 2 seconds + 500,000,000 ns
```

---

## 11. Getting Time Directly in Nanoseconds

QNX also provides helper functions that return time directly as nanoseconds.

These are useful when you do not want to manually use `struct timespec`.

Conceptually:

```text
clock_gettime_nanoseconds(CLOCK_MONOTONIC)
clock_gettime_nanoseconds(CLOCK_REALTIME)
```

There may also be convenience functions for directly getting monotonic or real-time nanoseconds.

---

## 12. Which Clock Should You Use?

| Use Case               | Recommended Clock |
| ---------------------- | ----------------- |
| Measuring elapsed time | `CLOCK_MONOTONIC` |
| Timeouts               | `CLOCK_MONOTONIC` |
| Latency measurement    | `CLOCK_MONOTONIC` |
| Current date and time  | `CLOCK_REALTIME`  |
| Log timestamps         | `CLOCK_REALTIME`  |
| File timestamps        | `CLOCK_REALTIME`  |
| System clock setting   | `CLOCK_REALTIME`  |

---

## 13. Common Mistake

Do not use `CLOCK_REALTIME` for measuring durations.

Why?

Because `CLOCK_REALTIME` can change if the system time is updated.

Better:

```text
Use CLOCK_MONOTONIC for measuring time intervals.
```

---

## Final Summary

```text
QNX stores time internally as 64-bit nanoseconds.

CLOCK_MONOTONIC:
    Time since boot
    Does not go backward
    Cannot be set
    Best for measuring intervals

CLOCK_REALTIME:
    Time since January 1, 1970
    Represents real system time
    Can be set or adjusted
    Best for calendar time and timestamps

clock_gettime():
    Reads a clock

clock_settime():
    Sets CLOCK_REALTIME

ClockAdjust():
    Smoothly adjusts time over a period

timespec2nsec() / nsec2timespec():
    Convert between POSIX timespec and nanoseconds
```

Golden sentence:

```text
Use CLOCK_MONOTONIC for measuring time intervals, and use CLOCK_REALTIME when you need real calendar time that may be set from RTC, NTP, PTP, or GPS.
```