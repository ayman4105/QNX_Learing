# QNX Synchronization — Quick Study README

## Core Idea

Threads inside the same process share the process resources.

```text
Process
 |
 +-- Thread 1
 +-- Thread 2
 +-- Thread 3
 |
 +-- shared memory
 +-- file descriptors
 +-- channels
 +-- timers
 +-- global variables
```

This sharing is useful, but it can create bugs if threads access the same resource at the same time.

Golden sentence:

```text
Synchronization protects shared resources from unsafe access by multiple threads.
```

---

## 1. Why Synchronization Is Needed

Threads in the same process share resources such as:

```text
memory
global variables
file descriptors
channels
timers
GPIO / hardware resources
```

Example global variable:

```c
int counter = 0;
```

Both threads can access it:

```text
Thread A writes counter
Thread B reads counter
```

Problem:

```text
If one thread writes while another reads,
the reader may get invalid, incomplete, or stale data.
```

---

## 2. Race Condition Example

```text
counter = 5

Thread A reads counter = 5
Thread B reads counter = 5
Thread A writes counter = 6
Thread B writes counter = 6

Expected result = 7
Actual result   = 6
```

This is called:

```text
Race condition
```

Meaning:

```text
The final result depends on thread timing.
```

---

## 3. Reader/Writer Problem

If a writer updates data while a reader is reading it, the reader may see mixed old/new data.

Example:

```text
vehicle_status:
    speed
    rpm
    gear
```

Bad timing:

```text
Writer updates speed
Reader reads speed = new value
Reader reads rpm   = old value
Reader reads gear  = old value
```

Result:

```text
Reader gets inconsistent data.
```

---

## 4. Main Synchronization Tools

This section focuses mainly on:

```text
mutexes
condition variables / condvars
atomic operations
```

Other useful tools:

```text
semaphores
reader/writer locks
barriers
once control
thread local storage
```

---

## 5. Mutex

`mutex` means:

```text
Mutual Exclusion
```

It allows only one thread to access a critical section at a time.

```text
Thread A
   |
   | lock mutex
   v
critical section
   |
   | unlock mutex
   v

Thread B waits until mutex is unlocked
```

Use mutexes to protect:

```text
shared counters
shared queues
global variables
hardware access
shared configuration
```

Key idea:

```text
One writer at a time.
```

---

## 6. Condition Variable / Condvar

A condition variable lets a thread wait until a condition becomes true.

Example:

```text
Consumer thread:
    waits until data is available

Producer thread:
    adds data
    signals condition variable
```

Diagram:

```text
Consumer thread
      |
      | waits on condvar
      v

Producer thread
      |
      | adds data
      | signals condvar
      v

Consumer wakes up
```

Use condvars to avoid busy waiting.

```text
Blocked thread = not consuming CPU
```

---

## 7. Atomic Operations

Atomic operations are small operations that happen as one indivisible step.

Examples:

```text
atomic increment
atomic load/store
atomic compare-and-swap
```

Use atomics for simple shared values:

```text
flags
counters
small state variables
```

Do not use atomics as a replacement for all locks.

For larger shared structures, use:

```text
mutex
reader/writer lock
```

---

## 8. Semaphore

A semaphore is like notification plus a counter.

It can represent the number of available resources.

Example:

```text
buffer has 3 items
semaphore count = 3
```

When a thread takes an item:

```text
count decreases
```

When a thread adds an item:

```text
count increases
```

If count is zero:

```text
threads block and wait
```

---

## 9. Reader/Writer Lock

Reader/writer locks allow:

```text
multiple readers at the same time
```

But if there is a writer:

```text
only one writer
no readers during writing
```

Allowed:

```text
Reader + Reader + Reader
```

Not allowed:

```text
Writer + Reader
Writer + Writer
```

Use it when:

```text
many threads read data
few threads write data
```

Example:

```text
configuration table
sensor status cache
shared lookup table
```

---

## 10. Barrier

A barrier makes threads wait until a required number of threads reach the same point.

Example use:

```text
initialization phase
```

Diagram:

```text
Thread A ----Thread B -----+--> Barrier --> all continue together
Thread C ----/
```

