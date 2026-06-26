# QNX Hardware I/O — Quick README

## Core Idea

Hardware devices use physical addresses.

User processes run in virtual address space.

So before a process can access device memory or registers, QNX must map the device physical address into the process virtual address space.

```text
Physical device address
        |
        v
mmap()
        |
        v
Virtual pointer inside process
```

---

## 1. Accessing Device Memory

Example use case:

```text
video frame buffer
memory-mapped control registers
device memory region
```

Use `mmap()` with physical mapping flags.

Typical idea:

```c
ptr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PHYS | MAP_SHARED, NOFD, physical_address);
```

Meaning:

```text
size:
    size of hardware memory region

PROT_READ | PROT_WRITE:
    allow read and write access

MAP_PHYS:
    map physical memory

MAP_SHARED:
    shared mapping, not private copy

NOFD:
    no file descriptor is used

physical_address:
    device physical address / offset
```

`mmap()` returns a virtual pointer that the process can use.

---

## 2. Cache Control

For hardware memory, cache behavior matters.

Some regions should not be cached, especially control registers.

Reason:

```text
CPU cache could hide real hardware changes
writes may not reach hardware immediately
reads may return old values
```

So when mapping hardware, choose protection/cache options carefully according to the device type.

---

## 3. DMA Memory

DMA means:

```text
Direct Memory Access
```

The hardware reads/writes memory directly without CPU copying every byte.

DMA often needs physically contiguous memory.

For DMA allocation, use `mmap()` with:

```text
MAP_PHYS | MAP_ANON
```

Concept:

```c
dma_buf = mmap(NULL, length, PROT_READ | PROT_WRITE, MAP_PHYS | MAP_ANON, NOFD, 0);
```

Meaning:

```text
MAP_ANON:
    allocate anonymous memory

MAP_PHYS:
    request physical memory suitable for hardware access

offset = 0:
    because we are allocating new physical memory, not mapping existing device address
```

---

## 4. Getting Physical Address for DMA

The process gets a virtual pointer from `mmap()`.

But the hardware DMA engine needs a physical address.

Use:

```c
mem_offset();
```

Purpose:

```text
virtual pointer -> physical address
```

Flow:

```text
mmap()
    gives virtual pointer

mem_offset()
    gives physical address for hardware/DMA
```

---

## 5. Accessing Control Registers on AArch64

On AArch64, hardware control registers are usually memory-mapped.

That means after mapping them with `mmap()`, you access them like memory:

```c
volatile uint32_t *reg = mapped_base;
uint32_t value = reg[0];
reg[1] = 0x12345678;
```

Use `volatile` because these addresses represent hardware, not normal RAM.

`volatile` tells the compiler:

```text
do not optimize away reads/writes
always perform the actual memory access
```

---

## 6. How QNX Knows Hardware Regions

QNX `procnto` gets hardware information from startup code.

Startup code builds system pages.

System pages describe important platform information, such as:

```text
RAM areas
hardware address regions
CPU/platform information
special memory flags
```

This helps QNX know that some addresses are hardware regions, not normal RAM.

---

## 7. x86 I/O Ports

Some x86 devices use old-style I/O ports instead of memory-mapped registers.

This is separate from normal memory addressing.

Use QNX I/O port helper functions from:

```c
#include <hw/inout.h>
```

Read functions:

```text
in8()
in16()
in32()
```

Write functions:

```text
out8()
out16()
out32()
```

---

## 8. I/O Privilege

Before accessing x86 I/O ports, the thread needs I/O privilege.

Use:

```c
ThreadCtl();
```

Typical idea:

```c
ThreadCtl(_NTO_TCTL_IO, 0);
```

Without the required privilege, port I/O access will fail or be blocked.

---

## 9. Quick Decision Table

| Goal                                      | Use                                   |
| ----------------------------------------- | ------------------------------------- |
| Map existing hardware physical address    | `mmap()` with `MAP_PHYS | MAP_SHARED` |
| Allocate physically contiguous DMA memory | `mmap()` with `MAP_PHYS | MAP_ANON`   |
| Get physical address from virtual pointer | `mem_offset()`                        |
| Access AArch64 control registers          | memory-mapped `volatile` pointers     |
| Access x86 I/O ports                      | `in8/in16/in32`, `out8/out16/out32`   |
| Enable I/O port access                    | `ThreadCtl()`                         |

---

## 10. Common Mistakes

### Mistake 1: Treating physical addresses like normal pointers

A process cannot directly use a physical address.

You must map it first.

### Mistake 2: Forgetting `volatile` for hardware registers

Hardware register pointers should be volatile.

### Mistake 3: Using cached mapping for control registers

Control registers usually need careful cache settings.

### Mistake 4: Giving hardware a virtual address for DMA

Hardware needs a physical address, usually obtained with `mem_offset()`.

### Mistake 5: Using x86 I/O ports without I/O privilege

Use `ThreadCtl()` first.

---

## Final Summary

```text
Device memory access:
    mmap physical address into process virtual space

DMA memory:
    allocate physical memory, then get physical address with mem_offset

AArch64 registers:
    memory-mapped, use volatile pointers

x86 ports:
    use in/out functions from hw/inout.h and enable I/O privilege
```

Golden sentence:

```text
In QNX hardware I/O, the process never uses device physical addresses directly; it maps them into virtual memory with mmap(), uses volatile memory access for memory-mapped registers, and uses special in/out functions for x86 I/O ports.
```