# QNX Thread APIs — Quick Study README

## Core Idea

QNX provides more than one API level for creating threads and using synchronization.

```text
Application code
      |
      v
Portable API: POSIX pthreads / C11 threads
      |
      v
QNX low-level kernel calls
      |
      v
Kernel
```

Key sentence:

```text
Use portable POSIX pthread APIs instead of direct QNX kernel thread calls.
```

---

## 1. The Three Thread API Levels

QNX supports at least three thread API styles:

```text
1. QNX kernel functions
2. POSIX pthread functions
3. C11 threading functions
```

Examples:

```text
QNX kernel:
    ThreadCreate()
    SyncMutexLock()

POSIX:
    pthread_create()
    pthread_mutex_lock()

C11:
    thrd_create()
    mtx_lock()
```

---

## 2. QNX Kernel Thread Functions

These are low-level QNX-specific functions.

Examples:

```c
ThreadCreate();
SyncMutexLock();
```

They are close to the kernel implementation.

Important:

```text
They are not usually intended for direct application use.
```

Why?

Because they may not perform the complete high-level setup needed by normal application code.

Example:

```text
ThreadCreate() creates a kernel thread,
but it does not necessarily handle all POSIX-level setup by itself.
```

---

## 3. POSIX pthread APIs

These are the recommended APIs in this section.

Examples:

```c
pthread_create();
pthread_join();
pthread_mutex_lock();
pthread_mutex_unlock();
pthread_cond_wait();
pthread_cond_signal();
```

They are portable and familiar to Unix/Linux developers.

```text
pthread_create()
      |
      v
prepares POSIX thread setup
      |
      v
calls QNX ThreadCreate() underneath
```

Use POSIX pthreads because they are:

```text
portable
standard
easier to read
easier to maintain
recommended by QNX for normal use
```

---

## 4. C11 Threading APIs

C11 also has thread APIs.

Examples:

```c
thrd_create();
mtx_lock();
```

They are standard C APIs, but this course focuses mainly on POSIX pthread APIs.

```text
C11 APIs are portable too,
but POSIX pthreads are more common in QNX/Linux-style systems.
```

---

## 5. Why Not Use ThreadCreate() Directly?

The script gives an important example:

```text
pthread_create()
    allocates a local stack
    prepares thread information
    then calls ThreadCreate()
```

So:

```text
pthread_create() = complete portable thread creation API
ThreadCreate()   = lower-level kernel mechanism
```

Diagram:

```text
Your code
   |
   | pthread_create()
   v
POSIX library
   |
   +-- allocate stack
   +-- prepare attributes
   +-- handle POSIX behavior
   |
   v
ThreadCreate()
   |
   v
Kernel creates thread
```

---

## 6. Same Idea for Mutexes

Instead of using:

```c
SyncMutexLock();
```

Use:

```c
pthread_mutex_lock();
```

Because the POSIX function gives you a standard and portable interface.

Diagram:

```text
pthread_mutex_lock()
        |
        v
POSIX wrapper
        |
        v
SyncMutexLock()
        |
        v
QNX kernel sync object
```

---

## 7. Recommended Rule

```text
Use POSIX pthread APIs for normal application/thread programming.
```

Use QNX low-level APIs only when:

```text
you are writing very low-level QNX-specific code
you know exactly why the POSIX API is not enough
the documentation explicitly requires it
```

---

## 8. Common Quiz Questions

### Q1: What are the three thread API families in QNX?

```text
QNX kernel functions
POSIX pthread functions
C11 threading functions
```

### Q2: Which API does the course focus on?

```text
POSIX pthread functions
```

### Q3: Why are POSIX APIs recommended?

```text
They are portable, standard, easier to read, and built on top of QNX kernel calls.
```

### Q4: What does pthread_create() do before calling ThreadCreate()?

```text
It prepares the thread setup, such as allocating a stack and passing stack information to ThreadCreate().
```

### Q5: Should normal applications call ThreadCreate() directly?

```text
Usually no.
Use pthread_create() instead.
```

---

## 9. Final Comparison

| Feature                     | QNX Kernel API              | POSIX pthread API              | C11 Thread API          |
| --------------------------- | --------------------------- | ------------------------------ | ----------------------- |
| Example create function     | `ThreadCreate()`            | `pthread_create()`             | `thrd_create()`         |
| Example mutex lock          | `SyncMutexLock()`           | `pthread_mutex_lock()`         | `mtx_lock()`            |
| Portability                 | QNX-specific                | POSIX portable                 | C11 portable            |
| Level                       | Low-level                   | High-level wrapper             | High-level standard C   |
| Recommended for this course | No                          | Yes                            | Not the main focus      |
| Ease of use                 | Harder                      | Easier/common                  | Moderate                |
| Completeness                | Kernel-side building block  | Complete POSIX behavior        | C11 standard behavior   |
| Best use case               | Low-level QNX-specific work | Normal QNX application threads | C11-style portable code |

---

## Final Summary

```text
QNX kernel calls are the low-level implementation.
POSIX pthread APIs are portable wrappers built on top of them.
C11 thread APIs also exist, but this course focuses on POSIX.
```

Final key sentence:

```text
In QNX, write normal thread code using POSIX pthread functions, not direct kernel thread calls.
```