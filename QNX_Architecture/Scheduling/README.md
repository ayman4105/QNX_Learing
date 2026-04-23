
# QNX Scheduling - Complete Guide

## Table of Contents
- [Introduction](#introduction)
- [Thread States](#thread-states)
- [Priority System](#priority-system)
- [Preemptive Scheduling](#preemptive-scheduling)
- [Multicore Systems](#multicore-systems)
- [Cluster-Based Scheduling](#cluster-based-scheduling)
- [Scheduling Decision Flow](#scheduling-decision-flow)
- [Scheduling Algorithms](#scheduling-algorithms)
- [Core Affinity](#core-affinity)
- [QNX vs Linux Scheduling](#qnx-vs-linux-scheduling)
- [Design Best Practices](#design-best-practices)
- [Frequently Asked Questions](#frequently-asked-questions)
- [Summary](#summary)

---

## Introduction

QNX uses **preemptive priority-based scheduling** designed for **Real-Time Systems**.

### The Most Important Rule

```
┌─────────────────────────────────────────────────────────────────┐
│                                                                  │
│           QNX SCHEDULES THREADS, NOT PROCESSES!                 │
│                                                                  │
│   ┌─────────────────────────────────────────────────────────┐   │
│   │                                                          │   │
│   │   ✗ Processes do NOT have priority                      │   │
│   │   ✓ THREADS have priority                               │   │
│   │                                                          │   │
│   │   ┌─────────────────────────────────────────────────┐   │   │
│   │   │                 PROCESS A                        │   │   │
│   │   │                                                  │   │   │
│   │   │   ┌──────────┐ ┌──────────┐ ┌──────────┐        │   │   │
│   │   │   │ Thread 1 │ │ Thread 2 │ │ Thread 3 │        │   │   │
│   │   │   │ Pri: 20  │ │ Pri: 15  │ │ Pri: 30  │        │   │   │
│   │   │   └──────────┘ └──────────┘ └──────────┘        │   │   │
│   │   │                                                  │   │   │
│   │   │   Each thread scheduled INDEPENDENTLY!           │   │   │
│   │   └─────────────────────────────────────────────────┘   │   │
│   │                                                          │   │
│   │   When you say "launch process at priority 20"          │   │
│   │   You mean: "launch FIRST THREAD at priority 20"        │   │
│   │                                                          │   │
│   └─────────────────────────────────────────────────────────┘   │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

---

## Thread States

### The Big Picture

```
┌─────────────────────────────────────────────────────────────────┐
│                       THREAD STATES                              │
│                                                                  │
│   ┌─────────────────────────────────────────────────────────┐   │
│   │                      BLOCKED                             │   │
│   │                                                          │   │
│   │   • RECEIVE (waiting for message)                       │   │
│   │   • SEND (waiting to send)                              │   │
│   │   • REPLY (waiting for reply)                           │   │
│   │   • MUTEX (waiting for lock)                            │   │
│   │   • CONDVAR (waiting for signal)                        │   │
│   │   • SEM (waiting for semaphore)                         │   │
│   │   • STOPPED (externally stopped)                        │   │
│   │                                                          │   │
│   │   ════════════════════════════════════════════════════  │   │
│   │   ║  SCHEDULER IGNORES ALL BLOCKED THREADS!          ║  │   │
│   │   ║  They CANNOT be chosen to run on ANY core!       ║  │   │
│   │   ════════════════════════════════════════════════════  │   │
│   │                                                          │   │
│   └─────────────────────────┬───────────────────────────────┘   │
│                             │                                    │
│                        Event happens                             │
│                             │                                    │
│                             ▼                                    │
│   ┌─────────────────────────────────────────────────────────┐   │
│   │                     RUNNABLE                             │   │
│   │                                                          │   │
│   │   ┌─────────────────────┐   ┌─────────────────────┐     │   │
│   │   │      RUNNING        │   │       READY         │     │   │
│   │   │                     │   │                     │     │   │
│   │   │  Actually using     │   │  Wants to run but   │     │   │
│   │   │  CPU RIGHT NOW!     │◄─►│  waiting for turn   │     │   │
│   │   │                     │   │                     │     │   │
│   │   │  # = CPU Cores      │   │  # = Unlimited!     │     │   │
│   │   │  (4 cores = 4 max)  │   │                     │     │   │
│   │   └─────────────────────┘   └─────────────────────┘     │   │
│   │                                                          │   │
│   └─────────────────────────────────────────────────────────┘   │
│                             │                                    │
│                        Terminates                                │
│                             │                                    │
│                             ▼                                    │
│   ┌─────────────────────────────────────────────────────────┐   │
│   │                       DEAD                               │   │
│   │                                                          │   │
│   │   Terminated but not fully cleaned up                   │   │
│   │   Can NEVER run again!                                  │   │
│   │                                                          │   │
│   └─────────────────────────────────────────────────────────┘   │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### Quick Reference Table

| State | Description | Scheduled? |
|-------|-------------|------------|
| BLOCKED | Waiting for something | ❌ NO |
| READY | Wants to run, waiting for turn | ✅ YES |
| RUNNING | Actually using CPU | ✅ YES (chosen) |
| DEAD | Terminated | ❌ NO |

---

## Priority System

### Priority Range (0-255)

```
┌─────────────────────────────────────────────────────────────────┐
│                     PRIORITY RANGE                               │
│                                                                  │
│        ┌──────────────────────────────────────────────┐         │
│   255  │▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓│ IPI IST │
│        │           RESERVED FOR KERNEL                │         │
│   254  │▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓│ Timer   │
│        ├──────────────────────────────────────────────┤         │
│   253  │                                              │         │
│        │                                              │         │
│    ▲   │                                              │         │
│    │   │      U S E R   S P A C E   T H R E A D S    │         │
│    │   │                                              │         │
│  HIGH  │      Your applications run here!            │         │
│    │   │                                              │         │
│    │   │      Higher number = More important         │         │
│    ▼   │                                              │         │
│        │                                              │         │
│    1   │                                              │         │
│        ├──────────────────────────────────────────────┤         │
│    0   │▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓│ Idle    │
│        │           RESERVED FOR KERNEL                │ Thread  │
│        └──────────────────────────────────────────────┘         │
│                                                                  │
│   RESERVED PRIORITIES:                                          │
│   ┌────────────────────────────────────────────────────────┐    │
│   │  255: IPI IST - Cores talking to each other           │    │
│   │  254: Timer IST + InterruptAttachEvent handlers       │    │
│   │    0: Idle Thread - Runs when nothing else can        │    │
│   └────────────────────────────────────────────────────────┘    │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

---

## Preemptive Scheduling

### The Golden Rule

```
┌─────────────────────────────────────────────────────────────────┐
│                                                                  │
│              ╔═══════════════════════════════════╗              │
│              ║   HIGHER PRIORITY ALWAYS WINS!    ║              │
│              ╚═══════════════════════════════════╝              │
│                                                                  │
│   ┌─────────────────────────────────────────────────────────┐   │
│   │                                                          │   │
│   │   Thread A (Priority 50)        Thread B (Priority 30)  │   │
│   │                                                          │   │
│   │   ████████████████████████      ░░░░░░░░░░░░░░░░░░░░░░ │   │
│   │        100% CPU                       0% CPU            │   │
│   │        RUNNING                        WAITING           │   │
│   │                                                          │   │
│   │   ═══════════════════════════════════════════════════   │   │
│   │                                                          │   │
│   │   This is NOT fair share!                               │   │
│   │   This is NOT 60% vs 40%!                               │   │
│   │   This is ABSOLUTE: Higher runs, Lower waits!           │   │
│   │                                                          │   │
│   │   Priority 51 preempts Priority 50 = 100%               │   │
│   │   Priority 200 preempts Priority 10 = 100%              │   │
│   │                                                          │   │
│   │   Doesn't matter how big the gap!                       │   │
│   │                                                          │   │
│   └─────────────────────────────────────────────────────────┘   │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### How Threads Share CPU (The Right Way)

```
┌─────────────────────────────────────────────────────────────────┐
│               HOW THREADS SHARE CPU TIME                        │
│                                                                  │
│   GOOD DESIGN ✓ (Block when no work)                           │
│   ┌─────────────────────────────────────────────────────────┐   │
│   │                                                          │   │
│   │   while(1) {                                            │   │
│   │       msg = MsgReceive(...);  ←── BLOCK here!          │   │
│   │       process(msg);           ←── Quick work            │   │
│   │       MsgReply(...);          ←── Reply                 │   │
│   │   }                                                     │   │
│   │                                                          │   │
│   │   Timeline:                                             │   │
│   │   ┌───────────────────────────────────────────────────┐ │   │
│   │   │BLOCK░░░░│RUN██│BLOCK░░░░│RUN██│BLOCK░░░░│RUN██│  │ │   │
│   │   │   95%   │ 5% │   95%   │ 5% │   95%   │ 5% │     │ │   │
│   │   └───────────────────────────────────────────────────┘ │   │
│   │                                                          │   │
│   │   While BLOCKED → Other threads run! ✓                  │   │
│   │                                                          │   │
│   └─────────────────────────────────────────────────────────┘   │
│                                                                  │
│   BAD DESIGN ✗ (Polling)                                       │
│   ┌─────────────────────────────────────────────────────────┐   │
│   │                                                          │   │
│   │   while(1) {                                            │   │
│   │       if(check_for_data()) {  ←── POLLING! Never blocks│   │
│   │           process(data);                                │   │
│   │       }                                                 │   │
│   │   }                                                     │   │
│   │                                                          │   │
│   │   Timeline:                                             │   │
│   │   ┌───────────────────────────────────────────────────┐ │   │
│   │   │RUN████████████████████████████████████████████████│ │   │
│   │   │                      100%                         │ │   │
│   │   └───────────────────────────────────────────────────┘ │   │
│   │                                                          │   │
│   │   Burns CPU! Starves other threads! ✗                  │   │
│   │                                                          │   │
│   └─────────────────────────────────────────────────────────┘   │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

---

## Multicore Systems

### Basic Multicore Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                    MULTICORE SYSTEM                             │
│                                                                  │
│   ┌─────────────────────────────────────────────────────────┐   │
│   │                                                          │   │
│   │   ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐      │   │
│   │   │ CORE 0  │ │ CORE 1  │ │ CORE 2  │ │ CORE 3  │      │   │
│   │   │         │ │         │ │         │ │         │      │   │
│   │   │Thread A │ │Thread B │ │Thread C │ │Thread D │      │   │
│   │   └────┬────┘ └────┬────┘ └────┬────┘ └────┬────┘      │   │
│   │        │           │           │           │            │   │
│   │        └───────────┴─────┬─────┴───────────┘            │   │
│   │                          │                               │   │
│   │   ┌──────────────────────┴──────────────────────────┐   │   │
│   │   │              SHARED RESOURCES                    │   │   │
│   │   │   [RAM] [Bus] [Network] [Disk] [Devices]        │   │   │
│   │   └─────────────────────────────────────────────────┘   │   │
│   │                                                          │   │
│   └─────────────────────────────────────────────────────────┘   │
│                                                                  │
│   • Each core executes INDEPENDENTLY                            │
│   • All cores SHARE hardware                                    │
│   • 4 cores = 4 threads can run SIMULTANEOUSLY!                │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### Cores Can Be Different!

```
┌─────────────────────────────────────────────────────────────────┐
│                CORES ARE NOT ALL THE SAME!                      │
│                                                                  │
│   CASE 1: Shared Cache Groups                                   │
│   ┌─────────────────────────────────────────────────────────┐   │
│   │                                                          │   │
│   │   ┌─────────────────────┐   ┌─────────────────────┐     │   │
│   │   │    CACHE GROUP A    │   │    CACHE GROUP B    │     │   │
│   │   │  ┌──────┐ ┌──────┐  │   │  ┌──────┐ ┌──────┐  │     │   │
│   │   │  │Core 0│ │Core 1│  │   │  │Core 2│ │Core 3│  │     │   │
│   │   │  └──┬───┘ └──┬───┘  │   │  └──┬───┘ └──┬───┘  │     │   │
│   │   │     └───┬────┘      │   │     └───┬────┘      │     │   │
│   │   │    ┌────┴────┐      │   │    ┌────┴────┐      │     │   │
│   │   │    │L3 CACHE │      │   │    │L3 CACHE │      │     │   │
│   │   │    └─────────┘      │   │    └─────────┘      │     │   │
│   │   └─────────────────────┘   └─────────────────────┘     │   │
│   │                                                          │   │
│   │   Core 0 ↔ Core 1 = FAST (same L3)                      │   │
│   │   Core 0 ↔ Core 2 = SLOW (different L3)                 │   │
│   │                                                          │   │
│   └─────────────────────────────────────────────────────────┘   │
│                                                                  │
│   CASE 2: big.LITTLE Architecture                              │
│   ┌─────────────────────────────────────────────────────────┐   │
│   │                                                          │   │
│   │   ┌─────────────────────┐   ┌─────────────────────┐     │   │
│   │   │     BIG CORES       │   │    LITTLE CORES     │     │   │
│   │   │    (High Power)     │   │    (Low Power)      │     │   │
│   │   │  ┌──────┐ ┌──────┐  │   │  ┌──────┐ ┌──────┐  │     │   │
│   │   │  │Core 0│ │Core 1│  │   │  │Core 2│ │Core 3│  │     │   │
│   │   │  │ 🔥🔥 │ │ 🔥🔥 │  │   │  │ 🔋  │ │ 🔋  │  │     │   │
│   │   │  │ FAST │ │ FAST │  │   │  │ slow │ │ slow │  │     │   │
│   │   │  └──────┘ └──────┘  │   │  └──────┘ └──────┘  │     │   │
│   │   └─────────────────────┘   └─────────────────────┘     │   │
│   │                                                          │   │
│   │   Big = Fast but uses lots of power                     │   │
│   │   Little = Slow but saves battery                       │   │
│   │                                                          │   │
│   └─────────────────────────────────────────────────────────┘   │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### SMP (Symmetrical Multiprocessing)

```
┌─────────────────────────────────────────────────────────────────┐
│                          SMP                                     │
│            (Symmetrical Multiprocessing)                        │
│                                                                  │
│   ┌─────────────────────────────────────────────────────────┐   │
│   │                                                          │   │
│   │   All cores are IDENTICAL and INTERCHANGEABLE           │   │
│   │                                                          │   │
│   │   ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐      │   │
│   │   │ CORE 0  │ │ CORE 1  │ │ CORE 2  │ │ CORE 3  │      │   │
│   │   │    =    │ │    =    │ │    =    │ │    =    │      │   │
│   │   │  SAME   │ │  SAME   │ │  SAME   │ │  SAME   │      │   │
│   │   └─────────┘ └─────────┘ └─────────┘ └─────────┘      │   │
│   │                                                          │   │
│   │   Kernel: "Any thread can run on any core"              │   │
│   │                                                          │   │
│   └─────────────────────────────────────────────────────────┘   │
│                                                                  │
│   QNX Kernel name: procnto-smp                                  │
│                           ───                                   │
│                            └── SMP = Symmetrical MultiProc      │
│                                                                  │
│   By DEFAULT, QNX treats ALL systems as SMP                    │
│   (even if cores are actually different!)                       │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

---

## Cluster-Based Scheduling

### What is a Cluster?

```
┌─────────────────────────────────────────────────────────────────┐
│                    WHAT IS A CLUSTER?                           │
│                                                                  │
│   ╔═════════════════════════════════════════════════════════╗   │
│   ║  CLUSTER = A set of related CPU cores                   ║   │
│   ║                                                          ║   │
│   ║  Think of it as a FENCE around certain cores!           ║   │
│   ║  Threads inside can only see cores inside the fence.    ║   │
│   ╚═════════════════════════════════════════════════════════╝   │
│                                                                  │
│   ┌─────────────────────────────────────────────────────────┐   │
│   │                                                          │   │
│   │   ┌───────────────────────┐ ┌───────────────────────┐   │   │
│   │   │ ╔═══════════════════╗ │ │ ╔═══════════════════╗ │   │   │
│   │   │ ║    CLUSTER A      ║ │ │ ║    CLUSTER B      ║ │   │   │
│   │   │ ║  ┌──────┐┌──────┐ ║ │ │ ║  ┌──────┐┌──────┐ ║ │   │   │
│   │   │ ║  │Core 0││Core 1│ ║ │ │ ║  │Core 2││Core 3│ ║ │   │   │
│   │   │ ║  └──────┘└──────┘ ║ │ │ ║  └──────┘└──────┘ ║ │   │   │
│   │   │ ║                   ║ │ │ ║                   ║ │   │   │
│   │   │ ║  Thread X here    ║ │ │ ║  Thread Y here    ║ │   │   │
│   │   │ ║  can ONLY run on  ║ │ │ ║  can ONLY run on  ║ │   │   │
│   │   │ ║  Core 0 or 1!     ║ │ │ ║  Core 2 or 3!     ║ │   │   │
│   │   │ ╚═══════════════════╝ │ │ ╚═══════════════════╝ │   │   │
│   │   └───────────────────────┘ └───────────────────────┘   │   │
│   │                                                          │   │
│   │   Thread X CANNOT run on Core 2 or 3!                   │   │
│   │   Thread Y CANNOT run on Core 0 or 1!                   │   │
│   │                                                          │   │
│   └─────────────────────────────────────────────────────────┘   │
│                                                                  │
│   CLUSTER DETERMINES:                                           │
│   • Which cores a thread CAN run on                            │
│   • Which cores a thread CAN preempt                           │
│   • Which ready list the thread is on                          │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### Default Clusters (Always Exist)

```
┌─────────────────────────────────────────────────────────────────┐
│                   DEFAULT CLUSTERS                               │
│           (Always exist in EVERY QNX system)                    │
│                                                                  │
│   1️⃣ GLOBAL CLUSTER (Contains ALL cores)                        │
│   ┌─────────────────────────────────────────────────────────┐   │
│   │                                                          │   │
│   │   ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐      │   │
│   │   │ Core 0  │ │ Core 1  │ │ Core 2  │ │ Core 3  │      │   │
│   │   └─────────┘ └─────────┘ └─────────┘ └─────────┘      │   │
│   │                                                          │   │
│   │   Thread here → Can run on ANY core                     │   │
│   │                                                          │   │
│   └─────────────────────────────────────────────────────────┘   │
│                                                                  │
│   2️⃣ INDIVIDUAL CORE CLUSTERS (One per core)                    │
│   ┌─────────────────────────────────────────────────────────┐   │
│   │                                                          │   │
│   │   ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐      │   │
│   │   │Cluster 0│ │Cluster 1│ │Cluster 2│ │Cluster 3│      │   │
│   │   │┌───────┐│ │┌───────┐│ │┌───────┐│ │┌───────┐│      │   │
│   │   ││Core 0 ││ ││Core 1 ││ ││Core 2 ││ ││Core 3 ││      │   │
│   │   │└───────┘│ │└───────┘│ │└───────┘│ │└───────┘│      │   │
│   │   └─────────┘ └─────────┘ └─────────┘ └─────────┘      │   │
│   │                                                          │   │
│   │   Used by: Idle Thread, IPI Thread, Timer Thread        │   │
│   │   (Must run on THAT specific core only)                 │   │
│   │                                                          │   │
│   └─────────────────────────────────────────────────────────┘   │
│                                                                  │
│   ╔═════════════════════════════════════════════════════════╗   │
│   ║  RULE: Every core is member of AT LEAST 2 clusters:    ║   │
│   ║        1. Global Cluster                                 ║   │
│   ║        2. Its Individual Cluster                         ║   │
│   ╚═════════════════════════════════════════════════════════╝   │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### Can a Core Be in Multiple Clusters? YES!

```
┌─────────────────────────────────────────────────────────────────┐
│            CORE CAN BE IN MULTIPLE CLUSTERS!                    │
│                                                                  │
│   Example: Core 2 is in 4 different clusters                   │
│                                                                  │
│   ┌─────────────────────────────────────────────────────────┐   │
│   │                                                          │   │
│   │   GLOBAL CLUSTER                                        │   │
│   │   ┌──────┐ ┌──────┐ ┌══════┐ ┌──────┐                  │   │
│   │   │Core 0│ │Core 1│ ║Core 2║ │Core 3│  ← Core 2 ✓     │   │
│   │   └──────┘ └──────┘ └══════┘ └──────┘                  │   │
│   │                                                          │   │
│   │   ──────────────────────────────────────────────────    │   │
│   │                                                          │   │
│   │   INDIVIDUAL CLUSTER FOR CORE 2                         │   │
│   │               ┌══════┐                                   │   │
│   │               ║Core 2║  ← Core 2 ✓                      │   │
│   │               └══════┘                                   │   │
│   │                                                          │   │
│   │   ──────────────────────────────────────────────────    │   │
│   │                                                          │   │
│   │   CUSTOM CLUSTER A (0x7 = Cores 0,1,2)                  │   │
│   │   ┌──────┐ ┌──────┐ ┌══════┐                            │   │
│   │   │Core 0│ │Core 1│ ║Core 2║  ← Core 2 ✓               │   │
│   │   └──────┘ └──────┘ └══════┘                            │   │
│   │                                                          │   │
│   │   ──────────────────────────────────────────────────    │   │
│   │                                                          │   │
│   │   CUSTOM CLUSTER B (0x6 = Cores 1,2)                    │   │
│   │            ┌──────┐ ┌══════┐                             │   │
│   │            │Core 1│ ║Core 2║  ← Core 2 ✓                │   │
│   │            └──────┘ └══════┘                             │   │
│   │                                                          │   │
│   └─────────────────────────────────────────────────────────┘   │
│                                                                  │
│   Core 2 is member of 4 clusters!                              │
│   Maximum allowed: 8 clusters per core                         │
│   (2 default + 6 custom max)                                   │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### Adding Custom Clusters (At Boot Time)

```
┌─────────────────────────────────────────────────────────────────┐
│                  DEFINING CUSTOM CLUSTERS                       │
│                                                                  │
│   Done in STARTUP using -c option:                             │
│                                                                  │
│   ┌─────────────────────────────────────────────────────────┐   │
│   │                                                          │   │
│   │   startup-boardname -c cluster0:0x7,cluster1:0x9        │   │
│   │                         │       │   │       │           │   │
│   │                       name    mask name    mask         │   │
│   │                                                          │   │
│   └─────────────────────────────────────────────────────────┘   │
│                                                                  │
│   HOW BITMASK WORKS:                                            │
│   ┌─────────────────────────────────────────────────────────┐   │
│   │                                                          │   │
│   │   0x7 = Binary 0111           0x9 = Binary 1001         │   │
│   │              ││││                          ││││         │   │
│   │   Bit 0 = 1 ─┘│││ → Core 0 ✓    Bit 0 = 1 ─┘│││ → ✓    │   │
│   │   Bit 1 = 1 ──┘││ → Core 1 ✓    Bit 1 = 0 ──┘││ → ✗    │   │
│   │   Bit 2 = 1 ───┘│ → Core 2 ✓    Bit 2 = 0 ───┘│ → ✗    │   │
│   │   Bit 3 = 0 ────┘ → Core 3 ✗    Bit 3 = 1 ────┘ → ✓    │   │
│   │                                                          │   │
│   │   cluster0 = {Core 0, 1, 2}   cluster1 = {Core 0, 3}   │   │
│   │                                                          │   │
│   └─────────────────────────────────────────────────────────┘   │
│                                                                  │
│   COMMON BITMASKS:                                              │
│   ┌───────────┬──────────┬────────────────────────────────┐    │
│   │   Hex     │  Binary  │  Cores                         │    │
│   ├───────────┼──────────┼────────────────────────────────┤    │
│   │   0x1     │   0001   │  Core 0 only                   │    │
│   │   0x3     │   0011   │  Core 0, 1                     │    │
│   │   0x7     │   0111   │  Core 0, 1, 2                  │    │
│   │   0x9     │   1001   │  Core 0, 3                     │    │
│   │   0xF     │   1111   │  Core 0, 1, 2, 3               │    │
│   │   0xFF    │ 11111111 │  Core 0-7 (all 8 cores)        │    │
│   └───────────┴──────────┴────────────────────────────────┘    │
│                                                                  │
│   RULES:                                                        │
│   • Clusters must be UNIQUE (no duplicate sets)                │
│   • Each core: MAX 8 clusters                                  │
│   • Exceed 8 → BOOT FAILS!                                     │
│   • Clusters FIXED at boot (can't change at runtime)           │
│                                                                  │
│   View clusters: pidin syspage=cluster                         │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### Why Clusters? Real-Time Efficiency!

```
┌─────────────────────────────────────────────────────────────────┐
│                  WHY USE CLUSTERS?                              │
│                                                                  │
│   PROBLEM: Global Queue is SLOW with core affinity             │
│   ┌─────────────────────────────────────────────────────────┐   │
│   │                                                          │   │
│   │   Ready Queue: [T1][T2][T3][T4]...[T80]                 │   │
│   │                  │                                       │   │
│   │   Core 4 needs to schedule...                           │   │
│   │   Check T1 - Can't run on Core 4! ✗                    │   │
│   │   Check T2 - Can't run on Core 4! ✗                    │   │
│   │   Check T3 - Can't run on Core 4! ✗                    │   │
│   │   ... (check all 80 threads!)                           │   │
│   │   Check T80 - Can run! ✓                               │   │
│   │                                                          │   │
│   │   WORST CASE: Check ALL threads!                        │   │
│   │   More threads = SLOWER scheduling! 😱                  │   │
│   │   BAD for Real-Time!                                    │   │
│   │                                                          │   │
│   └─────────────────────────────────────────────────────────┘   │
│                                                                  │
│   SOLUTION: Cluster-based scheduling                           │
│   ┌─────────────────────────────────────────────────────────┐   │
│   │                                                          │   │
│   │   Cluster A Ready: [T1][T5]                             │   │
│   │   Cluster B Ready: [T2][T3]                             │   │
│   │                                                          │   │
│   │   Core 4 (in Cluster B) needs to schedule...            │   │
│   │   Check ONLY Cluster B list!                            │   │
│   │   Pick T2! ✓                                            │   │
│   │                                                          │   │
│   │   FIXED COST! Doesn't matter how many threads!          │   │
│   │   GREAT for Real-Time! ✓                                │   │
│   │                                                          │   │
│   └─────────────────────────────────────────────────────────┘   │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

---

## Scheduling Decision Flow

### When Thread Becomes Ready

```
┌─────────────────────────────────────────────────────────────────┐
│         SCHEDULING FLOW: Thread Becomes Ready                   │
│                                                                  │
│   ┌─────────────────────────────────────────────────────────┐   │
│   │                                                          │   │
│   │   Thread X becomes READY (e.g., sem_post() woke it up)  │   │
│   │              │                                           │   │
│   │              ▼                                           │   │
│   │   Add X to its cluster's ready list                     │   │
│   │              │                                           │   │
│   │              ▼                                           │   │
│   │   Is X the MOST ELIGIBLE in its cluster?                │   │
│   │   (Highest priority + Oldest timestamp)                 │   │
│   │              │                                           │   │
│   │         NO ──┼──► Done! Wait in ready list.             │   │
│   │              │                                           │   │
│   │              ▼ YES                                       │   │
│   │   ┌──────────────────────────────────────────────────┐  │   │
│   │   │                                                   │  │   │
│   │   │   STEP 1: Can preempt LOCAL core?                │  │   │
│   │   │           (Core where event happened)            │  │   │
│   │   │              │                                    │  │   │
│   │   │         YES ─┼──► LOCAL PREEMPTION! ✓            │  │   │
│   │   │              │    (Fastest - no IPI!)            │  │   │
│   │   │              ▼ NO                                 │  │   │
│   │   │   STEP 2: IDLE core in X's cluster?              │  │   │
│   │   │              │                                    │  │   │
│   │   │         YES ─┼──► IPI to idle core! ✓            │  │   │
│   │   │              │    (Don't interrupt work)         │  │   │
│   │   │              ▼ NO                                 │  │   │
│   │   │   STEP 3: Can preempt OTHER core in cluster?     │  │   │
│   │   │           (Find lowest priority thread)          │  │   │
│   │   │              │                                    │  │   │
│   │   │         YES ─┼──► IPI to that core! ✓            │  │   │
│   │   │              │                                    │  │   │
│   │   │              ▼ NO                                 │  │   │
│   │   │   Wait in ready list.                            │  │   │
│   │   │                                                   │  │   │
│   │   └──────────────────────────────────────────────────┘  │   │
│   │                                                          │   │
│   └─────────────────────────────────────────────────────────┘   │
│                                                                  │
│   WHY THIS ORDER?                                               │
│   1. Local = Fastest (no IPI overhead)                         │
│   2. Idle = Don't interrupt useful work                        │
│   3. Other = Last resort (interrupt lowest priority)           │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### Complete Example with Clusters

```
┌─────────────────────────────────────────────────────────────────┐
│            PREEMPTION EXAMPLE WITH CLUSTERS                     │
│                                                                  │
│   SETUP:                                                        │
│   • Cluster A = Core 0, Core 1                                 │
│   • Cluster B = Core 2, Core 3                                 │
│                                                                  │
│   BEFORE:                                                       │
│   ┌─────────────────────────────────────────────────────────┐   │
│   │                                                          │   │
│   │      CLUSTER A             │       CLUSTER B            │   │
│   │   ┌─────────┐┌─────────┐   │   ┌─────────┐┌─────────┐  │   │
│   │   │ Core 0  ││ Core 1  │   │   │ Core 2  ││ Core 3  │  │   │
│   │   │   T1    ││   T2    │   │   │   T3    ││  IDLE   │  │   │
│   │   │ Pri:50  ││ Pri:40  │   │   │ Pri:20  ││ Pri:0   │  │   │
│   │   └─────────┘└─────────┘   │   └─────────┘└─────────┘  │   │
│   │                            │                            │   │
│   └─────────────────────────────────────────────────────────┘   │
│                                                                  │
│   EVENT: T7 (Pri:45, in CLUSTER A) becomes ready!              │
│                                                                  │
│   DECISION:                                                     │
│   ┌─────────────────────────────────────────────────────────┐   │
│   │                                                          │   │
│   │   Q: Is there IDLE core in Cluster A?                   │   │
│   │                                                          │   │
│   │      Core 0: Running T1 → NOT idle                      │   │
│   │      Core 1: Running T2 → NOT idle                      │   │
│   │      Core 2: ❌ NOT IN CLUSTER A!                       │   │
│   │      Core 3: ❌ NOT IN CLUSTER A! (even though IDLE!)   │   │
│   │                                                          │   │
│   │   A: NO idle core in Cluster A!                         │   │
│   │                                                          │   │
│   │   ════════════════════════════════════════════════════  │   │
│   │                                                          │   │
│   │   Q: Can preempt another core in Cluster A?             │   │
│   │                                                          │   │
│   │      Core 0: T1 (Pri:50) > T7 (Pri:45) → Can't preempt │   │
│   │      Core 1: T2 (Pri:40) < T7 (Pri:45) → CAN preempt! ✓│   │
│   │                                                          │   │
│   │   A: IPI to Core 1: "Preempt T2, run T7!"              │   │
│   │                                                          │   │
│   └─────────────────────────────────────────────────────────┘   │
│                                                                  │
│   AFTER:                                                        │
│   ┌─────────────────────────────────────────────────────────┐   │
│   │                                                          │   │
│   │      CLUSTER A             │       CLUSTER B            │   │
│   │   ┌─────────┐┌─────────┐   │   ┌─────────┐┌─────────┐  │   │
│   │   │ Core 0  ││ Core 1  │   │   │ Core 2  ││ Core 3  │  │   │
│   │   │   T1    ││   T7    │   │   │   T3    ││  IDLE   │  │   │
│   │   │ Pri:50  ││ Pri:45  │   │   │ Pri:20  ││ Pri:0   │  │   │
│   │   └─────────┘└─────────┘   │   └─────────┘└─────────┘  │   │
│   │       │          ▲         │       │          ▲        │   │
│   │   No change   T7 here!     │   No change  Still IDLE!  │   │
│   │                            │                            │   │
│   │   Ready A: [T2 Pri:40]     │   Ready B: (empty)        │   │
│   │             ▲              │                            │   │
│   │        T2 preempted        │                            │   │
│   │                            │                            │   │
│   └─────────────────────────────────────────────────────────┘   │
│                                                                  │
│   ⚠️  IMPORTANT: Core 3 is IDLE but T7 CAN'T use it!           │
│      T7 is in Cluster A, Core 3 is in Cluster B!               │
│      CLUSTER BOUNDARIES ARE ABSOLUTE!                           │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### What If T7 Was in Global Cluster?

```
┌─────────────────────────────────────────────────────────────────┐
│        IF T7 WAS IN GLOBAL CLUSTER INSTEAD...                   │
│                                                                  │
│   DECISION:                                                     │
│   ┌─────────────────────────────────────────────────────────┐   │
│   │                                                          │   │
│   │   Q: Is there IDLE core in Global Cluster?              │   │
│   │                                                          │   │
│   │      Core 3 is IDLE! ✓                                  │   │
│   │                                                          │   │
│   │   A: IPI to Core 3: "Run T7!"                          │   │
│   │      (No preemption needed!)                            │   │
│   │                                                          │   │
│   └─────────────────────────────────────────────────────────┘   │
│                                                                  │
│   RESULT:                                                       │
│   ┌─────────────────────────────────────────────────────────┐   │
│   │   ┌─────────┐┌─────────┐┌─────────┐┌─────────┐         │   │
│   │   │ Core 0  ││ Core 1  ││ Core 2  ││ Core 3  │         │   │
│   │   │   T1    ││   T2    ││   T3    ││   T7    │         │   │
│   │   │ Pri:50  ││ Pri:40  ││ Pri:20  ││ Pri:45  │         │   │
│   │   └─────────┘└─────────┘└─────────┘└─────────┘         │   │
│   │       │          │          │          ▲               │   │
│   │   No change  No change  No change  T7 here!            │   │
│   │              T2 keeps running!                          │   │
│   │                                                          │   │
│   └─────────────────────────────────────────────────────────┘   │
│                                                                  │
│   COMPARISON:                                                   │
│   ┌──────────────────────────┬───────────────────────────────┐ │
│   │   T7 in Cluster A        │   T7 in Global Cluster        │ │
│   ├──────────────────────────┼───────────────────────────────┤ │
│   │                          │                                │ │
│   │   Core 3 is IDLE but     │   Core 3 is IDLE and          │ │
│   │   T7 CAN'T use it! ✗     │   T7 CAN use it! ✓            │ │
│   │                          │                                │ │
│   │   T7 preempts T2         │   T7 runs on idle core        │ │
│   │   T2 stops working       │   T2 keeps working            │ │
│   │                          │                                │ │
│   │   Less efficient         │   More efficient              │ │
│   │                          │                                │ │
│   └──────────────────────────┴───────────────────────────────┘ │
│                                                                  │
│   BUT! Restricting to Cluster A might be INTENTIONAL:          │
│   • T7 needs shared L3 cache with Core 0, 1                    │
│   • T7 is safety-critical (ASIL), Cores 2,3 are QM             │
│   • T7 needs specific hardware only on Core 0, 1               │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

---

## Scheduling Algorithms

### Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                  SCHEDULING ALGORITHMS                          │
│                                                                  │
│   Every thread has:                                             │
│   1. PRIORITY (0-255)                                          │
│   2. SCHEDULING ALGORITHM                                       │
│                                                                  │
│   ╔═════════════════════════════════════════════════════════╗   │
│   ║  Algorithm only affects threads at SAME priority!       ║   │
│   ║  Higher priority ALWAYS preempts lower priority!        ║   │
│   ╚═════════════════════════════════════════════════════════╝   │
│                                                                  │
│   ┌───────────────┬──────────────────────────────────────────┐ │
│   │  Algorithm    │  Behavior                                │ │
│   ├───────────────┼──────────────────────────────────────────┤ │
│   │  FIFO         │  Run until you block (simple)           │ │
│   │  Round Robin  │  Run for 4ms, then give turn to others  │ │
│   │  Sporadic     │  High priority for budget, then drops   │ │
│   │  High-Pri IST │  Kernel only (priority 254-255)         │ │
│   └───────────────┴──────────────────────────────────────────┘ │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### FIFO (First In, First Out)

```
┌─────────────────────────────────────────────────────────────────┐
│                         FIFO                                     │
│                                                                  │
│   RULE: Run until you BLOCK yourself!                          │
│                                                                  │
│   ┌─────────────────────────────────────────────────────────┐   │
│   │                                                          │   │
│   │   Thread (FIFO, Pri:40)                                 │   │
│   │                                                          │   │
│   │   ┌─────────────────────────────────────────────────┐   │   │
│   │   │ RUNNING █████████████████████████████████████████│   │   │
│   │   │              No time limit!                      │   │   │
│   │   │              Runs FOREVER until...               │   │   │
│   │   └────────────────────────┬────────────────────────┘   │   │
│   │                            │                             │   │
│   │                            ▼                             │   │
│   │              MsgReceive() / sleep() / mutex_lock()      │   │
│   │                            │                             │   │
│   │                            ▼                             │   │
│   │                        BLOCKED!                          │   │
│   │                                                          │   │
│   └─────────────────────────────────────────────────────────┘   │
│                                                                  │
│   STOPS WHEN:                                                   │
│   • It blocks itself (calls blocking function)                 │
│   • Higher priority thread preempts it                         │
│   • It explicitly yields (rare)                                │
│                                                                  │
│   ⚠️  DANGER: FIFO thread that never blocks = STARVES others! │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### Round Robin

```
┌─────────────────────────────────────────────────────────────────┐
│                      ROUND ROBIN                                 │
│                                                                  │
│   RULE: Run for TIME SLICE (default 4ms), then give turn!      │
│                                                                  │
│   3 Threads: A, B, C (all Round Robin, all Pri:40)             │
│   ┌─────────────────────────────────────────────────────────┐   │
│   │                                                          │   │
│   │   TIME ──────────────────────────────────────────────►  │   │
│   │                                                          │   │
│   │   ┌────┐┌────┐┌────┐┌────┐┌────┐┌────┐┌────┐┌────┐     │   │
│   │   │ A  ││ B  ││ C  ││ A  ││ B  ││ C  ││ A  ││ B  │     │   │
│   │   │4ms ││4ms ││4ms ││4ms ││4ms ││4ms ││4ms ││4ms │     │   │
│   │   └────┘└────┘└────┘└────┘└────┘└────┘└────┘└────┘     │   │
│   │                                                          │   │
│   │   A → B → C → A → B → C → A → B → ...                  │   │
│   │                                                          │   │
│   │   Everyone gets FAIR share! ✓                           │   │
│   │                                                          │   │
│   └─────────────────────────────────────────────────────────┘   │
│                                                                  │
│   IMPORTANT: Preempted time does NOT count!                    │
│   ┌─────────────────────────────────────────────────────────┐   │
│   │                                                          │   │
│   │   Thread A (RR, Pri:40, 4ms slice)                      │   │
│   │   Thread X (Pri:60, higher) preempts!                   │   │
│   │                                                          │   │
│   │   ┌──────┐┌─────────────┐┌──────┐                       │   │
│   │   │  A   ││      X      ││  A   │                       │   │
│   │   │ 2ms  ││    2ms      ││ 2ms  │                       │   │
│   │   └──────┘└─────────────┘└──────┘                       │   │
│   │      │                       │                           │   │
│   │      │◄──── A's 4ms slice ───►│                         │   │
│   │           (X's time NOT counted!)                       │   │
│   │                                                          │   │
│   │   A runs 2ms → X preempts 2ms → A gets remaining 2ms   │   │
│   │   FAIR! A doesn't lose time! ✓                         │   │
│   │                                                          │   │
│   └─────────────────────────────────────────────────────────┘   │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### Sporadic

```
┌─────────────────────────────────────────────────────────────────┐
│                       SPORADIC                                   │
│                                                                  │
│   IDEA: Thread has TWO priorities, switches between them!      │
│                                                                  │
│   Parameters:                                                   │
│   ┌─────────────────┬───────────────────────────────────────┐   │
│   │  PRIORITY       │  Normal/High priority                 │   │
│   │  LOW_PRIORITY   │  Background/Low priority              │   │
│   │  BUDGET         │  How long can run at high priority    │   │
│   │  REPLENISH      │  When budget gets refilled            │   │
│   └─────────────────┴───────────────────────────────────────┘   │
│                                                                  │
│   EXAMPLE: Pri=50, Low=10, Budget=5ms, Replenish=20ms          │
│   ┌─────────────────────────────────────────────────────────┐   │
│   │                                                          │   │
│   │   PRIORITY                                               │   │
│   │      ▲                                                   │   │
│   │   50 │  ████████               ████████                 │   │
│   │      │  ║      ║               ║      ║                 │   │
│   │      │  ║      ║               ║      ║                 │   │
│   │   10 │  ║      ║░░░░░░░░░░░░░░░║      ║░░░░░░           │   │
│   │      │  ║      ║               ║      ║                 │   │
│   │      └──╨──────╨───────────────╨──────╨──────────────►  │   │
│   │         0    5ms            20ms   25ms        TIME     │   │
│   │                                                          │   │
│   │         │◄─ Budget ─►│       │◄─ Budget ─►│            │   │
│   │         │◄──────── Replenish Period ──────►│            │   │
│   │                                                          │   │
│   │   ████ = Running at Priority 50 (using budget)         │   │
│   │   ░░░░ = Running at Priority 10 (budget exhausted)     │   │
│   │                                                          │   │
│   └─────────────────────────────────────────────────────────┘   │
│                                                                  │
│   USE CASE: Prevent CPU-hungry thread from starving others!    │
│                                                                  │
│   Without Sporadic: Thread A (Pri:50) runs FOREVER!            │
│                     Thread B (Pri:40) NEVER gets CPU! 😱       │
│                                                                  │
│   With Sporadic:    Thread A gets 5ms at Pri:50                │
│                     Then drops to Pri:10                        │
│                     Thread B (Pri:40) can now run! ✓           │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

---

## Core Affinity

### Binding Thread to Cluster