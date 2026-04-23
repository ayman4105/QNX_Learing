# QNX Microkernel - Complete Guide

## Table of Contents
- [Introduction](#introduction)
- [What is the Microkernel?](#what-is-the-microkernel)
- [Communicating with the Microkernel](#communicating-with-the-microkernel)
- [What Happens During a Kernel Call?](#what-happens-during-a-kernel-call)
- [Microkernel Threads](#microkernel-threads)
- [Microkernel Services](#microkernel-services)
- [Interprocess Communication (IPC)](#interprocess-communication-ipc)
- [Thread Management](#thread-management)
- [Thread Synchronization](#thread-synchronization)
- [Time Services](#time-services)
- [Interrupt Handling](#interrupt-handling)
- [When Does the Microkernel Run?](#when-does-the-microkernel-run)
- [Where Does the Microkernel Run?](#where-does-the-microkernel-run)
- [Summary](#summary)

---

## Introduction

This document provides a comprehensive overview of the QNX Microkernel, the heart of the QNX Real-Time Operating System. The microkernel architecture makes QNX highly reliable, modular, and suitable for real-time applications.

---

## What is the Microkernel?

The **Microkernel** is the core component of the QNX system. It has two primary responsibilities:

1. **Handle Hardware Interrupts** - Receives interrupts from hardware and routes them to applications
2. **Interprocess Communication (IPC)** - Enables processes to communicate with each other

```
┌─────────────────────────────────────────────────────────────┐
│                        QNX SYSTEM                           │
│                                                             │
│  ┌───────────────────────────────────────────────────────┐ │
│  │                   APPLICATIONS                         │ │
│  │    ┌─────┐    ┌─────┐    ┌─────┐    ┌─────┐          │ │
│  │    │App A│    │App B│    │App C│    │App D│          │ │
│  │    └──┬──┘    └──┬──┘    └──┬──┘    └──┬──┘          │ │
│  └───────┼─────────┼─────────┼─────────┼────────────────┘ │
│          │         │         │         │                   │
│          ▼         ▼         ▼         ▼                   │
│  ┌───────────────────────────────────────────────────────┐ │
│  │              ★ MICROKERNEL ★                          │ │
│  │                                                        │ │
│  │   • Handle Hardware Interrupts                        │ │
│  │   • IPC (Interprocess Communication)                  │ │
│  │                                                        │ │
│  └───────────────────────────────────────────────────────┘ │
│                         ▲                                   │
│                         │                                   │
│  ┌───────────────────────────────────────────────────────┐ │
│  │                    HARDWARE                            │ │
│  │   [CPU]  [Memory]  [Disk]  [Network]  [Timer]         │ │
│  └───────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────┘
```

---

## Communicating with the Microkernel

Programs communicate with the microkernel through **Kernel Calls** - special library routines that execute code in the microkernel.

### Identifying Kernel Calls

Kernel calls use **CamelCase** naming convention:

| Kernel Call | Purpose |
|-------------|---------|
| `MsgSend()` | Send a message |
| `MsgReceive()` | Receive a message |
| `MsgReply()` | Reply to a message |
| `ThreadCreate()` | Create a new thread |
| `TimerCreate()` | Create a timer |

```
┌──────────────────────────────────────────────────────┐
│                YOUR APPLICATION                       │
│                                                       │
│    MsgSend()      ThreadCreate()      TimerCreate()  │
│        │               │                   │          │
│        └───────────────┼───────────────────┘          │
│                        │                              │
│                        ▼                              │
│              ┌─────────────────┐                      │
│              │  KERNEL CALLS   │                      │
│              │  (CamelCase)    │                      │
│              └────────┬────────┘                      │
└───────────────────────┼──────────────────────────────┘
                        │
                        ▼
              ┌─────────────────┐
              │  MICROKERNEL    │
              └─────────────────┘
```

---

## What Happens During a Kernel Call?

When a thread makes a kernel call, several things change:

```
┌────────────────────────────────────────────────────────────────┐
│                    BEFORE KERNEL CALL                          │
│  ┌──────────────────────────────────────────────────────────┐ │
│  │                YOUR THREAD                                │ │
│  │   • Running USER CODE                                     │ │
│  │   • USER-LEVEL privileges (limited)                       │ │
│  │   • Using REGULAR STACK                                   │ │
│  │   • Can only see YOUR address space                       │ │
│  └──────────────────────────────────────────────────────────┘ │
└────────────────────────────────────────────────────────────────┘
                              │
                              │  Kernel Call (e.g., MsgSend())
                              ▼
┌────────────────────────────────────────────────────────────────┐
│                    AFTER KERNEL CALL                           │
│  ┌──────────────────────────────────────────────────────────┐ │
│  │                YOUR THREAD (same thread!)                 │ │
│  │   • Running MICROKERNEL CODE                              │ │
│  │   • KERNEL-LEVEL privileges (full power)                  │ │
│  │   • Using KERNEL STACK                                    │ │
│  │   • Can see PROCNTO address space                         │ │
│  └──────────────────────────────────────────────────────────┘ │
└────────────────────────────────────────────────────────────────┘
```

### What Changes vs. What Stays the Same

| Aspect | Before (User Mode) | After (Kernel Mode) |
|--------|-------------------|---------------------|
| Code Running | User Code | Microkernel Code |
| Privilege Level | User (Limited) | Kernel (Full) |
| Stack | Regular Stack | Kernel Stack |
| Address Space | App Only | + Procnto Visible |

**Things that DON'T change:**
- ✓ Same Thread
- ✓ Same Priority
- ✓ Can be Pre-empted
- ✓ Can be Interrupted

> **Important:** The thread can still be pre-empted by higher priority threads, making QNX excellent for real-time applications!

---

## Microkernel Threads

The microkernel is primarily **externally driven** but maintains **3 dedicated threads per CPU core**:

```
┌─────────────────────────────────────────────────────────────────┐
│                         CPU CORE                                │
│  ┌───────────────────────────────────────────────────────────┐ │
│  │                  MICROKERNEL THREADS                       │ │
│  │                                                            │ │
│  │   ┌─────────────────────────────────────────────────────┐ │ │
│  │   │  1. IST for IPI (Interprocess Interrupts)           │ │ │
│  │   │     → Cores communicate with each other             │ │ │
│  │   └─────────────────────────────────────────────────────┘ │ │
│  │                                                            │ │
│  │   ┌─────────────────────────────────────────────────────┐ │ │
│  │   │  2. IST for Timer Interrupts                        │ │ │
│  │   │     → Managing timer activities                     │ │ │
│  │   └─────────────────────────────────────────────────────┘ │ │
│  │                                                            │ │
│  │   ┌─────────────────────────────────────────────────────┐ │ │
│  │   │  3. Idle Thread                                     │ │ │
│  │   │     → Runs when nothing else is running             │ │ │
│  │   └─────────────────────────────────────────────────────┘ │ │
│  │                                                            │ │
│  └───────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────┘
```

### IPI (Interprocess Interrupt) Thread

Used for cores to wake each other up in multi-core systems:

```
┌──────────┐                        ┌──────────┐
│  CORE 0  │  ── "Hey wake up!" ──► │  CORE 1  │
│          │         (IPI)          │ (sleeping)│
└──────────┘                        └──────────┘
```

---

## Microkernel Services

The microkernel provides the following services:

```
┌─────────────────────────────────────────────────────────────────┐
│                    MICROKERNEL SERVICES                         │
│                                                                  │
│   ┌─────────────────┐    ┌─────────────────┐                   │
│   │ Synchronization │    │    Scheduler    │                   │
│   │   Primitives    │    │                 │                   │
│   └─────────────────┘    └─────────────────┘                   │
│                                                                  │
│   ┌─────────────────┐    ┌─────────────────┐                   │
│   │     Thread      │    │  Time Services  │                   │
│   │   Execution     │    │                 │                   │
│   └─────────────────┘    └─────────────────┘                   │
│                                                                  │
│   ┌─────────────────┐    ┌─────────────────┐                   │
│   │   Interrupt     │    │  Fault Handler  │                   │
│   │   Redirector    │    │                 │                   │
│   └─────────────────┘    └─────────────────┘                   │
│                                                                  │
│   ┌─────────────────────────────────────────┐                  │
│   │      IPC (Interprocess Communication)    │                  │
│   └─────────────────────────────────────────┘                  │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

---

## Interprocess Communication (IPC)

The microkernel provides **4 methods** for processes to communicate:

### Overview

| Method | Type | Use Case |
|--------|------|----------|
| Messages | Synchronous (Blocking) | Client-Server communication |
| Pulses | Asynchronous (Non-blocking) | Quick notifications |
| Signals | Interrupt | Emergency/Termination |
| POSIX Message Queues | Queued (Fixed size) | Buffered delivery |

### 1. Native QNX Messages (Synchronous)

```
┌─────────────────────────────────────────────────────────────────┐
│                  NATIVE QNX MESSAGES                            │
│                     (SYNCHRONOUS)                                │
│                                                                  │
│   ┌──────────┐                         ┌──────────┐             │
│   │  CLIENT  │                         │  SERVER  │             │
│   │          │                         │          │             │
│   │  Step 1: │ ──── MsgSend() ───────► │ Receives │             │
│   │  Send    │      "Do something!"    │ message  │             │
│   │          │                         │          │             │
│   │ ⏳ WAIT  │                         │ Step 2:  │             │
│   │ (BLOCKED)│                         │ Process  │             │
│   │          │                         │          │             │
│   │  Step 3: │ ◄──── MsgReply() ────── │ Reply    │             │
│   │  Continue│      "Here's result"    │          │             │
│   └──────────┘                         └──────────┘             │
│                                                                  │
│   Client BLOCKS until server replies!                           │
└─────────────────────────────────────────────────────────────────┘
```

### 2. Pulses (Asynchronous)

```
┌─────────────────────────────────────────────────────────────────┐
│                      PULSES                                      │
│                   (ASYNCHRONOUS)                                 │
│                                                                  │
│   ┌──────────┐                         ┌──────────┐             │
│   │ PROCESS  │                         │ PROCESS  │             │
│   │    A     │                         │    B     │             │
│   │          │                         │          │             │
│   │  Send    │ ──── Pulse ───────────► │ Receives │             │
│   │  pulse   │   "Data arrived!"       │ pulse    │             │
│   │          │                         │          │             │
│   │ Continue │ (A doesn't wait!)       │ Handles  │             │
│   │ working! │                         │ event    │             │
│   └──────────┘                         └──────────┘             │
│                                                                  │
│   Fire and forget - sender continues immediately!               │
└─────────────────────────────────────────────────────────────────┘
```

### 3. POSIX Signals

Used primarily for process termination and error handling:

```
┌─────────────────────────────────────────────────────────────────┐
│                     POSIX SIGNALS                                │
│                                                                  │
│   Common Signals:                                                │
│   ┌─────────────┬────────────────────────────────────┐          │
│   │  SIGKILL    │  Kill immediately (can't ignore)   │          │
│   │  SIGTERM    │  Please terminate (can handle)     │          │
│   │  SIGSEGV    │  Segmentation fault                │          │
│   │  SIGFPE     │  Floating point error              │          │
│   └─────────────┴────────────────────────────────────┘          │
│                                                                  │
│   ⚠️  Signals are HEAVYWEIGHT - not recommended for             │
│      regular communication!                                      │
└─────────────────────────────────────────────────────────────────┘
```

### 4. POSIX Message Queues

```
┌─────────────────────────────────────────────────────────────────┐
│                   POSIX MESSAGE QUEUES                          │
│                    (Like a Mailbox)                              │
│                                                                  │
│   ┌──────────┐      ┌─────────────────────┐      ┌──────────┐  │
│   │ PROCESS  │      │    MESSAGE QUEUE    │      │ PROCESS  │  │
│   │    1     │      │     ┌───┐           │      │    2     │  │
│   │ (Sender) │ ───► │ ───►│ A │           │      │(Receiver)│  │
│   │          │      │     ├───┤           │      │          │  │
│   │   Send   │ ───► │ ───►│ B │           │ ───► │  Read    │  │
│   │          │      │     ├───┤           │      │  when    │  │
│   │   Send   │ ───► │ ───►│ C │           │      │  ready   │  │
│   └──────────┘      └─────────────────────┘      └──────────┘  │
│                                                                  │
│   • Messages are FIXED SIZE                                      │
│   • Messages are QUEUED (FIFO)                                  │
└─────────────────────────────────────────────────────────────────┘
```

### IPC Quick Reference

```
MESSAGES:    "Do this and tell me the result" → Wait for reply
PULSES:      "Hey! Something happened" → Continue working
SIGNALS:     "Terminate now!" → Emergency
MSG QUEUE:   "I'll leave messages here, read when ready" → Mailbox
```

---

## Thread Management

The microkernel provides functions to manage threads:

```
┌─────────────────────────────────────────────────────────────────┐
│                    THREAD FUNCTIONS                              │
│                                                                  │
│   ┌─────────────────────────────────────────────────────────┐   │
│   │                                                          │   │
│   │   ThreadCreate()     →   Create a new thread            │   │
│   │   ThreadDestroy()    →   Terminate a thread             │   │
│   │   ThreadJoin()       →   Wait for thread completion     │   │
│   │   ThreadCtl()        →   Change thread attributes       │   │
│   │                                                          │   │
│   └─────────────────────────────────────────────────────────┘   │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### ThreadJoin() Example

```
┌───────────────┐              ┌───────────────┐
│  MAIN THREAD  │              │ WORKER THREAD │
│               │              │               │
│  Create ──────┼─────────────►│  Start        │
│  worker       │              │    │          │
│               │              │    ▼          │
│  ThreadJoin() │              │  Working...   │
│       │       │              │    │          │
│   ⏳ WAIT     │              │    ▼          │
│       │       │              │  Working...   │
│       │       │              │    │          │
│       │       │◄─────────────│  DONE! ✓     │
│       ▼       │   (returns)  │               │
│  Continue!    │              │               │
│  Read result  │              │               │
└───────────────┘              └───────────────┘
```

---

## Thread Synchronization

When multiple threads access shared data, synchronization is required:

```
┌─────────────────────────────────────────────────────────────────┐
│                    THE PROBLEM                                   │
│                                                                  │
│   ┌──────────┐          SHARED DATA          ┌──────────┐       │
│   │ THREAD A │              │ │              │ THREAD B │       │
│   │  Write   │ ────────────►│ │◄──────────── │  Write   │       │
│   │  X = 5   │         ┌────┴─┴────┐         │  X = 10  │       │
│   └──────────┘         │   X = ?   │         └──────────┘       │
│                        └───────────┘                             │
│                           CHAOS!                                 │
└─────────────────────────────────────────────────────────────────┘
```

### Synchronization Methods

| Method | Use Case | Analogy |
|--------|----------|---------|
| **Mutex** | Only one thread at a time | Bathroom key 🔑 |
| **Condvar** | Producer/Consumer pattern | Doorbell 🔔 |
| **Semaphore** | Limited concurrent access | Parking lot 🅿️ |
| **Barrier** | Wait for all threads | Family dinner 👨‍👩‍👧‍👦 |
| **RWLock** | Many readers, one writer | Library rules 📚 |

### 1. Mutex (Mutual Exclusion)

```
┌─────────────────────────────────────────────────────────────────┐
│                        MUTEX                                     │
│                                                                  │
│   ┌──────────┐         ┌──────────┐         ┌──────────┐       │
│   │ THREAD A │         │  MUTEX   │         │ THREAD B │       │
│   │          │         │   🔒     │         │          │       │
│   │  Lock()  │────────►│ LOCKED   │◄────────│  Lock()  │       │
│   │    ✓     │         │ (by A)   │         │    ✗     │       │
│   │          │         │          │         │  ⏳ WAIT │       │
│   │ Access   │         │          │         │          │       │
│   │ data     │         │          │         │          │       │
│   │          │         │          │         │          │       │
│   │ Unlock() │────────►│   🔓     │────────►│  Lock()  │       │
│   │          │         │ UNLOCKED │         │    ✓     │       │
│   └──────────┘         └──────────┘         └──────────┘       │
│                                                                  │
│   Only ONE thread can hold the mutex at a time!                 │
└─────────────────────────────────────────────────────────────────┘
```

### 2. Condition Variable (Condvar)

```
┌─────────────────────────────────────────────────────────────────┐
│                      CONDVAR                                     │
│                                                                  │
│   ┌──────────────┐      ┌─────────┐      ┌──────────────┐       │
│   │   PRODUCER   │      │ BUFFER  │      │   CONSUMER   │       │
│   │              │      │         │      │              │       │
│   │  Put data ───┼─────►│ [DATA]  │      │  ⏳ Wait()   │       │
│   │              │      │         │      │              │       │
│   │  Signal() ───┼──────┼─────────┼─────►│  Wake up!   │       │
│   │              │      │         │◄─────┼── Get data   │       │
│   └──────────────┘      └─────────┘      └──────────────┘       │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### 3. Semaphore

```
┌─────────────────────────────────────────────────────────────────┐
│                      SEMAPHORE (Count = 3)                      │
│                                                                  │
│   Thread A: sem_wait() → Count=2 ✓                              │
│   Thread B: sem_wait() → Count=1 ✓                              │
│   Thread C: sem_wait() → Count=0 ✓                              │
│   Thread D: sem_wait() → Count=0 ⏳ WAIT (no slots!)            │
│                                                                  │
│   When Thread A does sem_post() → Count=1 → Thread D enters!   │
└─────────────────────────────────────────────────────────────────┘
```

### 4. Barrier

```
┌─────────────────────────────────────────────────────────────────┐
│                        BARRIER                                   │
│                                                                  │
│   ┌────────┐ ┌────────┐ ┌────────┐ ┌────────┐ ┌────────┐       │
│   │Thread 1│ │Thread 2│ │Thread 3│ │Thread 4│ │Thread 5│       │
│   │  Done! │ │Working │ │  Done! │ │Working │ │  Done! │       │
│   │  WAIT  │ │   ↓    │ │  WAIT  │ │   ↓    │ │  WAIT  │       │
│   │        │ │  Done! │ │        │ │  Done! │ │        │       │
│   │        │ │  WAIT  │ │        │ │  WAIT  │ │        │       │
│   └────────┘ └────────┘ └────────┘ └────────┘ └────────┘       │
│                                                                  │
│   ═══════════════ BARRIER (All 5 arrived!) ═══════════════     │
│                                                                  │
│                   ALL THREADS CONTINUE! ▼                       │
└─────────────────────────────────────────────────────────────────┘
```

### 5. Read/Write Lock (RWLock)

```
┌─────────────────────────────────────────────────────────────────┐
│                       RWLOCK                                     │
│                                                                  │
│   READING MODE:                                                  │
│   ┌────────┐ ┌────────┐ ┌────────┐ ┌────────┐                  │
│   │Reader 1│ │Reader 2│ │Reader 3│ │Reader 4│                  │
│   │  READ ✓│ │  READ ✓│ │  READ ✓│ │  READ ✓│                  │
│   └────────┘ └────────┘ └────────┘ └────────┘                  │
│   Multiple readers allowed simultaneously! ✓                    │
│                                                                  │
│   WRITING MODE:                                                  │
│   ┌────────┐ ┌────────┐ ┌────────┐ ┌────────┐                  │
│   │Reader 1│ │Reader 2│ │ WRITER │ │Reader 3│                  │
│   │ ⏳WAIT │ │ ⏳WAIT │ │ WRITE ✓│ │ ⏳WAIT │                  │
│   └────────┘ └────────┘ └────────┘ └────────┘                  │
│   Only ONE writer, everyone else waits! ✗                       │
└─────────────────────────────────────────────────────────────────┘
```

---

## Time Services

### Two Types of Time Services

```
┌─────────────────────────────────────────────────────────────────┐
│                      TIME SERVICES                               │
│                                                                  │
│   ┌─────────────────────────────────────────────────────────┐   │
│   │                                                          │   │
│   │   TYPE 1: Access Time of Day                            │   │
│   │           "What time is it NOW?"                        │   │
│   │                                                          │   │
│   │   TYPE 2: Timers                                        │   │
│   │           "Tell me when 500ms has passed"               │   │
│   │                                                          │   │
│   └─────────────────────────────────────────────────────────┘   │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### Timer Use Cases

- **Timeout:** "If no response in 5 sec, abort"
- **Polling:** "Check buffer every 100ms"
- **Periodic tasks:** "Run this every 1 second"
- **Watchdog:** "If no heartbeat, something is wrong"

---

## Interrupt Handling

All hardware interrupts go through the microkernel, which provides two delivery methods:

```
┌─────────────────────────────────────────────────────────────────┐
│                   INTERRUPT HANDLING                             │
│                                                                  │
│                      HARDWARE                                    │
│   ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐       │
│   │ Keyboard │  │  Timer   │  │ Network  │  │   Disk   │       │
│   └────┬─────┘  └────┬─────┘  └────┬─────┘  └────┬─────┘       │
│        └─────────────┴─────────────┴─────────────┘               │
│                           │                                      │
│                           ▼                                      │
│              ┌─────────────────────────┐                        │
│              │      MICROKERNEL        │                        │
│              │   Interrupt Redirector  │                        │
│              └───────────┬─────────────┘                        │
│                          │                                       │
│           ┌──────────────┴──────────────┐                       │
│           ▼                             ▼                        │
│   ┌───────────────┐            ┌───────────────┐                │
│   │  METHOD 1:    │            │  METHOD 2:    │                │
│   │  IST (Direct) │            │ EVENT(Indirect│                │
│   └───────────────┘            └───────────────┘                │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### Method Comparison

| Aspect | Method 1 (IST) | Method 2 (EVENT) |
|--------|----------------|------------------|
| Delivery | Direct | Indirect |
| Speed | Faster | Slightly slower |
| How | Thread registers as IST | Microkernel IST delivers event |

> **Note:** Even with interrupt handling, threads must have appropriate priority and scheduling configured!

---

## When Does the Microkernel Run?

The microkernel runs in **three scenarios**:

```
┌─────────────────────────────────────────────────────────────────┐
│              WHEN DOES MICROKERNEL RUN?                         │
│                                                                  │
│   1. KERNEL CALL       Application explicitly calls it          │
│      └─► MsgSend(), ThreadCreate(), etc.                        │
│                                                                  │
│   2. INTERRUPT         Hardware triggers it                     │
│      └─► Keyboard press, timer tick, network packet             │
│                                                                  │
│   3. FAULT/EXCEPTION   Processor encounters error               │
│      └─► Divide by zero, segmentation fault, illegal instruction│
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### Common Faults

| Fault | Description |
|-------|-------------|
| Divide by Zero | `x / 0` |
| Segmentation Fault | Accessing memory you don't own |
| Illegal Instruction | CPU doesn't understand the instruction |
| Bus Error | Memory alignment problem |

---

## Where Does the Microkernel Run?

The microkernel runs on the **core where the event happened**:

```
┌─────────────────────────────────────────────────────────────────┐
│                    MULTI-CORE CPU                               │
│                                                                  │
│   ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐       │
│   │  CORE 0  │  │  CORE 1  │  │  CORE 2  │  │  CORE 3  │       │
│   │          │  │          │  │          │  │          │       │
│   │  App A   │  │  App B   │  │  App C   │  │  App D   │       │
│   │   │      │  │          │  │          │  │          │       │
│   │   ▼      │  │          │  │          │  │          │       │
│   │ Kernel   │  │          │  │          │  │          │       │
│   │ Call!    │  │          │  │          │  │          │       │
│   │   │      │  │          │  │          │  │          │       │
│   │   ▼      │  │          │  │          │  │          │       │
│   │┌────────┐│  │          │  │          │  │          │       │
│   ││MICRO-  ││  │          │  │          │  │          │       │
│   ││KERNEL  ││  │          │  │          │  │          │       │
│   │└────────┘│  │          │  │          │  │          │       │
│   └──────────┘  └──────────┘  └──────────┘  └──────────┘       │
│                                                                  │
│   Microkernel runs on CORE 0 (where kernel call was made)      │
└─────────────────────────────────────────────────────────────────┘
```

### Rules

- Kernel Call on Core 2? → Microkernel runs on Core 2
- Interrupt directed to Core 0? → Microkernel runs on Core 0
- Fault on Core 3? → Microkernel runs on Core 3

> **The microkernel is NOT on a dedicated core - it runs wherever needed!**

---

## Summary

```
┌─────────────────────────────────────────────────────────────────┐
│                  MICROKERNEL SUMMARY                             │
│                                                                  │
│  ┌────────────────────────────────────────────────────────────┐ │
│  │  WHAT IS IT?                                                │ │
│  │  Heart of QNX - handles interrupts & IPC                   │ │
│  └────────────────────────────────────────────────────────────┘ │
│                                                                  │
│  ┌────────────────────────────────────────────────────────────┐ │
│  │  HOW TO TALK TO IT?                                        │ │
│  │  Kernel Calls (CamelCase): MsgSend(), ThreadCreate()       │ │
│  └────────────────────────────────────────────────────────────┘ │
│                                                                  │
│  ┌────────────────────────────────────────────────────────────┐ │
│  │  DEDICATED THREADS (per core)                               │ │
│  │  1. IPI IST     - cores communicate                        │ │
│  │  2. Timer IST   - timer management                         │ │
│  │  3. Idle Thread - runs when nothing to do                  │ │
│  └────────────────────────────────────────────────────────────┘ │
│                                                                  │
│  ┌────────────────────────────────────────────────────────────┐ │
│  │  SERVICES PROVIDED                                          │ │
│  │  • IPC (Messages, Pulses, Signals, Message Queues)         │ │
│  │  • Thread management & Synchronization                     │ │
│  │  • Time services & Timers                                  │ │
│  │  • Interrupt handling                                      │ │
│  │  • Fault handling                                          │ │
│  └────────────────────────────────────────────────────────────┘ │
│                                                                  │
│  ┌────────────────────────────────────────────────────────────┐ │
│  │  WHEN DOES IT RUN?                                          │ │
│  │  1. Kernel Call    - app requests service                  │ │
│  │  2. Interrupt      - hardware triggers                     │ │
│  │  3. Fault          - error occurs                          │ │
│  └────────────────────────────────────────────────────────────┘ │
│                                                                  │
│  ┌────────────────────────────────────────────────────────────┐ │
│  │  WHERE DOES IT RUN?                                         │ │
│  │  On the core where the event happened                      │ │
│  └────────────────────────────────────────────────────────────┘ │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
