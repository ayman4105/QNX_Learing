# QNX Process Manager — Study Notes

These notes explain the **QNX Process Manager** section in a simple way, based on the course script and our step-by-step discussion.

---

# 1. Main Idea

The QNX Process Manager is a core part of QNX.

It is part of:

```text
procnto
```

The name `procnto` can be understood as:

```text
proc  → Process Manager
nto   → Neutrino Microkernel
```

So:

```text
procnto = Process Manager + Neutrino Microkernel
```

They are both inside the kernel image and share the same address space, but they are different components with different responsibilities.

---

# 2. procnto Structure

```text
procnto
 ├── Neutrino Microkernel
 │   ├── Scheduling
 │   ├── IPC / message passing
 │   ├── Interrupts
 │   └── Synchronization
 │
 └── Process Manager
     ├── Process creation
     ├── Process termination
     ├── Virtual memory
     ├── Shared memory
     ├── mmap mappings
     └── Path namespace
```

The microkernel is close to CPU-level and real-time behavior.

The Process Manager manages processes, memory, and the global path namespace.

---

# 3. Microkernel vs Process Manager

## 3.1 Neutrino Microkernel

The Neutrino microkernel handles low-level real-time OS features such as:

```text
Thread scheduling
Interrupt handling
Message passing
Synchronization
Kernel calls
```

This is the part related to scheduling and IPC.

---

## 3.2 Process Manager

The Process Manager handles higher-level process and resource management, such as:

```text
Creating processes
Terminating processes
Grouping threads inside a process
Creating the initial thread
Managing address spaces
Managing memory protection
Handling shared memory
Handling mmap()
Managing the path namespace
Routing pathname requests to resource managers
```

---

# 4. Applications Communicate Using Messages

In QNX, applications talk to the Process Manager through messages.

However, many of these messages are hidden behind normal C APIs.

For example:

```c
open("/dev/ser1", O_RDWR);
```

The application sees only `open()`.

But internally, QNX may send messages to the Process Manager to ask:

```text
Who owns this path?
Which resource manager should handle it?
Is the path valid?
What object should be opened?
```

So:

```text
Application C API
        |
        v
Hidden QNX messages
        |
        v
Process Manager / Resource Manager
```

---

# 5. Process Manager Main Responsibilities

The Process Manager is responsible for three big areas:

```text
1. Process management
2. Memory management
3. Path namespace management
```

Diagram:

```text
Process Manager
 ├── Processes
 │   ├── spawn
 │   ├── fork
 │   ├── exec
 │   └── terminate
 │
 ├── Memory
 │   ├── virtual address space
 │   ├── memory protection
 │   ├── shared memory
 │   └── mmap
 │
 └── Path Namespace
     ├── /
     ├── /proc
     ├── /dev/shmem
     ├── /dev/sem
     ├── /dev/ser1
     └── /etc/...
```

---

# 6. Process Management

The Process Manager packages all threads inside a process.

Even though QNX schedules threads, the process is still important because it is a container for related threads and resources.

Example:

```text
Process: EthernetService
 ├── RX Thread
 ├── TX Thread
 └── Monitor Thread
```

The Process Manager knows that these threads belong to the same process.

---

# 7. Creating a Process

When a process is created, the Process Manager is involved in:

```text
Creating the process object
Creating the initial thread
Preparing the virtual address space
Loading program code
Assigning memory
Preparing resources
```

A process cannot execute code without at least one thread.

```text
Process = resource container
Thread  = actual execution unit
```

---

# 8. spawn, fork, exec, terminate

## 8.1 spawn

`spawn` creates a new process and starts a program.

```text
spawn = create new process + run executable
```

---

## 8.2 fork

`fork` creates a new process as a copy of the current process.

```text
fork = duplicate current process
```

---

## 8.3 exec

`exec` replaces the current process image with a new program.

```text
exec = same process, new program image
```

---

## 8.4 terminate

`terminate` ends a process and allows QNX to clean its resources.

```text
terminate = stop process + clean resources
```

---

# 9. Initial Thread

When a process starts, QNX must create an initial thread.

For a normal C program, this thread eventually runs:

```c
main();
```

Conceptually:

