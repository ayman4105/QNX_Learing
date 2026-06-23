# QNX Processes, Threads & Synchronization — Quick Study README

## Core Idea

This section is about how QNX runs software and how multiple execution paths coordinate safely.

```text
Process  = owns resources
Thread   = runs code
Sync     = protects shared data/resources
```

Golden sentence:

```text
A process owns resources, threads execute code, and synchronization protects shared data between threads.
```

---

## 1. Program vs Process vs Thread

```text
Program:
    executable file on storage

Process:
    running instance of a program
    owns resources

Thread:
    execution path inside a process
    actually runs code on the CPU
```

Diagram:

```text
Program file
    |
    | run
    v
Process
    |
    +-- memory address space
    +-- file descriptors
    +-- connections
    +-- permissions
    |
    +-- Thread 1
    +-- Thread 2
    +-- Thread 3
```

Key point:

```text
Process owns resources.
Thread executes code.
```

---

## 2. What Does a Process Own?

A process owns system resources such as:

```text
PID
virtual memory address space
file descriptors
connections/channels
permissions/security abilities
one or more threads
```

In QNX, many OS services and drivers are also processes.

```text
driver program -> running driver process -> one or more threads
```

---

## 3. What Does a Thread Own?

A thread has its own execution state:

```text
stack
register context
priority
scheduling state
thread ID
```

But threads in the same process share:

```text
global variables
heap memory
file descriptors
process resources
```

Diagram:

```text
Process memory
   |
   +-- shared heap
   +-- shared globals
   |
   +-- Thread A stack
   +-- Thread B stack
```

---

## 4. QNX Schedules Threads

Important QNX rule:

```text
QNX schedules threads, not processes.
```

The scheduler looks at runnable threads and runs the highest-priority ones.

```text
Runnable threads
      |
      v
QNX Scheduler
      |
      v
Running threads on CPU cores
```

Blocked threads are ignored by the scheduler until they become runnable again.

---

## 5. Why Use Multiple Threads?

Multiple threads are useful when one process needs to handle multiple activities.

Example: driver process

```text
Thread 1 -> handle client requests
Thread 2 -> wait for hardware events/interrupts
Thread 3 -> handle timeout/monitoring
```

Example: application process

```text
Thread 1 -> user interface
Thread 2 -> networking
Thread 3 -> logging
```

Why not one thread?

```text
One blocking operation could stop the whole process work.
```

---

## 6. The Main Problem: Shared Data

Threads in the same process share memory.

This can cause race conditions.

Example:

```text
counter = 5

Thread A reads counter = 5
Thread B reads counter = 5
Thread A writes counter = 6
Thread B writes counter = 6

Expected: 7
Actual:   6
```

Problem:

```text
Two threads touched the same data at the same time without protection.
```

This is called:

```text
Race condition
```

---

## 7. Synchronization

Synchronization means controlling access to shared resources.

Used to prevent:

```text
race conditions
data corruption
wrong ordering
deadlocks if used badly
```

Common synchronization tools:

```text
mutex
condition variable / condvar
semaphore
join
signals/events/pulses depending on design
```

---

## 8. Mutex

`mutex` means:

```text
Mutual Exclusion
```

It allows only one thread to enter a critical section at a time.

Diagram:

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

Use mutex when multiple threads access the same shared data.

Example use:

```text
Protect shared counter
Protect shared queue
Protect shared configuration
```

---

## 9. Condition Variable / Condvar

A condition variable lets a thread sleep until a condition becomes true.

Example:

```text
Thread A:
    waits until data is available

Thread B:
    receives data
    signals Thread A
```

Diagram:

```text
Thread A waits
      ^
      |
condition signal
      |
Thread B produces data
```

Use condvar when:

```text
A thread should wait for a state change
instead of wasting CPU in a loop
```

---

## 10. Process Death Detection

Detecting when a process dies is important for reliable systems.

Why?

```text
restart failed component
log failure
cleanup resources
notify supervisor
enter safe state
```

Example:

```text
Sensor service dies
        |
        v
Supervisor detects failure
        |
        v
Restart service or enter safe mode
```

---

## 11. Thread Death Detection

If a process has multiple threads, you may need to detect when a thread exits.

Reasons:

```text
join with finished thread
cleanup thread resources
detect internal failure
restart worker thread
```

Example:

```text
Worker thread exits unexpectedly
        |
        v
Main thread detects it
        |
        v
cleanup / restart / report error
```

---

## 12. Important Mental Graph

```text
Process
 |
 +-- owns resources
 |
 +-- Thread 1 ---> executes code
 +-- Thread 2 ---> executes code
 +-- Thread 3 ---> executes code
 |
 +-- shared memory/resources
        |
        v
   synchronization needed
```

---

## 13. Common Quiz Questions

### Q1: What is a process?

```text
A running program that owns resources.
```

### Q2: What is a thread?

```text
An execution unit inside a process that runs code.
```

### Q3: What does QNX schedule?

```text
Threads.
```

### Q4: Why use multiple threads?

```text
To handle multiple activities concurrently inside one process.
```

### Q5: Why do threads need synchronization?

```text
Because threads in the same process share memory and resources.
```

### Q6: What is a mutex used for?

```text
To allow only one thread at a time to access a critical section.
```

### Q7: What is a condvar used for?

```text
To let a thread wait until a condition becomes true.
```

### Q8: Why detect process/thread death?

```text
For cleanup, restart, logging, monitoring, and safe-state handling.
```

---

## Final Summary

```text
Process:
    owns resources

Thread:
    runs code

Multiple threads:
    allow concurrent work

Synchronization:
    protects shared data

QNX:
    schedules runnable threads by priority
```

Final key sentence:

```text
In QNX, processes provide resource ownership, threads provide execution, and synchronization keeps shared access safe.
```