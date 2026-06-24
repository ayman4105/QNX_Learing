# QNX Thread Operations — Quick Study README

## 0. Thread Operations Cheat Sheet

The slide summary in text form:

```text
pthread_exit(retval)
    Terminate the calling thread.

pthread_join(tid, &retval)
    Wait for a thread to die and get its return value.

pthread_kill(tid, signo)
    Send signal signo to thread tid.
    Important: it does not simply kill the thread.

pthread_cancel(tid)
    Request that a thread terminates.

pthread_detach(tid)
    Make the thread detached / unjoinable.

tid = pthread_self()
    Get the calling thread ID.

pthread_setname_np()
    Give a thread a debug-friendly name.
    np means non-portable.
```

Golden sentence:

```text
A thread should normally finish cleanly: stop work, clean up resources, then return or call pthread_exit().
```

---

## 1. How a Thread Terminates

A created thread can end in two normal ways:

```text
1. return from the thread function
2. call pthread_exit()
```

Example:

```c
void *worker(void *arg)
{
    return NULL;
}
```

Or:

```c
void *worker(void *arg)
{
    pthread_exit(NULL);
}
```

For a thread created by `pthread_create()`, returning from the thread function behaves like calling:

```c
pthread_exit(return_value);
```

---

## 2. pthread_exit()

`pthread_exit()` terminates the calling thread and stores a return value.

```c
pthread_exit(retval);
```

Flow:

```text
Thread
  |
  | pthread_exit(retval)
  v
Thread terminates
  |
  v
retval can be collected by pthread_join()
```

Important:

```text
pthread_exit() ends the calling thread, not the whole process.
```

---

## 3. pthread_join()

`pthread_join()` waits for another thread to finish and collects its return value.

```c
pthread_join(tid, &retval);
```

Meaning:

```text
tid:
    thread ID to wait for

&retval:
    place to store the thread return value
```

Flow:

```text
Thread A
  |
  | pthread_join(Thread B)
  v
waits until Thread B exits
  |
  v
gets Thread B return value
```

If the target thread is still running, `pthread_join()` blocks.

If the target thread already exited, `pthread_join()` returns immediately with its stored result.

---

## 4. Threads Do Not Have Parent/Child Relationship

Processes have parent/child relationships.

Threads do not have a real parent/child relationship.

```text
Process
 |
 +-- Thread 1
 +-- Thread 2
 +-- Thread 3
```

Any thread in the same process can join another joinable thread.

```text
Thread 1 can join Thread 2
Thread 2 can join Thread 1
```

The only special moment is creation, where the new thread may inherit things from the creating thread, such as priority and scheduling policy.

---

## 5. A Thread Can Be Joined Only Once

When a thread exits, its termination information is kept until another thread joins it.

After `pthread_join()` succeeds, that information is removed.

```text
Thread B exits
      |
      v
Thread A joins Thread B
      |
      v
Thread A gets return value
      |
      v
Thread B info is removed
```

If another thread tries to join the same thread again:

```text
error
```

Key point:

```text
A joinable thread can be joined only once.
```

---

## 6. pthread_detach()

`pthread_detach()` makes a thread non-joinable.

```c
pthread_detach(tid);
```

Meaning:

```text
When this thread exits, clean it automatically.
Do not keep return value for pthread_join().
```

Joinable thread:

```text
thread exits
   |
   v
return value kept
   |
   v
pthread_join() collects it
```

Detached thread:

```text
thread exits
   |
   v
automatic cleanup
```

Important:

```text
Detached thread = no pthread_join()
Detached thread = no return value collection
```

Think of it like a thread version of no zombie.

---

## 7. pthread_self()

`pthread_self()` returns the calling thread ID.

```c
pthread_t tid = pthread_self();
```

Use it when a thread needs to:

```text
know its own ID
print debug information
pass its ID to another function
change its own scheduling settings
```

QNX extension:

```text
thread ID 0 can mean the calling thread in some QNX APIs
```

But for portable POSIX code, use:

```c
pthread_self()
```

---

## 8. pthread_kill()

Despite the name, `pthread_kill()` does not simply kill a thread.

It sends a signal to a specific thread.

```c
pthread_kill(tid, signo);
```

Possible results:

```text
signal is masked
    -> held pending

signal is ignored
    -> discarded

signal handler exists
    -> that thread runs the handler

default action is terminate
    -> the whole process may die
```

Key point:

```text
pthread_kill() = signal delivery, not guaranteed thread termination.
```

---

## 9. pthread_cancel()

`pthread_cancel()` requests that a thread terminates.

```c
pthread_cancel(tid);
```

But cancellation can be controlled by the target thread.

The thread may:

```text
accept cancellation
delay cancellation
ignore cancellation
respond only at cancellation points
```

Important warning:

```text
pthread_cancel() is asynchronous and can be dangerous.
```

Example danger:

```text
Thread locks mutex
      |
      v
pthread_cancel() happens
      |
      v
thread exits without unlocking
      |
      v
other threads may block forever
```

---

## 10. Better Design: Controlled Shutdown

A safer design is to ask the thread to stop using a flag.