```text
Create process
        |
        v
Create initial thread
        |
        v
Start executing program code
```

Important idea:

```text
A process without a thread cannot execute.
```

---

# 10. Memory Management

The Process Manager manages memory for processes.

This includes:

```text
Virtual address spaces
Memory protection
Mapping virtual addresses to physical memory
Shared memory
mmap mappings
Memory blocks requested by applications
```

---

# 11. Virtual Address Space

Each process has its own virtual address space.

This is the process's private view of memory.

Example:

```text
Process A Virtual Address Space
 ├── Code
 ├── Data
 ├── Heap
 └── Stack

Process B Virtual Address Space
 ├── Code
 ├── Data
 ├── Heap
 └── Stack
```

Each process thinks it has its own memory layout.

The Process Manager maps these virtual addresses to real physical memory.

---

# 12. Virtual Address vs Physical Address

## 12.1 Virtual Address

A virtual address is what the application sees.

Example:

```c
int x = 10;
int *ptr = &x;
```

The value stored in `ptr` is a virtual address inside the current process.

---

## 12.2 Physical Address

A physical address is the real address in RAM or in the hardware memory map.

Example:

```text
UART registers at physical address 0xFE201000
```

A user application should not use this address directly as a pointer.

---

# 13. Why Physical Addresses Cannot Be Used Directly

Bad idea:

```c
volatile uint32_t *uart = (volatile uint32_t *)0xFE201000;
```

This assumes that `0xFE201000` is valid inside the current process virtual address space.

Usually, this is wrong.

The correct idea is:

```text
Ask QNX to map this physical address into my virtual address space.
```

This is done using `mmap()`.

---

# 14. mmap()

`mmap()` maps memory into a process virtual address space.

You can use it to map:

```text
Physical memory
Device registers
Shared memory
File-backed memory
```

Conceptually:

```text
Application asks:
    "Map this physical memory region into my process."

Process Manager:
    Creates a virtual mapping.

Application receives:
    A virtual pointer.
```

---

# 15. Hardware Register Mapping Example

Suppose a device register exists at:

```text
Physical address = 0x48000000
Size             = 0x1000
```

The application requests a mapping using `mmap()`.

QNX may return a virtual address like:

```text
0x10234000
```

Meaning:

```text
Application uses: 0x10234000
Actually maps to: 0x48000000 physical
```

Diagram:

```text
Process Virtual Address Space

0x10234000  --------------------+
                                |
                                v
Physical Memory / Device Register

0x48000000  --------------------+
```

Important:

```text
Use the pointer returned by mmap().
Do not directly use the physical address as a pointer.
```

---

# 16. MMU Role

The MMU translates virtual addresses to physical addresses.

```text
Application pointer
        |
        v
Virtual address
        |
        v
MMU translation
        |
        v
Physical address
```

The Process Manager prepares the mappings that the MMU uses.

---

# 17. Memory Protection

Memory protection prevents one process from corrupting another process.

Example:

```text
Process A has a pointer bug.
It writes to an invalid address.
```

With memory protection:

```text
QNX can fault Process A.
Process B remains safe.
Kernel memory remains protected.
```

This is important for embedded and automotive systems.

---

# 18. Shared Memory

Shared memory allows two or more processes to access the same physical memory block.

Example:

```text
Producer Process
        |
        v
Shared Physical Memory
        ^
        |
Consumer Process
```

The same physical memory can appear at different virtual addresses in different processes.

Example:

```text
Process A sees shared block at: 0x10000000
Process B sees shared block at: 0x30000000

Both map to the same physical memory block.
```

---

# 19. Important Pointer Rule

A pointer value is valid only inside the process that owns that virtual address space.

If Process A sends this pointer to Process B:

```text
0x10000000
```

Process B cannot safely use it as a pointer.

Why?

Because Process B has a different virtual address space.

Correct design:

```text
Both processes map the same shared memory object.
Each process uses its own returned pointer.
```

---

# 20. Shared Memory Example

Example system:

```text
CameraProcess
AIProcess
```

Without shared memory:

```text
CameraProcess copies frame to AIProcess
```

With shared memory:

```text
CameraProcess writes frame to shared memory
AIProcess reads frame from the same shared memory
```

