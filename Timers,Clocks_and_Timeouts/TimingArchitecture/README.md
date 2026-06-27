# QNX Timing Architecture — Quick README

## Core Idea

QNX timing has two major parts:

```text
1. Current time
   What time is it now?

2. System timers
   Wake me up after some time has passed.
```

The important difference is:

```text
ClockCycles() measures time.
System timers notify you later.
```

---

## 1. Current Time in QNX

In QNX 8, the current time is based on a hardware-supplied free-running counter.

A free-running counter is a counter that keeps increasing all the time:

```text
0, 1, 2, 3, 4, 5, ...
```

It is provided by the hardware and is used for high-resolution time measurement.

---

## 2. `ClockCycles()`

QNX provides this function:

```c
ClockCycles()
```

It returns the current value of the hardware counter.

Example usage:

```c
uint64_t start = ClockCycles();

/* Code to measure */

uint64_t end = ClockCycles();
uint64_t elapsed_cycles = end - start;
```

Important note:

```text
ClockCycles() returns cycles, not seconds.
```

To convert cycles into time, you need to know how many cycles happen per second.

---

## 3. Hardware Independence

Your code calls:

```c
ClockCycles()
```

QNX handles the hardware-specific details underneath.

For example:

```text
x86/x64:
    may use RDTSC instruction

ARM/AArch64:
    may use a platform hardware counter
```

So your code does not need to know the exact CPU instruction.

---

## 4. Synchronized Across Cores

QNX expects the hardware counter to be synchronized across all CPU cores.

This means the counter must not go backwards when code moves between cores.

Correct example:

```text
Core 0: ClockCycles() = 1000
Core 1: ClockCycles() = 1005
Core 0: ClockCycles() = 1010
```

Wrong example:

```text
Core 0: ClockCycles() = 1000
Core 1: ClockCycles() = 900
```

If the counter went backwards, timing measurements could become invalid.

---

## 5. Why Synchronization Matters

A thread may run on one CPU core, then move to another core.

Example:

```text
start = ClockCycles() on Core 0
thread moves to Core 1
end = ClockCycles() on Core 1
```

If counters are not synchronized, the calculated elapsed time may be wrong.

QNX requires supported hardware to provide a synchronized high-resolution counter.

---

## 6. Resolution

The free-running counter must have high precision.

The expected precision is around nanosecond-level resolution.

This is useful for:

```text
Latency measurement
Profiling
Real-time timing analysis
Performance measurement
```

---

## 7. System Timers

System timers are different from `ClockCycles()`.

They are used when software wants notification after time passes.

Examples:

```text
Wake me after 10 ms
Timeout after 1 second
Run this timer periodically
```

System timers are driven by hardware timer interrupts.

---

## 8. Core-Local Timer Interrupts

Each CPU core has a local hardware timer.

```text
Core 0 -> local timer
Core 1 -> local timer
Core 2 -> local timer
```

When a timer expires, it generates an interrupt.

That interrupt wakes a core-local clock IST.

IST means:

```text
Interrupt Service Thread
```

---

## 9. Default System Tick Size

The default QNX system tick size is:

```text
1 millisecond
```

This means the default timer resolution is around 1 ms.

Important distinction:

```text
ClockCycles():
    high-resolution measurement

System timer tick:
    default timer interrupt resolution
```

---

## 10. Cycles Per Second / CPS

Because `ClockCycles()` returns cycles, you need the counter rate.

This value is called:

```text
cycles per second
CPS
```

It tells you how many counter cycles happen in one second.

To calculate elapsed time:

```text
elapsed_cycles = end - start
elapsed_seconds = elapsed_cycles / cycles_per_second
```

---

## 11. System Page and `qtime`

QNX stores timing information in the system page.

The time-related part is called:

```text
qtime
```

It contains information such as:

```text
cycles_per_sec
```

Your application can read this information from the system page.

---

## 12. Checking CPS with `pidin`

You can inspect QNX timing information using `pidin`.

Example idea:

```sh
pidin syspage=qtime
```

Look for:

```text
CPS
```

CPS means:

```text
Cycles Per Second
```

On some systems, this value may be close to the CPU frequency.

---

## 13. Quick Comparison

| Feature                | Purpose                | Main Idea                              |
| ---------------------- | ---------------------- | -------------------------------------- |
| `ClockCycles()`        | Measure time           | Reads high-resolution hardware counter |
| System timers          | Notify later           | Uses timer interrupt and clock IST     |
| `cycles_per_sec` / CPS | Convert cycles to time | Counter frequency                      |
| System page / `qtime`  | Store timing info      | Provides CPS and timing metadata       |

---

## Final Summary

QNX timing architecture is based on two main ideas:

```text
1. A synchronized high-resolution hardware counter
   Read using ClockCycles()

2. Per-core hardware timers
   Used to generate timer interrupts and wake software later
```

Golden sentence:

```text
ClockCycles() is used to measure time using a synchronized high-resolution hardware counter, while QNX system timers are used to notify software after time has passed.
```