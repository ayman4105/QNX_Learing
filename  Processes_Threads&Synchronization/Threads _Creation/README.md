# QNX pthread_create() — Quick Study README

## Core Idea

`pthread_create()` is the POSIX function used to create a new thread.

```text
pthread_create()
    |
    v
creates a new thread
    |
    v
new thread starts from your thread function
```

Golden sentence:

```text
pthread_create() starts a new thread at a function you provide, and pthread_attr_t controls how that thread is created.
```

---

## 1. Basic Function Form

```c
int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void *), void *arg);
```

Simple example:

```c
pthread_create(&tid, &attr, thread_func, arg);
```

Meaning:

```text
&tid        -> output place for the new thread ID
&attr       -> thread attributes/settings
thread_func -> function where the thread starts
arg         -> data passed to the thread function
```

---

## 2. Parameter 1: `&tid`

```c
pthread_t tid;
pthread_create(&tid, NULL, worker, NULL);
```

`&tid` is **not** the thread ID you want to assign.

It is a place where `pthread_create()` stores the new thread ID.

```text
Wrong idea:
    I choose the thread ID

Correct idea:
    OS creates the thread ID
    pthread_create() writes it into tid
```

Diagram:

```text
pthread_create()
      |
      v
OS creates new thread
      |
      v
stores new thread ID in tid
```

Important quiz point:

```text
&tid is output storage, not an input ID chosen by the programmer.
```

---

## 3. Parameter 2: `&attr`

`attr` controls how the new thread is created.

It can control things like:

```text
priority
scheduling policy
stack size
guard size
detach state
```

If you do not need custom settings, pass:

```c
NULL
```

Example:

```c
pthread_create(&tid, NULL, worker, NULL);
```

Meaning:

```text
Create the thread using default attributes.
```

If you want custom attributes:

```c
pthread_attr_t attr;

pthread_attr_init(&attr);
/* set custom values here */
pthread_create(&tid, &attr, worker, NULL);
pthread_attr_destroy(&attr);
```

---

## 4. Parameter 3: `thread_func`

The third parameter is the function where the new thread starts running.

The function must look like this:

```c
void *worker(void *arg)
{
    return NULL;
}
```

Then:

```c
pthread_create(&tid, NULL, worker, NULL);
```

Diagram:

```text
main thread
    |
    | pthread_create()
    v
new thread starts at worker()
```

Important quiz point:

```text
The thread does not start from main().
It starts from the function you pass to pthread_create().
```

---

## 5. Parameter 4: `arg`

The fourth parameter is data passed to the thread function.

It is a:

```c
void *
```

So you can pass a pointer to any data.

Example:

```c
int value = 10;

pthread_create(&tid, NULL, worker, &value);
```

Inside the thread:

```c
void *worker(void *arg)
{
    int *ptr = (int *)arg;

    printf("%d\n", *ptr);

    return NULL;
}
```

Important:

```text
The OS/library does not examine this pointer.
It does not validate it.
It does not dereference it.
It passes it to your thread function unchanged.
```

So this statement is wrong:

```text
You cannot pass a pointer.
```

Actually:

```text
arg is designed to pass a pointer.
```

---

## 6. Important Safety Note About `arg`

Do not pass the address of a local variable if that variable may disappear before the thread uses it.

Risky example:

```c
void start_thread(void)
{
    int value = 10;

    pthread_create(&tid, NULL, worker, &value);

    /* value disappears when this function returns */
}
```

Better options:

```text
Use static/global data
Use heap allocation
Join the thread before the variable goes out of scope
Pass a pointer to a struct that remains valid
```

---

## 7. Attribute Flow

When using custom attributes:

```text
declare pthread_attr_t
        |
        v
pthread_attr_init()
        |
        v
pthread_attr_set*()
        |
        v
pthread_create()
        |
        v
pthread_attr_destroy()
```

Example attributes:

```c
pthread_attr_setinheritsched();
pthread_attr_setschedpolicy();
pthread_attr_setschedparam();
pthread_attr_setstacksize();
pthread_attr_setguardsize();
```

---

## 8. Default Attributes

Passing `NULL` as the attribute parameter:

```c
pthread_create(&tid, NULL, worker, NULL);
```

is similar to:

```c
pthread_attr_t attr;

pthread_attr_init(&attr);
pthread_create(&tid, &attr, worker, NULL);
pthread_attr_destroy(&attr);
```

if you do not change anything inside `attr`.

Meaning:

```text
Use default thread creation behavior.
```

---

## 9. Quiz Example

Given:

```c
pthread_create(&tid, &attr, &func, &arg);
```

Correct statements:

```text
&func is the address of your function that you want the thread to execute.

&attr allows you to set thread attributes such as priority and stack size,
but can be NULL if you are okay accepting the default values.
```

Wrong statements:

```text
&tid passes the thread id that the programmer wants to assign to the new thread.
```

Why wrong?

```text
The programmer does not assign the thread ID.
The system creates it and stores it in tid.
```

Wrong statement:

```text
&arg is data passed to the thread function,
but since the upper 16 bits are ignored, you cannot pass a pointer.
```

Why wrong?

```text
arg is a void pointer.
You can pass a pointer.
It is passed unchanged to the thread function.
```

---

## 10. Common Quiz Questions

### Q1: What does `pthread_create()` do?

```text
It creates a new thread.
```

### Q2: Where does the new thread start?

```text
At the thread function passed as the third parameter.
```

### Q3: Does the new thread start from `main()`?

```text
No.
It starts from the function passed to pthread_create().
```

### Q4: What is `&tid` used for?

```text
To store the ID of the newly created thread.
```

### Q5: Does the programmer choose the thread ID?

```text
No.
The OS chooses it.
```

### Q6: What is `attr` used for?

```text
To configure the new thread, such as priority, stack size, and scheduling policy.
```

### Q7: Can `attr` be NULL?

```text
Yes.
NULL means use default attributes.
```

### Q8: Can `arg` be a pointer?

```text
Yes.
arg is a void pointer passed unchanged to the thread function.
```

---

## Final Summary

```text
pthread_create parameters:

1. thread:
    output storage for new thread ID

2. attr:
    thread configuration
    can be NULL for defaults

3. start_routine:
    function where thread starts

4. arg:
    data/pointer passed to thread function
```

Final key sentence:

```text
pthread_create() does not let you choose the thread ID; it gives you the new thread ID, starts the thread at your function, and passes your argument pointer unchanged.
```