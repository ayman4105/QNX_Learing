# QNX IPC — Shared Memory Quick README

## 1. Core Idea

Shared memory means two or more processes can access the same underlying RAM area.

Instead of copying a large buffer from one process to another using `MsgSend()`, both processes map the same memory and use it directly.

```text
Process A                 Process B
---------                 ---------
ptr_A  ----\             /---- ptr_B
           v             v
        +-------------------+
        |   Shared Memory   |
        +-------------------+
```

Use shared memory for **large data**.
Use message passing for **control, metadata, and synchronization**.

---

## 2. Why Shared Memory?

Normal message passing copies data between processes.

```text
Client buffer  --copy-->  Server buffer
```

This is fine for small messages, but expensive for large data like:

```text
images
video frames
large logs
large sensor buffers
graphics buffers
```

Shared memory avoids the large copy.

```text
Client writes data in shared memory
Server reads same data from shared memory
```

---

## 3. Important Rule About Pointers

Each process has its own virtual address space.

So the same shared memory may appear at different addresses:

```text
Process A pointer = 0x10000000
Process B pointer = 0x30000000
```

Both point to the same underlying memory, but the pointer values are different.

So inside shared memory, avoid storing raw pointers.

Use:

```text
offset
index
size
ID
```

instead of pointers.

---

## 4. POSIX Shared Memory Creation Flow

The creator process usually does:

```text
shm_open()
ftruncate()
mmap()
close(fd)
```

---

## 5. Step 1 — `shm_open()`

Create or open a shared memory object.

```c
fd = shm_open("/my_shared_mem", O_RDWR | O_CREAT, 0600);
```

Meaning:

```text
/name:
    shared memory name

O_RDWR:
    open for read and write

O_CREAT:
    create if it does not exist

0600:
    owner can read/write, others cannot access
```

Use names that start with `/`.

Good:

```text
/my_shared_mem
/my_project/frame_buffer
```

Avoid:

```text
my_shared_mem
```

because it may depend on current working directory.

---

## 6. `O_EXCL`

You can combine `O_CREAT` with `O_EXCL`.

```c
fd = shm_open("/my_shared_mem", O_RDWR | O_CREAT | O_EXCL, 0600);
```

Meaning:

```text
Create only if it does not already exist.
If it already exists, fail.
```

This is useful when many processes may start at the same time and only one should become the creator.

---

## 7. Step 2 — `ftruncate()`

After `shm_open()`, the object size is zero.

You must set the size:

```c
ftruncate(fd, SHARED_SIZE);
```

This allocates memory for the shared memory object.

The size is rounded up to page size.

Typical page size:

```text
4 KB
```

Example:

```text
Requested size = 21 KB
Actual allocation = 24 KB
```

New memory is zero-filled.

---

## 8. Step 3 — `mmap()`

`mmap()` gives the process a pointer to the shared memory.

```c
ptr = mmap(NULL, SHARED_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
```

Meaning:

```text
NULL:
    let OS choose virtual address

SHARED_SIZE:
    mapping size

PROT_READ | PROT_WRITE:
    allow read/write access

MAP_SHARED:
    changes are visible to other processes

fd:
    shared memory object

0:
    offset from start
```

Always use `MAP_SHARED` for shared memory IPC.

Do not use `MAP_PRIVATE` for this case, because that creates private copy behavior.

---

## 9. Close FD After Mapping

After successful `mmap()`, you can close the file descriptor:

```c
close(fd);
```

The mapping still remains valid.

`close(fd)` only releases the file descriptor resource.

---

## 10. Accessor Process Flow

A process that only wants to access existing shared memory does:

```text
shm_open()
mmap()
close(fd)
```

It does not call `ftruncate()`.

Example:

```c
fd = shm_open("/my_shared_mem", O_RDWR, 0);
ptr = mmap(NULL, SHARED_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
close(fd);
```

The third argument is `0` because permissions are only meaningful when creating.

---

## 11. Cleanup

Important cleanup calls:

```text
close(fd)
munmap(ptr, size)
shm_unlink("/name")
```

### `close(fd)`

Closes the file descriptor.

### `munmap()`

Removes the mapping from the process.

```c
munmap(ptr, SHARED_SIZE);
```

### `shm_unlink()`

Removes the name from the system.

```c
shm_unlink("/my_shared_mem");
```

If you do not unlink the name, the shared memory object can remain visible.

---

## 12. `/dev/shm` in QNX

QNX exposes named shared memory objects under:

```text
/dev/shm
```

This is useful for debugging.

You can use:

```sh
ls /dev/shm
rm /dev/shm/my_shared_mem
```

Programmatic cleanup is normally done with:

```c
shm_unlink("/my_shared_mem");
```

---

## 13. Reference Count Idea

Shared memory remains alive while references exist.

References include:

```text
name in pathname space
open file descriptor
active mmap mapping
```

Example:

```text
shm_open()     -> fd reference
ftruncate()    -> memory exists
mmap()         -> mapping reference
close(fd)      -> fd reference removed
munmap()       -> mapping reference removed
shm_unlink()   -> name reference removed
```

Memory is freed when the last reference disappears.

---

## 14. Main Problems With Shared Memory