Meaning:

```text
All threads wait until everyone is ready.
```

---

## 11. Once Control

Once control runs an initialization function only once for the whole process.

Useful for libraries.

Example:

```text
Thread A calls library
Thread B calls library
Thread C calls library

library_init() runs only once
```

POSIX example:

```c
pthread_once();
```

Use it when:

```text
some initialization must happen exactly one time
```

---

## 12. Thread Local Storage

Thread Local Storage means each thread gets its own copy of some data.

Useful for libraries that may be used by many threads.

```text
Thread A -> private data A
Thread B -> private data B
Thread C -> private data C
```

Without TLS:

```text
all threads share one global variable
```

With TLS:

```text
each thread has its own version
```

Key idea:

```text
TLS = per-thread private data.
```

---

## 13. Who Wakes Up First?

When a synchronization object is released, QNX chooses which blocked thread wakes up.

Examples of release:

```text
mutex unlock
semaphore post
condition variable signal
reader/writer lock unlock
```

Rule:

```text
Highest-priority waiting thread wins.
If multiple waiting threads have the same highest priority,
the longest-waiting one wins.
```

Short form:

```text
highest priority, longest waiting
```

---

## 14. Wake-Up Example

Threads waiting for a mutex:

```text
Thread A priority 10 waiting 5 sec
Thread B priority 20 waiting 1 sec
Thread C priority 20 waiting 3 sec
Thread D priority 15 waiting 10 sec
```

Highest priority is:

```text
20
```

Threads with priority 20:

```text
Thread B
Thread C
```

Longest waiting among them:

```text
Thread C
```

So:

```text
Thread C gets the mutex.
```

---

## 15. Blocked Means No CPU

In QNX, a blocked thread is waiting for something and does not consume CPU.

```text
Thread waits on mutex / condvar / semaphore
        |
        v
Thread becomes blocked
        |
        v
QNX runs other runnable threads
```

This is better than busy waiting.

Bad:

```text
while (!ready)
{
    // keep checking and waste CPU
}
```

Better:

```text
block waiting for notification
```

---

## 16. Tool Comparison

| Tool                 | Main Use                            | Key Idea                        |
| -------------------- | ----------------------------------- | ------------------------------- |
| Mutex                | Protect shared resource             | One thread at a time            |
| Condvar              | Wait for condition/event            | Sleep until signaled            |
| Atomic operation     | Simple shared value update          | One indivisible operation       |
| Semaphore            | Counted notification/resource count | Counter + blocking              |
| Reader/writer lock   | Many readers or one writer          | Readers share, writer exclusive |
| Barrier              | Wait for group to arrive            | Release all together            |
| Once control         | One-time initialization             | Run function once               |
| Thread local storage | Per-thread data                     | Each thread has its own data    |

---

## 17. Common Quiz Questions

### Q1: Why do threads need synchronization?

```text
Because threads in the same process share resources.
```

### Q2: What problem happens when two threads access shared data unsafely?

```text
Race condition.
```

### Q3: What does a mutex do?

```text
Allows only one thread at a time to access a critical section.
```

### Q4: What does a condition variable do?

```text
Lets a thread wait until a condition becomes true.
```

### Q5: When is a reader/writer lock useful?

```text
When many threads read data and few threads write it.
```

### Q6: What does a barrier do?

```text
Blocks threads until the required number of threads reach the barrier.
```

### Q7: What does once control do?

```text
Runs an initialization function only once during the process lifetime.
```

### Q8: What is thread local storage?

```text
A way to store data separately for each thread.
```

### Q9: When a synchronization object is released, which blocked thread gets it?

```text
The highest-priority waiting thread.
If tied, the longest-waiting one.
```

---

## Final Summary

```text
Threads share resources.
Shared resources can cause race conditions.
Synchronization controls safe access.
```

Important tools:

```text
mutex
condvar
atomic operations
semaphore
reader/writer lock
barrier
once control
thread local storage
```

Final key sentence:

```text
In QNX, synchronization protects shared resources, and when a sync object is released, the highest-priority longest-waiting blocked thread is selected.
```