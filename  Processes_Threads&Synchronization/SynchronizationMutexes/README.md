# QNX Mutexes — Quick Study README

## Core Idea

`mutex` means:

```text
Mutual Exclusion
```

A mutex is used when only one thread should access shared data or a shared resource at a time.

Golden sentence:

```text
A mutex protects data only if every thread follows the rule: lock before access, unlock after access.
```

---

## 1. Why We Need Mutexes

Threads inside the same process share resources such as:

```text
global variables
linked lists
arrays
buffers
file descriptors
channels
hardware registers
hardware buffers
```

If two threads access the same data at the same time, data corruption can happen.

```text
Thread A writes shared data
Thread B writes same shared data
        |
        v
race condition
```

---

## 2. Critical Section

A critical section is code that accesses shared data or shared hardware.

Examples:

```c
counter++;
add_node_to_linked_list();
write_to_gpio_register();
```

The rule:

```text
Before critical section:
    lock mutex

After critical section:
    unlock mutex
```

Pattern:

```c
pthread_mutex_lock(&mutex);

/* access shared data here */

pthread_mutex_unlock(&mutex);
```

---

## 3. Main Mutex APIs

| API                       | Purpose                                         |
| ------------------------- | ----------------------------------------------- |
| `pthread_mutex_init()`    | Initialize mutex before use                     |
| `pthread_mutex_destroy()` | Destroy mutex when finished                     |
| `pthread_mutex_lock()`    | Lock mutex before accessing shared data         |
| `pthread_mutex_unlock()`  | Unlock mutex after finishing shared data access |

---

## 4. pthread_mutex_init()

Used to initialize a mutex.

```c
pthread_mutex_t my_mutex;

pthread_mutex_init(&my_mutex, NULL);
```

`NULL` means:

```text
Use default mutex attributes.
```

This is the common case for a mutex shared between threads in the same process.

---

## 5. pthread_mutex_destroy()

Used to destroy a mutex when you are done with it.

```c
pthread_mutex_destroy(&my_mutex);
```

Important:

```text
Do not destroy a mutex while another thread may be using it.
```

In QNX, mutexes are cleaned when the process exits, but clean code should still destroy mutexes during normal cleanup.

---

## 6. pthread_mutex_lock()

`pthread_mutex_lock()` tries to lock the mutex.

```c
pthread_mutex_lock(&my_mutex);
```

Cases:

```text
Case 1:
    mutex is unlocked
    calling thread locks it and continues

Case 2:
    mutex is locked by another thread
    calling thread blocks

Case 3:
    same thread tries to lock the same default mutex again
    QNX returns an error such as EDEADLOCK/EDEADLK
```

---

## 7. pthread_mutex_unlock()

`pthread_mutex_unlock()` releases the mutex.

```c
pthread_mutex_unlock(&my_mutex);
```

Important:

```text
Only the thread that locked the mutex should unlock it.
```

This is called mutex ownership.

```text
thread that locks mutex = owner
only owner can unlock mutex
```

---

## 8. Who Gets the Mutex Next?

If multiple threads are blocked waiting for a mutex, QNX chooses:

```text
highest-priority waiting thread
```

If more than one waiting thread has the same highest priority:

```text
longest-waiting thread wins
```

Short rule:

```text
highest priority, longest waiting
```

Example:

```text
Thread A priority 10 waiting 5 sec
Thread B priority 20 waiting 1 sec
Thread C priority 20 waiting 3 sec
```

Result:

```text
Thread C gets the mutex.
```

---

## 9. Simple Mutex Example

```c
#include <pthread.h>
#include <stdio.h>

static pthread_mutex_t counter_mutex = PTHREAD_MUTEX_INITIALIZER;
static int counter = 0;

void *worker(void *arg)
{
    pthread_mutex_lock(&counter_mutex);      /* Lock before accessing shared counter */

    counter++;                               /* Safely update shared data */

    pthread_mutex_unlock(&counter_mutex);    /* Unlock after finishing */

    return NULL;                             /* End this thread */
}
```