This avoids large data copies.

Useful for:

```text
Camera frames
Sensor buffers
Audio buffers
Network packets
High-speed producer/consumer data
```

---

# 21. malloc and Process Manager

When you call:

```c
malloc(1024);
```

The C library may satisfy the request from existing heap memory.

But eventually, larger memory blocks must come from the OS.

The Process Manager provides those larger memory blocks to the process.

```text
malloc/new/free        → C/C++ library level
Raw memory from OS     → Process Manager
```

---

# 22. Contiguous Virtual Memory

The Process Manager can give a process memory that looks contiguous virtually, even if physical memory is fragmented.

```text
Virtual memory:
+---------------------------+
| One contiguous block      |
+---------------------------+

Physical memory:
+------+      +------+      +------+
| part |      | part |      | part |
+------+      +------+      +------+
```

This makes programming easier because the application sees a linear memory block.

---

# 23. Kernel Address Space

The kernel/microkernel has its own address space region separate from user applications.

Important idea:

```text
User applications cannot directly access kernel memory.
```

When an application makes a kernel call, QNX can switch privileges and execute kernel code safely.

This separation improves protection and performance.

---

# 24. Path Namespace

The Process Manager is responsible for the entire path namespace.

The path namespace is the global tree of paths starting from:

```text
/
```

Example:

```text
/
├── proc
├── dev
├── etc
├── lib
└── tmp
```

QNX has a directory-tree-like namespace, but the important difference is that paths are connected to resource managers.

---

# 25. Paths Are Not Always Real Files

In QNX, a path can represent:

```text
Real file
Device
Pseudo file
Process information
Shared memory view
Semaphore view
Custom service
Driver
```

Examples:

```text
/proc      → process/system information
/dev/shmem → shared memory objects
/dev/sem   → semaphore objects
/dev/ser1  → serial driver
/dev/con1  → console driver
```

---

# 26. Resource Manager

A resource manager is a server or driver that attaches to a path and handles requests for that path.

Example:

```text
devc-ser8250 attaches to /dev/ser1
devc-con attaches to /dev/con1
Process Manager provides /proc
```

Applications use normal file APIs:

```c
open();
read();
write();
close();
```

But the request is handled by the resource manager behind the path.

---

# 27. Path Namespace Boot Idea

At the beginning of boot, the namespace starts with only:

```text
/
```

The Process Manager controls this root.

Then drivers and resource managers attach paths.

Example:

```text
/
├── proc/       → Process Manager pseudo filesystem
├── dev/
│   ├── con1    → devc-con
│   ├── ser1    → devc-ser8250
│   ├── shmem   → shared memory view
│   └── sem     → semaphore view
├── etc/        → filesystem/disk driver
├── usr/        → filesystem/disk driver
└── lib/        → filesystem/disk driver
```

---

# 28. open() Flow

When an application calls:

```c
int fd = open("/dev/ser1", O_RDWR);
```

Conceptually:

```text
Application
    |
    | open("/dev/ser1")
    v
C Library
    |
    | hidden message/request
    v
Process Manager
    |
    | Who owns /dev/ser1?
    v
Serial Resource Manager
    |
    | handles the open request
    v
Application gets file descriptor
```

---

# 29. Most Specific Match

When resolving a path, QNX uses the most specific matching owner.

Example namespace:

```text
/
├── /dev/con1 handled by devc-con
└── /dev/ser1 handled by devc-ser8250
```

If the application opens:

```c
open("/dev/ser1", O_RDWR);
```

The Process Manager chooses:

```text
/dev/ser1
```

not just:

```text
/
```

So:

```text
Most specific matching path wins.
```

---

# 30. Example: /dev/con1

```c
open("/dev/con1", O_RDWR);
```

Flow:

```text
Application
    |
    | open("/dev/con1")
    v
Process Manager
    |
    | /dev/con1 belongs to devc-con
    v
devc-con
    |
    v
Console device
```

---

# 31. Example: /dev/ser1

```c
open("/dev/ser1", O_RDWR);
```

Flow:

```text
Application
    |
    | open("/dev/ser1")
    v
Process Manager
    |
    | /dev/ser1 belongs to devc-ser8250
    v
devc-ser8250
    |
    v
Serial hardware
```

