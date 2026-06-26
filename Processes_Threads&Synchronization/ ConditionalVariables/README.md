# QNX Condition Variables — Quick Study README

## Core Idea

A **condition variable** or **condvar** is used when a thread needs to sleep until some shared condition becomes true.

Golden sentence:

```text
A condvar does not store events; it only wakes waiting threads.
The real condition must be stored in shared state protected by a mutex.
```

Main idea:

```text
mutex:
    protects shared data/state

condvar:
    notifies threads that the state may have changed

shared state:
    stores the real condition
```

---

## 1. Why We Need Condition Variables

Sometimes a thread needs to wait until a variable changes.

Example:

```c
int data_ready = 0;
```

Bad style:

```c
while (data_ready == 0)
{
    /* keep checking */
}
```

This is bad because it wastes CPU.

Better style:

```text
Thread blocks and sleeps.
Another thread wakes it when data becomes ready.
```

This is what a condition variable is used for.

---

## 2. Condvar vs Mutex

| Tool    | Purpose                                   |
| ------- | ----------------------------------------- |
| Mutex   | Protect shared data/state                 |
| Condvar | Notify waiting threads that state changed |

Important:

```text
A mutex does not notify.
A condvar does not protect data.
You usually need both.
```

---

## 3. Basic Shared-State Example

Shared objects:

```c
pthread_mutex_t mutex;
pthread_cond_t cond;

int data_ready = 0;
queue_t queue;
```

Meaning:

```text
mutex:
    protects data_ready and queue

cond:
    wakes waiting thread when data may be ready

data_ready:
    remembers whether data exists

queue:
    shared data structure
```

---

## 4. Main Condition Variable APIs

| API                        | Purpose                       |
| -------------------------- | ----------------------------- |
| `pthread_cond_init()`      | Initialize condition variable |
| `pthread_cond_destroy()`   | Destroy condition variable    |
| `pthread_cond_wait()`      | Wait on condition variable    |
| `pthread_cond_signal()`    | Wake one waiting thread       |
| `pthread_cond_broadcast()` | Wake all waiting threads      |

---

## 5. pthread_cond_init()

Used to initialize a condition variable.

```c
pthread_cond_t cond;

pthread_cond_init(&cond, NULL);
```

`NULL` means:

```text
Use default condition variable attributes.
```

---

## 6. pthread_cond_destroy()

Used to destroy the condition variable when finished.

```c
pthread_cond_destroy(&cond);
```

Important:

```text
Do not destroy a condvar while threads may still be waiting on it.
```

Safe cleanup idea:

```text
request threads to stop
wake waiting threads if needed
join threads
destroy condvar
destroy mutex
```

---

## 7. Static Initialization

Like mutexes, condition variables can be statically initialized.

```c
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
```

Use this when:

```text
the objects are global/static
default attributes are enough
you do not need a custom init function
```

---

## 8. pthread_cond_wait()

This is the most important function.

```c
pthread_cond_wait(&cond, &mutex);
```

It does three key operations:

```text
1. unlock mutex
2. block waiting on condvar
3. re-lock mutex before returning
```

Diagram:

```text
thread calls pthread_cond_wait()
        |
        v
mutex unlocked
        |
        v
thread blocks on condvar
        |
        v
signal/broadcast happens
        |
        v
thread becomes runnable
        |
        v
thread locks mutex again
        |
        v
pthread_cond_wait() returns
```

Key sentence:

```text
pthread_cond_wait() waits with the mutex unlocked, but returns with the mutex locked.
```

---

## 9. Why pthread_cond_wait() Unlocks the Mutex

If the waiting thread kept the mutex locked while waiting, the producer could not update the condition.

Bad situation:

```text
consumer locks mutex
consumer sees data_ready == 0
consumer waits but keeps mutex locked

producer needs mutex
producer cannot set data_ready = 1
consumer waits forever
```

So `pthread_cond_wait()` unlocks the mutex while waiting.

---

## 10. Why pthread_cond_wait() Re-locks the Mutex

After wake-up, the thread must check shared state again.

The shared state is protected by the mutex.

So before returning, `pthread_cond_wait()` re-locks the mutex.

```text
wake up
lock mutex again
return to user code
check condition safely
```

---

## 11. Correct Waiting Pattern

Always use a loop.

```c
pthread_mutex_lock(&mutex);

while (data_ready == 0)
{
    pthread_cond_wait(&cond, &mutex);
}

/* data_ready is true here */
/* use shared data while mutex is locked */

pthread_mutex_unlock(&mutex);
```

