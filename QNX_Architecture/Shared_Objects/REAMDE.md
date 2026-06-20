# QNX Shared Objects — Quick Study README

## 1. Core Idea

A **shared object** is code loaded into a process at **runtime**, instead of being copied into the executable at compile/build time.

```text
Shared Object = runtime-loaded code
```

Common examples:

```text
libc.so
lib-tcpip.so
plugin.so
```

`.so` usually means:

```text
Shared Object
```

---

## 2. Why Shared Objects Exist

Without shared objects, every program may carry its own copy of library code.

With shared objects, many processes can use **one copy of the same code** in memory.

```text
                 +----------------+
Process A  ----> |                |
Process B  ----> |    libc.so     |  one code copy in RAM
Process C  ----> |                |
                 +----------------+
```

Main benefit:

```text
Save memory
Reuse common code
Support plugins/dynamic loading
```

---

## 3. Loading Methods

A shared object can be loaded in two main ways.

### 1. Loaded automatically at process launch

The executable contains information saying:

```text
When this process starts, load these shared libraries too.
```

Example:

```text
Application needs libc.so
        |
        v
System loader loads app + libc.so
```

Flow:

```text
Executable
    |
    | contains dependency info
    v
Loader
    |
    v
Process memory:
    app code + shared object
```

---

### 2. Loaded manually at runtime

The program can explicitly load a shared object using:

```c
dlopen();
dlsym();
dlclose();
```

This is often used for plugins or dynamic modules.

Flow:

```text
Program starts
    |
    v
Program decides to load plugin
    |
    | dlopen("plugin.so")
    v
Shared object is loaded at runtime
```

---

## 4. Shared Code vs Private Data

This is the most important point.

```text
Code is shared.
Global data is NOT automatically shared.
```

If a shared library has global data:

```c
int counter = 0;
```

Each process gets its own private copy of that data.

```text
Shared object code:
    one copy shared by all processes

Global data:
    Process A has its own counter
    Process B has its own counter
    Process C has its own counter
```

Diagram:

```text
              Shared Code
        +---------------------+
A ----> |                     |
B ----> |      libtest.so     |
C ----> |                     |
        +---------------------+

Private Data:
A: counter = 10
B: counter = 0
C: counter = 0
```

So if Process A changes a global variable inside the shared object, Process B will not see that change.

---

## 5. Read-Only Mapping

The shared code is usually mapped as **read-only**.

```text
Processes can execute the shared code
Processes cannot modify the shared code
```

This protects the system from one process corrupting shared library code used by others.

---

## 6. Shared Object vs Shared Memory

Do not confuse them.

| Concept       | Purpose     |
| ------------- | ----------- |
| Shared Object | Shares code |
| Shared Memory | Shares data |

```text
Shared Object:
    same code used by many processes
    data is process-private

Shared Memory:
    same data region used by many processes
    must be created explicitly
```

Diagram:

```text
Shared Object
-------------
Process A \
Process B  ---> same code
Process C /

Shared Memory
-------------
Process A \
Process B  ---> same data
Process C /
```

If processes need to share data, use explicit shared memory.

---

## 7. Similar to Linux/Unix

QNX shared objects are very similar to shared libraries on Linux/Unix.

```text
QNX .so model ≈ Linux/Unix .so model
```

So if you understand Linux shared libraries, the QNX idea is almost the same.

---

## 8. Quick Quiz Notes

### Q1: What is a shared object?

```text
Code loaded into a process at runtime instead of being copied into the executable at compile time.
```

### Q2: Give examples of shared objects.

```text
libc.so
lib-tcpip.so
plugin.so
```

### Q3: How can a shared object be loaded?

```text
Automatically by the loader at process launch
Manually using dlopen() / dlsym()
```

### Q4: If multiple processes use the same shared object, how many code copies are needed in memory?

```text
Usually one shared code copy in memory
```

### Q5: Is global data inside a shared object shared between processes?

```text
No. Global data is process-private.
```

### Q6: How do processes share data if needed?

```text
Use explicit shared memory.
```

---

## 9. Final Mental Model

```text
Shared Object:
    loaded at runtime
    shared code
    read-only code mapping
    process-private global data
    explicit shared memory needed for shared data
```

Final key sentence:

```text
Shared objects share code between processes, but they do not automatically share global data.
```