---

## 10. buf_alloc() Problem Without Mutex

Imagine a simple buffer allocator with a free list:

```text
free_list
   |
   v
[ size 64 ] -> [ size 128 ] -> [ size 256 ] -> NULL
```

A function `buf_alloc(128)` searches the list, removes a matching block, and returns it.

Bad timing in a multithreaded program:

```text
Thread A finds block size 128
Thread B also finds same block size 128
Thread A removes it
Thread B also removes it
Thread A returns pointer
Thread B returns same pointer
```

Result:

```text
both threads write to the same buffer
data corruption
```

---

## 11. Fixing buf_alloc() With Mutex

Protect the whole free list operation.

```c
static pthread_mutex_t alloc_mutex = PTHREAD_MUTEX_INITIALIZER;

void *buf_alloc(size_t bytes)
{
    struct block *p = free_list;
    void *result = NULL;

    pthread_mutex_lock(&alloc_mutex);        /* Protect free_list access */

    while (p != NULL)
    {
        if (p->size == bytes)
        {
            remove_from_free_list(p);        /* Update shared list safely */
            result = p->buffer;              /* Save result */
            break;
        }

        p = p->next;                         /* Move to next block */
    }

    pthread_mutex_unlock(&alloc_mutex);      /* Unlock on the only exit path */

    return result;
}
```

Key rule:

```text
Every path after pthread_mutex_lock() must reach pthread_mutex_unlock().
```

---

## 12. Avoid Returning Before Unlock

Wrong:

```c
pthread_mutex_lock(&mutex);

if (error)
{
    return -1;       /* Wrong: mutex is still locked */
}

pthread_mutex_unlock(&mutex);
```

Correct:

```c
pthread_mutex_lock(&mutex);

if (error)
{
    pthread_mutex_unlock(&mutex);
    return -1;
}

pthread_mutex_unlock(&mutex);
return 0;
```

Better style:

```text
Use one return path after unlocking when possible.
```

---

## 13. Keep Lock Time Short

Bad:

```c
pthread_mutex_lock(&mutex);

update_shared_data();
do_heavy_calculation();
write_file();
send_network_packet();

pthread_mutex_unlock(&mutex);
```

Better:

```c
pthread_mutex_lock(&mutex);

copy_shared_data_to_local();

pthread_mutex_unlock(&mutex);

do_heavy_calculation();
write_file();
send_network_packet();
```

Rule:

```text
Lock only around shared data access.
Do long work outside the mutex.
```

---

## 14. Mutex Initialization Methods

### Method 1: Explicit initialization

```c
pthread_mutex_t mutex;

pthread_mutex_init(&mutex, NULL);
```

Use this when:

```text
you have a clear init phase
you need custom attributes
you may need process-shared or robust mutexes
```

### Method 2: Static initialization

```c
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
```

Use this when:

```text
mutex is global/static
default attributes are enough
you do not want a separate init function
```

---

## 15. Can pthread_mutex_init() Fail?

For simple default mutexes in QNX, initialization usually only sets local data.

```text
default mutex:
    local initialization
    usually no extra kernel allocation
```

Special mutexes may require kernel resources and can fail.

Examples:

```text
process-shared mutex
robust mutex
special mutex attributes
```

For special mutexes, check the return value:

```c
int ret = pthread_mutex_init(&mutex, &attr);

if (ret != 0)
{
    /* Handle error */
}
```

---

## 16. Process-Shared Mutex

A normal mutex is private to one process.

For shared memory between processes, the mutex itself must live inside shared memory.

```text
Process A ----\
               \
                +--> Shared Memory
               /        |
Process B ----/         +-- pthread_mutex_t mutex
                        +-- shared data
```

You must use:

```text
PTHREAD_PROCESS_SHARED
```

---

## 17. Process-Shared Mutex Example

```c
#include <pthread.h>

typedef struct
{
    pthread_mutex_t mutex;   /* Mutex shared between processes */
    int counter;             /* Shared data protected by mutex */
} shared_data_t;
```