```c
while (!done)
{
    wait_for_event();
    do_work();
}

cleanup();
return NULL;
```

Flow:

```text
Thread starts
      |
      v
setup
      |
      v
while not done
      |
      +--> block waiting for event
      +--> handle work
      +--> check done flag
      |
      v
cleanup
      |
      v
exit
```

This is usually better than asynchronous cancellation because the thread can clean up safely.

---

## 11. Blocking Is Normal in QNX Threads

A long-running QNX thread usually blocks while waiting for an event.

It may wait for:

```text
message
pulse
signal
condition variable
interrupt
timer
I/O event
```

While blocked:

```text
the thread does not waste CPU
other threads can run
```

---

## 12. Cleanup Before Thread Exit

Before a thread exits, it should clean up anything it owns or controls.

Clean up examples:

```text
unlock mutexes
release reader/writer locks
restore synchronization objects to known state
free allocated memory
close files or streams
close POSIX message queues
close semaphores
release thread-specific resources
```

You do not manually release the thread stack.

```text
Created thread stack is cleaned automatically.
Local variables disappear automatically.
```

---

## 13. Main Thread Is Special

Returning from a created thread function ends that thread only.

```text
worker thread returns
    -> worker thread ends
```

But returning from `main()` ends the whole process.

```text
main returns
    -> process exits
    -> all threads go away
```

Important:

```text
Main thread stack stays for the lifetime of the process.
```

Why?

```text
command-line arguments
environment variables
C runtime data
```

Design tip:

```text
If one thread should live for the whole process lifetime,
make the main thread that permanent/control thread.
```

---

## 14. Thread Naming: pthread_setname_np()

`pthread_setname_np()` gives a thread a name for debugging and system information tools.

```c
pthread_setname_np(tid, "worker");
```

Why useful?

```text
easier debugging
clearer system information
easier to know each thread purpose
```

`np` means:

```text
non-portable
```

So this function is useful, but not standard POSIX portable.

---

## 15. Scheduling Operations

You can change or query a thread scheduling policy and priority while it is running.

Set scheduling:

```c
pthread_setschedparam(tid, policy, &param);
```

Get scheduling:

```c
pthread_getschedparam(tid, &policy, &param);
```

Example:

```c
struct sched_param param;

param.sched_priority = 20;
pthread_setschedparam(tid, SCHED_RR, &param);
```

Meaning:

```text
Set thread tid to Round Robin scheduling with priority 20.
```

Common policies:

```text
SCHED_FIFO
SCHED_RR
SCHED_OTHER
SCHED_SPORADIC
SCHED_NOCHANGE
```

---

## 16. Quick Comparison

| Operation                     | Purpose                            | Important Note                                    |
| ----------------------------- | ---------------------------------- | ------------------------------------------------- |
| `pthread_exit(retval)`        | End calling thread                 | Return value can be collected by join             |
| `return` from thread function | End created thread                 | C library behaves like pthread_exit(return_value) |
| `pthread_join(tid, &retval)`  | Wait for thread and collect result | Thread can be joined only once                    |
| `pthread_detach(tid)`         | Make thread auto-cleaned           | Cannot join or get return value                   |
| `pthread_self()`              | Get current thread ID              | Portable POSIX                                    |
| `pthread_kill(tid, signo)`    | Send signal to thread              | Misleading name; not simple thread kill           |
| `pthread_cancel(tid)`         | Request thread cancellation        | Risky/asynchronous                                |
| `pthread_setname_np()`        | Name thread for debug              | Non-portable                                      |
| `pthread_setschedparam()`     | Change priority/scheduling         | Can use policy + sched_param                      |
| `pthread_getschedparam()`     | Query priority/scheduling          | Reads current settings                            |

---

## 17. Common Quiz Questions

### Q1: How can a thread terminate itself?

```text
By calling pthread_exit() or returning from its thread function.
```

### Q2: What does pthread_join() do?

```text
It waits for a thread to terminate and collects its return value.
```

### Q3: Can a thread be joined more than once?

```text
No.
A thread can be joined only once.
```

### Q4: Is there a parent/child relationship between threads?

```text
No.
Any thread in the same process can join another joinable thread.
```

### Q5: What does pthread_detach() do?

```text
Makes the thread unjoinable and automatically cleaned up when it exits.
```

### Q6: Does pthread_kill() always kill a thread?

```text
No.
It sends a signal to a thread.
```

### Q7: Why is pthread_cancel() risky?

```text
Because it can terminate a thread asynchronously while it owns locks or resources.
```

### Q8: What is the safer alternative to pthread_cancel()?

```text
Use a done flag and let the thread clean itself up.
```

### Q9: What happens if main returns?

```text
The whole process exits.
```

---

## Final Summary

```text
Thread operations let you:
    terminate threads
    wait for threads
    detach threads
    signal threads
    request cancellation
    name threads
    change/query scheduling
```

Best practice:

```text
Prefer controlled thread shutdown:
    signal done
    unblock the thread
    clean up
    return or pthread_exit()
```

Final key sentence:

```text
In QNX/POSIX threading, clean self-termination and pthread_join() are usually safer and clearer than asynchronous cancellation.
```