Important:

```text
Use while, not if.
```

Reason:

```text
Wake-up does not guarantee the condition is still true.
Always re-check the condition.
```

---

## 12. Correct Signaling Pattern

The signaling thread should change the shared state first, then signal.

```c
pthread_mutex_lock(&mutex);

/* modify shared state */
data_ready = 1;

pthread_cond_signal(&cond);

pthread_mutex_unlock(&mutex);
```

Pattern:

```text
lock mutex
change shared state
signal/broadcast
unlock mutex
```

---

## 13. Lost Signal Problem

Condition variables do not remember notifications.

If this happens:

```c
pthread_cond_signal(&cond);
```

and no thread is waiting:

```text
signal is lost
```

Same for broadcast:

```text
broadcast with no waiter = no effect
```

Important:

```text
condvar signal has no memory.
```

---

## 14. Why We Need data_ready / state

Because signal can be lost, the real condition must be stored in a shared variable.

Example:

```c
int data_ready = 0;
```

or:

```c
int state = 0;
```

Difference:

```text
condvar:
    wakes threads if they are currently waiting

state variable:
    remembers the real condition
```

Short rule:

```text
condvar = notification
state = truth
mutex = protection
```

---

## 15. Signal Can Be Lost but State Saves Us

Scenario:

```text
Hardware thread is busy writing old data to hardware.
It is not waiting on condvar now.
```

Producer does:

```c
pthread_mutex_lock(&mutex);

add_data_to_queue();
data_ready = 1;
pthread_cond_signal(&cond);

pthread_mutex_unlock(&mutex);
```

The signal may be lost because nobody is waiting.

But:

```text
data_ready == 1
```

is still stored.

Later, hardware thread checks:

```c
while (data_ready == 0)
{
    pthread_cond_wait(&cond, &mutex);
}
```

It sees:

```text
data_ready == 1
```

So it does not wait.

---

## 16. Condvar Is Not a Semaphore

| Tool      | If notification happens with no waiter |
| --------- | -------------------------------------- |
| Semaphore | Counter increases                      |
| Condvar   | Notification is lost                   |

So:

```text
Semaphore remembers count.
Condvar does not remember signals.
```

That is why condvars must be used with shared state.

---

## 17. pthread_cond_signal()

`pthread_cond_signal()` wakes one waiting thread.

```c
pthread_cond_signal(&cond);
```

If multiple threads are waiting, QNX wakes:

```text
highest-priority waiting thread
```

If multiple waiters have the same highest priority:

```text
longest-waiting thread
```

Short rule:

```text
signal wakes one waiter:
    highest priority
    longest waiting if tied
```

---

## 18. pthread_cond_broadcast()

`pthread_cond_broadcast()` wakes all waiting threads.

```c
pthread_cond_broadcast(&cond);
```

But they do not all return from `pthread_cond_wait()` at the same time.

Why?

```text
pthread_cond_wait() returns with the mutex locked.
Only one thread can hold the mutex at a time.
```

Flow:

```text
broadcast
    |
    v
all waiters become runnable
    |
    v
all try to lock mutex
    |
    v
one gets mutex
others wait
    |
    v
they return one by one
```

---

## 19. Signal vs Broadcast

| Function                   | Wakes       | Use When                           |
| -------------------------- | ----------- | ---------------------------------- |
| `pthread_cond_signal()`    | One waiter  | Only one thread needs to work      |
| `pthread_cond_broadcast()` | All waiters | All threads need to re-check state |

Use `signal` when:

```text
one item is available
one worker is enough
all workers do the same job and any one can handle it
```

Use `broadcast` when:

```text
all threads must wake
shutdown was requested
you do not know which thread condition became true
different threads care about different state values
```

---

## 20. Queue Example: Data Provider + Hardware Handler

Threads:

```text
Data Provider Thread:
    gets data
    pushes data into queue
    signals condvar

Hardware Handling Thread:
    waits for data
    pops data from queue
    writes data to hardware
```

Shared objects:

```text
queue
data_ready
```

Protection/notification:

```text
mutex protects queue and data_ready
condvar notifies hardware thread when data arrives
```

---

## 21. Why Not Write to Hardware Directly?

The data provider should not always write to hardware directly because hardware writes may be slow.

```text
Data Provider:
    should collect more data
    may communicate with clients
    may return to a blocking loop

Hardware Handler:
    handles slow hardware writes
```

Design:

```text
Data Provider -> Queue -> Hardware Handler -> Hardware
```

---