Initialization by the creator process:

```c
pthread_mutexattr_t attr;

pthread_mutexattr_init(&attr);                                  /* Initialize attributes */
pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);    /* Make mutex process-shared */
pthread_mutex_init(&shared->mutex, &attr);                       /* Init mutex in shared memory */
pthread_mutexattr_destroy(&attr);                                /* Destroy attributes object */
```

Usage by all processes:

```c
pthread_mutex_lock(&shared->mutex);      /* Lock shared mutex */

shared->counter++;                       /* Modify shared data */

pthread_mutex_unlock(&shared->mutex);    /* Unlock shared mutex */
```

Important:

```text
Initialize the shared mutex once.
Other processes should only open/map and use it.
```

---

## 18. Does a Mutex Really Protect Data?

A mutex does not protect data like process memory protection.

Process memory protection:

```text
OS prevents Process B from touching Process A memory.
```

Mutex protection:

```text
OS does not know which data the mutex protects.
Programmers must follow the rule.
```

Correct:

```c
pthread_mutex_lock(&data_mutex);

shared_counter++;

pthread_mutex_unlock(&data_mutex);
```

Wrong:

```c
shared_counter++;     /* Direct access without lock */
```

Key point:

```text
A mutex protects data only if every access to that data uses the mutex.
```

---

## 19. Good Design Practice

Hide shared data inside a module.

Better:

```c
static pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
static queue_t shared_queue;

void queue_add_safe(item_t item)
{
    pthread_mutex_lock(&queue_mutex);

    queue_push(&shared_queue, item);

    pthread_mutex_unlock(&queue_mutex);
}
```

This prevents other code from touching `shared_queue` directly.

---

## 20. Deadlock

Deadlock happens when threads wait for each other forever.

Classic example:

```text
Thread 1 owns mutex A and waits for mutex B
Thread 2 owns mutex B and waits for mutex A
```

Diagram:

```text
Thread 1 owns A ---- waits for B
        ^                  |
        |                  v
Thread 2 waits for A ---- owns B
```

This is called:

```text
circular wait
```

QNX does not generally solve this deadlock automatically.

---

## 21. Locking Order

Solve deadlock at design level using a fixed locking order.

Example rule:

```text
Always lock A before B.
```

Correct:

```c
pthread_mutex_lock(&mutex_a);
pthread_mutex_lock(&mutex_b);

/* use data A and B */

pthread_mutex_unlock(&mutex_b);
pthread_mutex_unlock(&mutex_a);
```

Wrong if rule is A then B:

```c
pthread_mutex_lock(&mutex_b);
pthread_mutex_lock(&mutex_a);
```

Golden rule:

```text
If code may lock multiple mutexes, all threads must follow the same locking order.
```

---

## 22. Priority Inversion

Priority inversion can happen with mutexes.

Threads:

```text
Thread A = low priority
Thread B = medium priority
Thread C = high priority
```

Scenario:

```text
Thread A locks mutex M
Thread B preempts A
Thread C preempts B
Thread C tries to lock mutex M
Thread C blocks because A owns M
A cannot run because B is running
```

Problem:

```text
High-priority Thread C is indirectly blocked by medium-priority Thread B.
```

This is priority inversion.

---

## 23. Priority Inheritance

QNX default mutexes use priority inheritance.

If high-priority Thread C waits on a mutex owned by low-priority Thread A:

```text
QNX boosts Thread A to Thread C priority temporarily.
```

Flow:

```text
Thread C waits for mutex M
        |
        v
QNX finds owner = Thread A
        |
        v
QNX boosts A priority
        |
        v
A runs and unlocks M
        |
        v
A returns to original priority
        |
        v
C locks M and continues
```

Key point:

```text
Priority inheritance prevents high-priority work from being delayed by unrelated medium-priority work.
```

---

## 24. Mutex Performance: Fast Path vs Kernel Path

### Fast path

If the mutex is free:

```text
pthread_mutex_lock()
    |
    v
local atomic operation
    |
    v
success
```

No blocking, no kernel call.