---

# 32. Example: /etc/config.txt

```c
open("/etc/config.txt", O_RDONLY);
```

Flow:

```text
Application
    |
    | open("/etc/config.txt")
    v
Process Manager
    |
    | No special /etc/config.txt handler
    | Fallback to filesystem/disk driver
    v
devb-eide
    |
    v
Disk / storage
```

---

# 33. Device and Service as File-Like Resources

QNX makes devices and services look like paths.

This allows applications to use the same APIs for many resources.

```c
open();
read();
write();
close();
```

These APIs can be used with:

```text
Disk files
Serial ports
Console
Pseudo devices
Custom drivers
Shared memory views
Process information views
```

---

# 34. Custom Resource Manager Example

Suppose you create a temperature sensor service.

You can expose it as:

```text
/dev/temp_sensor
```

Application code:

```c
int fd = open("/dev/temp_sensor", O_RDONLY);
read(fd, &temperature, sizeof(temperature));
close(fd);
```

Conceptual flow:

```text
Application
    |
    | read("/dev/temp_sensor")
    v
Process Manager
    |
    | /dev/temp_sensor belongs to temp_sensor resource manager
    v
Temp Sensor Resource Manager
    |
    | reads sensor or cached value
    v
Application receives temperature
```

This is very common in QNX-style embedded design.

---

# 35. /proc

`/proc` is a pseudo filesystem created by the Process Manager.

It provides a view of running processes and system information.

Example:

```sh
ls /proc
```

This does not read normal files from disk.

It asks the Process Manager or related resource manager to provide a live view of the system.

---

# 36. /dev/shmem

`/dev/shmem` provides a view of shared memory objects.

It is related to shared memory management.

```text
/dev/shmem → shared memory objects
```

---

# 37. /dev/sem

`/dev/sem` provides a view of named semaphores.

```text
/dev/sem → semaphore objects
```

---

# 38. Full Process Manager Summary

```text
QNX Process Manager is part of procnto.

procnto = process manager + Neutrino microkernel.

Applications communicate with the Process Manager through messages,
but many messages are hidden behind C APIs like:
    open()
    mmap()
    spawn()
    fork()
    exec()

The Process Manager handles:
    process creation
    process termination
    initial thread creation
    virtual address spaces
    memory protection
    shared memory
    mmap mappings
    path namespace management
    routing path requests to resource managers

QNX path namespace starts at /.

Drivers and services attach themselves to paths.

When an application opens a path,
the Process Manager finds the most specific matching resource manager.

Paths in QNX can represent:
    real files
    devices
    pseudo files
    process information
    shared memory
    semaphores
    custom services
```

---

# 39. Big Diagram

```text
                         +----------------------+
                         |      Application     |
                         +----------+-----------+
                                    |
                                    | C APIs
                                    | open(), mmap(), spawn()
                                    v
                         +----------------------+
                         |      C Library       |
                         +----------+-----------+
                                    |
                                    | Hidden QNX messages
                                    v
+---------------------------------------------------------------+
|                           procnto                             |
|                                                               |
|   +-----------------------+    +---------------------------+   |
|   | Neutrino Microkernel  |    |     Process Manager       |   |
|   |                       |    |                           |   |
|   | Scheduling            |    | Process creation          |   |
|   | IPC                   |    | Virtual memory            |   |
|   | Interrupts            |    | Shared memory             |   |
|   | Synchronization       |    | Path namespace            |   |
|   +-----------------------+    +-------------+-------------+   |
|                                               |                 |
+-----------------------------------------------|-----------------+
                                                |
                                                | Path lookup
                                                v
                              +-----------------------------+
                              |      Resource Manager       |
                              |  /dev/ser1, /dev/con1, ... |
                              +-----------------------------+
```

---

# 40. Mental Model

Think of the QNX Process Manager like this:

```text
It creates and owns processes.
It gives each process its memory view.
It protects processes from each other.
It maps shared memory and hardware regions.
It owns the global pathname tree.
It connects paths to resource managers.
It lets applications access many resources using file-like APIs.
```

Final key sentence:

```text
The QNX Process Manager makes processes, memory, and pathname-based resources work together through message passing.
```