## 22. Hardware Handler Pattern

```c
pthread_mutex_lock(&mutex);

while (data_ready == 0)
{
    pthread_cond_wait(&cond, &mutex);
}

/* get data from queue while mutex is locked */

pthread_mutex_unlock(&mutex);

/* write data to hardware outside mutex */
```

Important:

```text
Pop from queue under mutex.
Write to hardware outside mutex.
```

Reason:

```text
Queue is shared data.
Hardware write may be slow.
Do not keep mutex locked during slow work.
```

---

## 23. Data Provider Pattern

```c
pthread_mutex_lock(&mutex);

/* add data to queue */
data_ready = 1;

pthread_cond_signal(&cond);

pthread_mutex_unlock(&mutex);
```

Meaning:

```text
lock mutex
update shared queue/state
signal waiting hardware thread
unlock mutex
```

---

## 24. Queue Flow

```text
Hardware Thread:
    lock mutex
    data_ready == 0
    pthread_cond_wait()
        |
        v
    mutex unlocked and thread blocked

Data Provider Thread:
    lock mutex
    add data to queue
    data_ready = 1
    signal condvar
    unlock mutex

Hardware Thread:
    wakes
    locks mutex
    returns from pthread_cond_wait()
    sees data_ready == 1
    pops data
    unlocks mutex
    writes data to hardware
```

---

## 25. Producer / Consumer Example

Shared variables:

```c
static int state = 0;
static int product = 0;
```

Meaning:

```text
state = 0:
    no product available
    producer can produce
    consumer should wait

state = 1:
    product available
    consumer can consume
    producer should wait
```

This is like a one-item buffer.

---

## 26. Producer Logic

Producer waits while the buffer is full.

```c
pthread_mutex_lock(&mutex);

while (state == 1)
{
    pthread_cond_wait(&cond, &mutex);
}

/* buffer is empty */
product++;
state = 1;

pthread_cond_signal(&cond);

pthread_mutex_unlock(&mutex);
```

Meaning:

```text
if state == 1:
    product already exists
    producer waits

if state == 0:
    producer creates product
    state becomes 1
    consumer is notified
```

---

## 27. Consumer Logic

Consumer waits while the buffer is empty.

```c
pthread_mutex_lock(&mutex);

while (state == 0)
{
    pthread_cond_wait(&cond, &mutex);
}

/* product is available */
printf("Consumed product: %d\n", product);
state = 0;

pthread_cond_signal(&cond);

pthread_mutex_unlock(&mutex);
```

Meaning:

```text
if state == 0:
    no product
    consumer waits

if state == 1:
    consumer consumes product
    state becomes 0
    producer is notified
```

---

## 28. Producer / Consumer Flow

Initial state:

```text
state = 0
product = 0
```

Flow:

```text
Consumer:
    sees state == 0
    waits

Producer:
    sees state == 0
    produces product
    state = 1
    signals consumer

Consumer:
    wakes
    sees state == 1
    consumes product
    state = 0
    signals producer

Producer:
    wakes
    sees state == 0
    produces again
```

State alternates:

```text
0 -> 1 -> 0 -> 1 -> 0
```

---

## 29. Full Producer / Consumer Pseudo-Code

```c
#include <pthread.h>
#include <stdio.h>

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

static int state = 0;
static int product = 0;

void *produce(void *arg)
{
    while (1)
    {
        pthread_mutex_lock(&mutex);                  /* Protect state and product */

        while (state == 1)                           /* Wait while product exists */
        {
            pthread_cond_wait(&cond, &mutex);        /* Unlock mutex and block */
        }

        product++;                                   /* Produce one product */
        state = 1;                                   /* Mark product available */

        pthread_cond_signal(&cond);                  /* Notify consumer */

        pthread_mutex_unlock(&mutex);                /* Release shared state */

        /* Producer does other work here */
    }

    return NULL;
}

void consume(void)
{
    while (1)
    {
        pthread_mutex_lock(&mutex);                  /* Protect state and product */

        while (state == 0)                           /* Wait while no product exists */
        {
            pthread_cond_wait(&cond, &mutex);        /* Unlock mutex and block */
        }

        printf("Consumed product: %d\n", product);   /* Consume the product */
        state = 0;                                   /* Mark buffer empty */

        pthread_cond_signal(&cond);                  /* Notify producer */

        pthread_mutex_unlock(&mutex);                /* Release shared state */

        /* Consumer does other work here */
    }
}
```

---

## 30. Multiple Condvars Design

You can use different condvars for different state changes.

