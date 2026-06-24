# QNX Atomic Operations — Quick Study README

## Core Idea

An **atomic operation** is a small update that is guaranteed to complete correctly as one indivisible operation.

It stays correct even if there is:

```text
preemption
interrupts
multiple threads
multiple CPU cores / SMP
```

Golden sentence:

```text
Use atomic operations for small independent updates; use mutexes when you need to protect a larger critical section or multiple related data items.
```

---

## 1. Why Atomic Operations Are Needed

A simple operation like:

```c
var3++;
```

looks like one step, but internally it is closer to:

```text
read var3
add 1
write var3
```

So if two threads do it at the same time, one update can be lost.

---

## 2. Lost Update Example

Initial value:

```text
var3 = 5
```

Bad timing:

```text
Thread A reads var3 = 5
Thread B reads var3 = 5

Thread A adds 1 -> 6
Thread B adds 1 -> 6

Thread A writes 6
Thread B writes 6
```

Expected result:

```text
var3 = 7
```

Actual result:

```text
var3 = 6
```

This is called:

```text
lost update
```

---

## 3. Mutex Solution

You can protect the increment using a mutex:

```c
pthread_mutex_lock(&mutex);

var3++;

pthread_mutex_unlock(&mutex);
```

This is correct, but for a very small update it may be heavier than needed.

A mutex may involve:

```text
atomic operation internally
possible blocking
possible kernel call
scheduler involvement
```

---

## 4. Atomic Solution

Instead of using a full mutex for a simple counter update, use an atomic operation.

Example:

```c
atomic_add(&var3, 1);
```

Meaning:

```text
Add 1 to var3 atomically.
No update is lost.
```

---

## 5. atomic_add()

`atomic_add()` updates the value atomically.

```c
atomic_add(&var3, 1);
```

Meaning:

```text
var3 = var3 + 1
```

But safely between multiple threads.

Use when:

```text
you only need to update the value
you do not need the old value
```

---

## 6. atomic_add_value()

`atomic_add_value()` does the operation and returns the original value.

```c
old = atomic_add_value(&var3, 1);
```

Example:

```text
before:
    var3 = 99

operation:
    old = atomic_add_value(&var3, 1)

after:
    old = 99
    var3 = 100
```

Important:

```text
atomic_add_value() returns the old value, not the new value.
```

---

## 7. Example: Print Every 10 Million Updates

Bad code:

```c
var3++;

if ((var3 % 10000000) == 0)
{
    printf("var3 reached %u\n", var3);
}
```

Problem:

```text
Two threads may update var3 at the same time.
The program may pass 10,000,000 but never print.
```

Example:

```text
var3 = 9,999,999

Thread A increments -> 10,000,000
Thread B increments -> 10,000,001

Both test after value became 10,000,001
printf may be skipped
```

---

## 8. Correct Version Using atomic_add_value()

```c
uint32_t old;

old = atomic_add_value(&var3, 1);

if (((old + 1) % 10000000) == 0)
{
    printf("var3 reached %u\n", old + 1);
}
```

Why `old + 1`?

```text
atomic_add_value() returns the value before adding.
So the new value is old + 1.
```

Example:

```text
old = 9,999,999
new value = 10,000,000
printf runs
```

---

## 9. Why This Works

Atomic operations guarantee that each update happens separately.

Example:

```text
var3 = 9,999,999
```

Thread A:

```text
old = 9,999,999
var3 becomes 10,000,000
```

Thread B:

```text
old = 10,000,000
var3 becomes 10,000,001
```

No increment is lost.

---

## 10. QNX Atomic Operations Mentioned

| Operation         | Meaning                         | Similar C Expression |
| ----------------- | ------------------------------- | -------------------- |
| `atomic_add()`    | Add value                       | `x += value`         |
| `atomic_sub()`    | Subtract value                  | `x -= value`         |
| `atomic_set()`    | Set bits using OR               | `x |= mask`          |
| `atomic_clear()`  | Clear bits using AND complement | `x &= ~mask`         |
| `atomic_toggle()` | Toggle bits using XOR           | `x ^= mask`          |

Many operations also have a `_value()` version.

```text
atomic_<operation>_value()
```

This means:

```text
perform the operation
return the original value
```

---

## 11. atomic_sub()

Subtract a value atomically.

```c
atomic_sub(&counter, 1);
```

Meaning:

```text
counter = counter - 1
```

Use for:

```text
reference counts
resource counters
available item counts
```

---

## 12. atomic_set()

Set bits atomically using bitwise OR.