### Kernel path

If the mutex is busy:

```text
pthread_mutex_lock()
    |
    v
thread must block
    |
    v
kernel involvement
```

Kernel path may involve:

```text
system call
scheduler
thread state change
context switch
wait queue
wake-up
priority handling
```

---

## 25. Unlock Performance

Fast unlock:

```text
unlock mutex
    |
    v
no waiters
    |
    v
local unlock
```

Kernel unlock:

```text
unlock mutex
    |
    v
waiting threads exist
    |
    v
kernel wakes highest-priority longest-waiting thread
```

---

## 26. Why Short Lock Time Matters

Short lock time increases the chance of fast-path locking.

Long lock time causes:

```text
more blocking
more kernel involvement
less parallelism
more priority inheritance events
more delay for high-priority threads
```

Rule:

```text
Keep mutex lock duration short.
```

---

## 27. Quick Comparison

| Topic                | Key Idea                           |
| -------------------- | ---------------------------------- |
| Mutex                | One thread at a time               |
| Critical section     | Code that touches shared data      |
| Lock                 | Enter protected region             |
| Unlock               | Leave protected region             |
| Ownership            | Only the locker should unlock      |
| Waiter selection     | Highest priority, longest waiting  |
| Static init          | `PTHREAD_MUTEX_INITIALIZER`        |
| Explicit init        | `pthread_mutex_init()`             |
| Process-shared mutex | Mutex lives in shared memory       |
| Deadlock             | Threads wait on each other forever |
| Locking order        | Design rule to prevent deadlock    |
| Priority inversion   | High priority indirectly blocked   |
| Priority inheritance | Owner boosted temporarily          |
| Fast path            | Local atomic operation             |
| Kernel path          | Blocking/wakeup needs kernel       |

---

## 28. Common Quiz Questions

### Q1: What does mutex mean?

```text
Mutual Exclusion.
```

### Q2: What is a mutex used for?

```text
To allow only one thread at a time to access shared data or a critical resource.
```

### Q3: What thread can unlock a mutex?

```text
The thread that locked it.
```

### Q4: What happens if a thread tries to lock a mutex already locked by another thread?

```text
It blocks until the mutex becomes available.
```

### Q5: What happens if multiple threads wait for the same mutex in QNX?

```text
The highest-priority waiting thread is selected.
If tied, the longest-waiting thread is selected.
```

### Q6: Why must every return path unlock the mutex?

```text
Because returning while the mutex is locked can block other threads forever.
```

### Q7: What is the difference between `pthread_mutex_init()` and `PTHREAD_MUTEX_INITIALIZER`?

```text
pthread_mutex_init() is explicit runtime initialization.
PTHREAD_MUTEX_INITIALIZER is static/default initialization for global/static mutexes.
```

### Q8: Where must a process-shared mutex live?

```text
Inside shared memory.
```

### Q9: Does a mutex protect data automatically?

```text
No. The programmer must ensure every access uses the mutex.
```

### Q10: What is deadlock?

```text
A situation where threads wait on each other forever.
```

### Q11: How can deadlock be prevented?

```text
Use a fixed locking order.
```

### Q12: What is priority inversion?

```text
A high-priority thread is blocked because a low-priority thread owns a mutex, while medium-priority work prevents the low-priority owner from running.
```

### Q13: What is priority inheritance?

```text
QNX temporarily boosts the mutex owner to the priority of the highest-priority waiter.
```

### Q14: Why should mutex hold time be short?

```text
To reduce blocking, kernel calls, priority inheritance, and loss of parallelism.
```

---

## Final Summary

```text
Mutex = Mutual Exclusion

Use it to protect shared data:
    lock
    access data
    unlock

Important rules:
    initialize before use
    only owner unlocks
    unlock on every path
    keep lock time short
    use locking order for multiple mutexes
    use process-shared attribute for inter-process mutexes
```

Final key sentence:

```text
Mutexes are powerful but depend on correct design: every shared access must follow the locking rule, and every multi-mutex path must follow the same locking order.
```