Example:

```text
variables: W, X, Y, Z
```

Threads interested in:

```text
Group 1:
    W or X changed

Group 2:
    Y or Z changed
```

Design:

```text
cond_wx:
    signal when W or X changes

cond_yz:
    signal when Y or Z changes
```

This avoids waking unrelated waiters.

---

## 31. Important Design Rule

```text
mutex is associated with shared variables
condvar is associated with notification of changes
```

Use this structure:

```text
shared state:
    protected by mutex

waiters:
    wait on condvar while condition is false

notifiers:
    change shared state
    signal or broadcast condvar
```

---

## 32. Common Mistakes

### Mistake 1: Waiting without checking condition

Wrong:

```c
pthread_cond_wait(&cond, &mutex);
```

Correct:

```c
while (condition_is_false)
{
    pthread_cond_wait(&cond, &mutex);
}
```

---

### Mistake 2: Using if instead of while

Wrong:

```c
if (condition_is_false)
{
    pthread_cond_wait(&cond, &mutex);
}
```

Correct:

```c
while (condition_is_false)
{
    pthread_cond_wait(&cond, &mutex);
}
```

Reason:

```text
After wake-up, condition must be checked again.
```

---

### Mistake 3: Signaling without changing state

Wrong:

```c
pthread_cond_signal(&cond);
```

Correct:

```c
condition_is_true = 1;
pthread_cond_signal(&cond);
```

---

### Mistake 4: Accessing state without mutex

Wrong:

```c
if (data_ready)
{
    use_data();
}
```

Correct:

```c
pthread_mutex_lock(&mutex);

if (data_ready)
{
    use_data();
}

pthread_mutex_unlock(&mutex);
```

---

### Mistake 5: Thinking signal is saved

Wrong thinking:

```text
I signaled once, so a future waiter will wake.
```

Correct thinking:

```text
If nobody was waiting, the signal was lost.
The state variable stores the real condition.
```

---

## 33. Quick Comparison

| Concept                    | Meaning                              |
| -------------------------- | ------------------------------------ |
| Condvar                    | Notification mechanism               |
| Mutex                      | Protects shared state                |
| State variable             | Stores the real condition            |
| `pthread_cond_wait()`      | Unlocks mutex, waits, re-locks mutex |
| `pthread_cond_signal()`    | Wakes one waiter                     |
| `pthread_cond_broadcast()` | Wakes all waiters                    |
| Lost signal                | Signal with no waiter has no effect  |
| `while` loop               | Re-checks condition after wake-up    |

---

## 34. Common Quiz Questions

### Q1: What is a condition variable used for?

```text
To let a thread wait until a condition becomes true.
```

### Q2: Does a condvar protect shared data?

```text
No.
A mutex protects shared data.
```

### Q3: Why does pthread_cond_wait() take a mutex?

```text
Because the condition is based on shared state that must be protected.
```

### Q4: What does pthread_cond_wait() do?

```text
It unlocks the mutex, waits on the condvar, then re-locks the mutex before returning.
```

### Q5: What happens if pthread_cond_signal() is called with no waiter?

```text
The signal is lost and has no effect.
```

### Q6: What is the difference between signal and broadcast?

```text
signal wakes one waiter.
broadcast wakes all waiters.
```

### Q7: Why do we need a state variable like data_ready?

```text
Because the condvar does not remember notifications.
The state variable stores the real condition.
```

### Q8: Why use while around pthread_cond_wait()?

```text
Because after wake-up, the thread must re-check whether the condition is actually true.
```

### Q9: In QNX, which waiter does signal wake?

```text
The highest-priority waiting thread.
If tied, the longest-waiting thread.
```

### Q10: What should be protected by the mutex in producer/consumer?

```text
The shared state and shared product/queue.
```

---

## Final Summary

```text
Condition variable:
    wakes waiting threads

Mutex:
    protects the shared condition/state

State variable:
    remembers whether work is really available
```

Golden pattern:

```c
pthread_mutex_lock(&mutex);

while (condition_is_false)
{
    pthread_cond_wait(&cond, &mutex);
}

/* condition is true; use shared data */

pthread_mutex_unlock(&mutex);
```

Notifier pattern:

```c
pthread_mutex_lock(&mutex);

/* change shared state */
condition_is_true = 1;

pthread_cond_signal(&cond);

pthread_mutex_unlock(&mutex);
```

Final key sentence:

```text
A condvar is only a wake-up mechanism; the condition itself must be stored in shared state and protected by a mutex.
```