```c
atomic_set(&flags, READY_BIT);
```

Similar to:

```c
flags |= READY_BIT;
```

Use for setting flags:

```text
READY
ERROR
DONE
STARTED
```

---

## 13. atomic_clear()

Clear bits atomically.

```c
atomic_clear(&flags, READY_BIT);
```

Similar to:

```c
flags &= ~READY_BIT;
```

Use when you want to turn off a flag.

---

## 14. atomic_toggle()

Toggle bits atomically using XOR.

```c
atomic_toggle(&flags, LED_BIT);
```

Similar to:

```c
flags ^= LED_BIT;
```

Use for:

```text
toggle flag
toggle bit state
simple debug state
```

---

## 15. When To Use Atomic Operations

Use atomic operations for small simple values:

```text
counters
flags
bit masks
simple state updates
single 32-bit values
```

Good examples:

```c
atomic_add(&counter, 1);
atomic_set(&flags, READY_BIT);
atomic_clear(&flags, ERROR_BIT);
```

---

## 16. When NOT To Use Atomic Operations

Do not use atomics as a replacement for all synchronization.

Use a mutex for:

```text
linked lists
queues
large structs
multiple related variables
buffer pools
complex critical sections
hardware sequences with multiple steps
```

Why?

Because atomics protect one small operation, not a whole logical sequence.

---

## 17. Atomic vs Mutex

| Item                        | Atomic Operation             | Mutex                      |
| --------------------------- | ---------------------------- | -------------------------- |
| Best for                    | Small value update           | Larger critical section    |
| Example                     | counter increment            | queue push/pop             |
| Blocking                    | Usually no                   | Yes, can block             |
| Kernel involvement          | Usually no                   | Possible                   |
| Protects multiple variables | Not suitable                 | Yes                        |
| Complexity                  | Simple for small updates     | Better for complex logic   |
| Portability                 | QNX atomics are QNX-specific | POSIX mutexes are portable |

---

## 18. QNX Atomics vs C11 Atomics

QNX provides convenient atomic functions.

```text
QNX atomic operations:
    simple
    useful for common updates
    QNX-specific
```

C11 also provides atomic functions.

```text
C11 atomics:
    more portable
    more complex
```

Use QNX atomics when:

```text
you are writing QNX-specific code
you need a simple counter or flag update
```

Use C11 atomics when:

```text
you need portability across systems
you can handle the extra complexity
```

---

## 19. Common Mistakes

### Mistake 1: Thinking `x++` is atomic

Wrong assumption:

```text
x++ is one safe operation
```

Reality:

```text
x++ can be read/add/write internally
```

Use:

```c
atomic_add(&x, 1);
```

---

### Mistake 2: Using atomics for complex data structures

Wrong idea:

```text
I can protect a linked list using one atomic update.
```

Better:

```text
Use a mutex for linked list operations.
```

---

### Mistake 3: Forgetting `_value()` returns old value

```c
old = atomic_add_value(&var3, 1);
```

Remember:

```text
old = value before addition
new value = old + 1
```

---

## 20. Common Quiz Questions

### Q1: What does atomic mean?

```text
The operation completes as one indivisible operation.
```

### Q2: Why is `var++` unsafe between threads?

```text
Because it can be implemented as read, add, and write, so updates can overlap.
```

### Q3: What problem does atomic_add solve?

```text
It prevents lost updates for simple increments/additions.
```

### Q4: What does atomic_add_value return?

```text
The original value before the addition.
```

### Q5: When should you use atomic operations?

```text
For small independent updates like counters, flags, and bit masks.
```

### Q6: When should you use a mutex instead?

```text
When protecting a larger critical section, linked list, queue, struct, or multiple related variables.
```

### Q7: What does atomic_set do?

```text
It sets bits using bitwise OR.
```

### Q8: What does atomic_clear do?

```text
It clears bits using bitwise AND with the complement.
```

### Q9: What does atomic_toggle do?

```text
It toggles bits using XOR.
```

### Q10: Are QNX atomic operations portable?

```text
They are QNX-specific. C11 atomics are more portable but more complex.
```

---

## Final Summary

```text
Atomic operations:
    small updates
    guaranteed safe against preemption/interruption/SMP races
    useful for counters and flags
```

Use atomic operations for:

```text
counter++
set flag
clear flag
toggle bit
small state update
```

Use mutexes for:

```text
queues
linked lists
multiple variables
complex critical sections
shared data structures
```

Final key sentence:

```text
Atomic operations are lightweight tools for small independent updates, while mutexes are the right tool for protecting larger shared logic.
```