Shared memory has two main problem groups:

```text
1. Access issues
2. Synchronization issues
```

---

## 15. Access Issues

Problems:

```text
pathname collisions
wrong process accessing memory
security limitations of file permissions
```

Example collision:

```text
Two teams both choose /myname
```

Better naming:

```text
/company/project/component/object
```

---

## 16. Synchronization Issues

If two processes access the same memory, you must control who reads/writes and when.

Possible tools:

```text
process-shared mutex
process-shared condition variable
process-shared semaphore
reader-writer lock
atomic operations
message passing
```

---

## 17. Process-shared Synchronization

Normal pthread synchronization objects are usually for threads inside one process.

For shared memory, mark them as process-shared.

For pthread mutex/cond/etc:

```text
PTHREAD_PROCESS_SHARED
```

For unnamed semaphores:

```c
sem_init(&sem, 1, initial_value);
```

The second parameter `1` means shared between processes.

---

## 18. Robust Mutexes

Problem:

```text
Process A locks shared mutex
Process A crashes
Process B tries to lock same mutex
```

The mutex may remain locked.

Solution:

```text
robust mutex
```

Use attributes:

```text
pthread_mutexattr_setpshared()
pthread_mutexattr_setrobust()
```

If the owner dies, the next locker gets:

```text
EOWNERDEAD
```

Meaning:

```text
The previous owner died while holding the mutex.
Protected data may be inconsistent.
```

If you can repair the data, call:

```c
pthread_mutex_consistent(&mutex);
```

Then unlock normally.

If you cannot repair the data, unlock without making it consistent.
Future lockers may get:

```text
ENOTRECOVERABLE
```

---

## 19. Best Practical Design

Do not use shared memory alone.

Common design:

```text
Message passing:
    control
    metadata
    synchronization

Shared memory:
    bulk data
```

Example:

```text
Client writes image into shared memory
Client sends MsgSend(): display image 5
Server reads image 5 from shared memory
Server replies when done
```

This avoids copying the big image while keeping control synchronized.

---

## 20. Anonymous Shared Memory

Named shared memory is visible by pathname.

For better access control, QNX supports anonymous shared memory:

```text
SHM_ANON
```

Each call creates a separate unnamed object.

```text
shm_open(SHM_ANON, ...)
```

No one can find it by name.

---

## 21. Shared Memory Handles

Anonymous memory can be shared using handles.

Flow:

```text
Server creates anonymous shared memory
Server creates handle for a specific client PID
Server sends handle to client
Client opens/maps the handle
```

Server uses:

```text
shm_create_handle()
```

Client uses:

```text
shm_open_handle()
```

or safer:

```text
shm_open_handle_pid()
```

`shm_open_handle_pid()` checks that the handle came from the expected server PID.

---

## 22. Quick Comparison

| Method                  | Use Case                                     | Access Style         |
| ----------------------- | -------------------------------------------- | -------------------- |
| Named shared memory     | simple shared object with known name         | pathname `/name`     |
| Anonymous shared memory | more private sharing                         | no pathname          |
| Shared memory handle    | share anonymous memory with selected process | handle passed by IPC |

---

## 23. Common Mistakes

### Mistake 1: Using shared memory without synchronization

Shared memory does not protect itself.

### Mistake 2: Storing raw pointers in shared memory

Pointers may differ between processes.

Use offsets instead.

### Mistake 3: Forgetting `ftruncate()`

After `shm_open()`, the object size is zero.

### Mistake 4: Using `MAP_PRIVATE`

For IPC shared memory, use `MAP_SHARED`.

### Mistake 5: Forgetting `shm_unlink()`

The name remains until explicitly removed.

### Mistake 6: Using normal mutex attributes

For interprocess synchronization, use `PTHREAD_PROCESS_SHARED`.

---

## 24. Quick Quiz

### Q1: What is shared memory used for?

```text
Sharing large data between processes without copying it through MsgSend().
```

### Q2: What are the main POSIX steps to create shared memory?

```text
shm_open()
ftruncate()
mmap()
```

### Q3: Why do we need `ftruncate()`?

```text
To set the size of the shared memory object.
```

### Q4: Why do we need `mmap()`?

```text
To get a virtual address pointer to the shared memory.
```

### Q5: Which mapping type should we use?

```text
MAP_SHARED
```

### Q6: Can the creator close the fd after mmap?

```text
Yes.
The mapping remains valid.
```

### Q7: What removes the name?

```text
shm_unlink()
```

### Q8: What is the best practice design?

```text
Use message passing for control and shared memory for bulk data.
```

### Q9: Why are robust mutexes useful?

```text
They handle the case where a process dies while holding a shared mutex.
```

### Q10: What should be stored instead of raw pointers?

```text
offsets or indexes
```

---

## Final Summary

```text
Shared memory:
    same RAM visible to multiple processes

Create:
    shm_open + ftruncate + mmap

Use:
    access through pointer

Cleanup:
    close + munmap + shm_unlink

Problems:
    access control
    synchronization

Best design:
    MsgSend for control
    shared memory for big data
```

Golden sentence:

```text
Shared memory avoids large data copies, but message passing is still usually used to control access, pass metadata, and synchronize users of that memory.
```