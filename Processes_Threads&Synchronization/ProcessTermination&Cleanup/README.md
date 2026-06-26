# QNX Process Termination & Cleanup — Quick Study README

## Core Idea

When a process terminates, cleanup happens in two ways:

```text
1. Internal cleanup
    Code inside your process gets a chance to clean up.

2. External cleanup
    QNX cleans up process-owned resources.
```

Golden sentence:

```text
Normal termination gives your code a chance to clean up, but QNX always cleans process-owned resources; named objects survive until explicitly unlinked.
```

---

## 1. Internal Cleanup

Internal cleanup means cleanup code inside the process runs before the process exits.

This happens only during:

```text
normal termination
```

Examples of internal cleanup:

```text
atexit() handlers run
stdio buffers are flushed
C++ global destructors run
normal exit processing happens
```

---

## 2. Normal Termination

Normal termination happens when:

```text
exit() is called
main() returns
```

Examples:

```c
exit(0);
```

or:

```c
int main(void)
{
    return 0;
}
```

Flow:

```text
main() returns
      |
      v
exit() is called
      |
      v
normal exit processing
      |
      v
internal cleanup runs
      |
      v
QNX external cleanup runs
```

---

## 3. What Runs During Normal Exit?

### atexit() handlers

`atexit()` registers functions to run when the process exits normally.

```c
atexit(cleanup_function);
```

Meaning:

```text
When normal exit happens, run this cleanup function.
```

### Buffer flushing

Buffered output is flushed.

Examples:

```text
stdio buffers
C++ streams
file streams
```

This means buffered data is written to its destination.

### Global destructors

In C++, global/static destructors run during normal exit.

```text
C++ global objects
    |
    v
destructors run on normal exit
```

---

## 4. Abnormal Termination

Abnormal termination happens when the process dies unexpectedly or outside normal `exit()` processing.

Examples:

```text
crash
divide by zero
invalid pointer dereference
SIGSEGV
SIGKILL
unhandled signal
last thread exits abnormally
last thread is canceled/aborted
```

Flow:

```text
process crash / SIGKILL / SIGSEGV
      |
      v
no normal internal cleanup
      |
      v
QNX external cleanup still runs
```

---

## 5. What Does Not Run During Abnormal Termination?

During abnormal termination, normal exit processing does not happen.

So these are not guaranteed:

```text
atexit() handlers
stdio buffer flushing
C++ global destructors
normal cleanup code
```

Important:

```text
If the process crashes, do not assume your internal cleanup code will run.
```

---

## 6. External Cleanup by QNX

External cleanup is done by the operating system.

QNX cleans up resources owned by the process even if the process crashes.

Examples:

```text
file descriptors are closed
QNX connections are destroyed
channels are destroyed
memory mappings are unmapped
timers are deleted
interrupt handlers are removed
```

Diagram:

```text
Process dies
    |
    v
QNX cleans:
    file descriptors
    connections
    channels
    memory mappings
    timers
    interrupts
```

---

## 7. Memory Cleanup

QNX unmaps memory owned by the process.

This includes:

```text
stack
heap
code segment
data segment
shared memory mappings
hardware mappings
explicit mmap() regions
```

Diagram:

```text
Process virtual memory
    |
    +-- stack
    +-- heap
    +-- code/data
    +-- shared memory mapping
    +-- hardware mapping
          |
          v
All unmapped when process dies
```

Important:

```text
The mapping is removed from that process.
```

---

## 8. Named Objects Exception

Some objects can survive process death if they have a pathname/name.

Examples:

```text
regular files
shared memory objects
named semaphores
POSIX message queues
```

Why?

Because the object is not only owned by the process; it is also reachable by name.

---

## 9. Regular File Example

If a process creates a file:

```c
open("data.txt", O_CREAT | O_WRONLY);
```

Then exits:

```text
process exits
    |
    v
file still exists
```

This is expected behavior.

Files are named objects and are not deleted just because the creating process died.

---

## 10. Other Named Objects

Named objects can be created with:

```text
shm_open()  -> shared memory object
sem_open()  -> named semaphore
mq_open()   -> POSIX message queue
```

Examples:

```text
/my_shared_memory
/my_semaphore
/my_queue
```

These can survive process death while their names exist.

---

## 11. How Named Objects Are Removed

Named objects must be explicitly unlinked.

```text
regular file:
    unlink()

shared memory object:
    shm_unlink()

named semaphore:
    sem_unlink()

POSIX message queue:
    mq_unlink()
```

Example:

```c
shm_unlink("/my_shared_memory");
```

---

## 12. When Is the Underlying Data Actually Released?

The object data is released only when both conditions are true:

```text
1. the name has been unlinked
2. all processes using it have closed it
```

Diagram:

```text
named object exists
      |
      +-- Process A has it open
      +-- Process B has it open

unlink name
      |
      v
name is removed
but data remains while still open

all processes close it
      |
      v
data is released
```

This is similar to Unix file behavior.

---

## 13. Normal vs Abnormal Termination

| Item                    | Normal Termination              | Abnormal Termination      |
| ----------------------- | ------------------------------- | ------------------------- |
| Example                 | `exit()` / `return from main()` | crash / SIGSEGV / SIGKILL |
| `atexit()` handlers     | Run                             | Do not run                |
| stdio/C++ stream flush  | Happens                         | Not guaranteed            |
| C++ global destructors  | Run                             | Do not run                |
| QNX external cleanup    | Happens                         | Happens                   |
| File descriptors closed | Yes                             | Yes                       |
| Memory unmapped         | Yes                             | Yes                       |
| Named objects survive   | Yes, until unlinked             | Yes, until unlinked       |

---

## 14. Internal vs External Cleanup

| Cleanup Type     | Done By                     | Happens When                   | Examples                                  |
| ---------------- | --------------------------- | ------------------------------ | ----------------------------------------- |
| Internal cleanup | Your process code / runtime | Normal termination only        | `atexit()`, buffer flush, C++ destructors |
| External cleanup | QNX OS                      | Normal or abnormal termination | close FDs, unmap memory, destroy channels |

---

## 15. Common Quiz Questions

### Q1: What are the two cleanup types after process termination?

```text
Internal cleanup and external cleanup.
```

### Q2: When does internal cleanup happen?

```text
During normal termination.
```

### Q3: What causes normal termination?

```text
exit() or return from main().
```

### Q4: What happens during normal exit processing?

```text
atexit handlers run, buffers flush, and C++ global destructors run.
```

### Q5: What is abnormal termination?

```text
Process death due to crash, invalid memory access, signal, or abnormal last-thread termination.
```

### Q6: Does abnormal termination run atexit handlers?

```text
No.
```

### Q7: Does QNX still clean process-owned resources after abnormal death?

```text
Yes.
```

### Q8: What resources does QNX clean automatically?

```text
file descriptors, connections, channels, memory mappings, timers, and interrupt handling.
```

### Q9: What is the exception to automatic disappearance?

```text
Named objects may survive process death.
```

### Q10: How do you remove named objects?

```text
Use unlink(), shm_unlink(), sem_unlink(), or mq_unlink().
```

---

## Final Summary

```text
Normal termination:
    internal cleanup runs
    QNX external cleanup runs

Abnormal termination:
    internal cleanup does not run
    QNX external cleanup still runs

Named objects:
    survive process death
    must be explicitly unlinked
```

Final key sentence:

```text
Your cleanup code runs only on normal exit, but QNX cleans process-owned resources on any process death; named objects remain until unlinked and closed by all users.
```