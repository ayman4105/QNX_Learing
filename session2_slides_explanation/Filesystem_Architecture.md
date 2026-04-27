# 📖 QNX Filesystem Architecture - Complete Guide

## Table of Contents
1. [What is a Filesystem in QNX?](#1-what-is-a-filesystem-in-qnx)
2. [The Three Pillars](#2-the-three-pillars)
   - [Pillar 1: No Single Root Disk](#pillar-1-no-single-root-disk)
   - [Pillar 2: Every Path is Registered by a Process](#pillar-2-every-path-is-registered-by-a-running-process)
   - [Pillar 3: The Path Space is Virtual](#pillar-3-the-path-space-is-virtual)
3. [What is /proc/boot?](#3-what-is-procboot)
4. [Are /proc/boot Files Virtual?](#4-are-procboot-files-virtual)
5. [Big Files & Disk Mounting](#5-big-files--disk-mounting)
6. [Full Boot-to-File-Access Example](#6-full-boot-to-file-access-example)

---

## 1. What is a Filesystem in QNX?

A filesystem organizes and stores data in a structured way, making it possible
for users and programs to access files easily.

**But in QNX, the filesystem is fundamentally different from traditional OSes:**

```
+================================================================+
|                    TRADITIONAL OS (Linux)                       |
|                                                                 |
|   +----------------------------------------------------------+ |
|   |                      KERNEL                               | |
|   |                                                           | |
|   |   +----------------+                                      | |
|   |   |  FILESYSTEM    |  <-- Lives INSIDE the kernel         | |
|   |   +----------------+                                      | |
|   +----------------------------------------------------------+ |
+================================================================+


+================================================================+
|                         QNX                                     |
|                                                                 |
|   +------------------+                                          |
|   |    KERNEL         |                                         |
|   |   (microkernel)   |   <-- NO filesystem here!              |
|   +------------------+                                          |
|                                                                 |
|   +------------------+     +------------------+                 |
|   | Filesystem       |     | Serial Driver    |                 |
|   | Process          |     | Process          |                 |
|   | (fs-qnx6)       |     | (devc-ser8250)   |                 |
|   +------------------+     +------------------+                 |
|          |                         |                            |
|          +------------+------------+                            |
|                       |                                         |
|              POSIX API (open, read, write, close)               |
+================================================================+
```

### Key Differences:

| Feature               | Traditional OS (Linux)        | QNX                              |
|------------------------|-------------------------------|----------------------------------|
| Filesystem location    | Inside the kernel             | Separate process outside kernel  |
| Communication          | System calls                  | Messages via POSIX API           |
| If filesystem crashes  | Kernel crashes too            | Kernel stays alive               |
| Architecture           | Monolithic                    | Microkernel                      |

> **In QNX, the filesystem is NOT part of the kernel. It runs as a separate
> process that communicates with applications via the POSIX API
> (open, read, write, close, etc.).**

---

## 2. The Three Pillars

### Pillar 1: No Single Root Disk

In traditional systems, the root `/` is mounted from a disk partition.
In QNX, the root `/` is a **virtual address book in RAM**, maintained
by the process manager (`procnto`).

```
+================================================================+
|                    TRADITIONAL OS (Linux)                       |
|                                                                 |
|   HARD DISK                                                     |
|   +------------------------+                                    |
|   | Partition: /dev/sda1   |                                    |
|   |                        |                                    |
|   |   /                    |                                    |
|   |   ├── home/            |                                    |
|   |   ├── etc/             |   <-- Root "/" is ON the disk      |
|   |   ├── var/             |                                    |
|   |   └── tmp/             |                                    |
|   +------------------------+                                    |
+================================================================+


+================================================================+
|                         QNX                                     |
|                                                                 |
|   HARD DISK                                                     |
|   +------------------------+                                    |
|   |                        |                                    |
|   |   (maybe some files)   |   <-- Root "/" is NOT here!        |
|   |                        |                                    |
|   +------------------------+                                    |
|                                                                 |
|   RAM (Memory)                                                  |
|   +----------------------------------------------+              |
|   |   procnto (Process Manager)                  |              |
|   |                                              |              |
|   |   /  <-- Root lives HERE in RAM              |              |
|   |   It is a VIRTUAL ADDRESS BOOK               |              |
|   |   maintained by procnto                      |              |
|   +----------------------------------------------+              |
+================================================================+
```

**What exactly is Root `/` in QNX?**

It is a **lookup table** inside RAM, maintained by `procnto`.
It maps **paths** to **processes** and answers one question:

> **"Which PROCESS is responsible for this PATH?"**

```
+================================================================+
|                    ROOT "/" in QNX                               |
|                                                                 |
|   procnto (Process Manager)                                     |
|   +------------------------------------------------------+      |
|   |                                                      |      |
|   |   ADDRESS BOOK (in RAM)                              |      |
|   |                                                      |      |
|   |   +----------------------------------------------+   |      |
|   |   | Path          |  Who Owns It?                |   |      |
|   |   |---------------|------------------------------|   |      |
|   |   | /             |  procnto itself              |   |      |
|   |   | /dev/ser1     |  serial driver process       |   |      |
|   |   | /dev/audio    |  audio driver process        |   |      |
|   |   | /fs/usb       |  usb filesystem process      |   |      |
|   |   | /tmp          |  ram filesystem process      |   |      |
|   |   +----------------------------------------------+   |      |
|   |                                                      |      |
|   |   This table IS the root "/"                         |      |
|   |   It is NOT a folder on a disk                       |      |
|   |   It is a LOOKUP TABLE in memory                     |      |
|   +------------------------------------------------------+      |
|                                                                  |
|   When app calls: open("/dev/ser1")                              |
|                                                                  |
|   procnto looks at this table:                                   |
|       "/dev/ser1" --> oh! serial driver owns this                |
|       --> sends message to serial driver process                 |
+==================================================================+
```

**Root `/` is NOT:**
- ✗ A folder on a hard disk
- ✗ A partition
- ✗ A file
- ✗ Permanent storage

**Root `/` IS:**
- ✓ A lookup table in RAM
- ✓ Built at boot time
- ✓ Disappears at shutdown
- ✓ Just a map: PATH → PROCESS

---

### Pillar 2: Every Path is Registered by a Running Process

A path in QNX does **NOT** exist by default.
A path **ONLY** exists when a **process** registers it using `resmgr_attach()`.

```
+================================================================+
|                    HOW IT WORKS - STEP BY STEP                  |
|================================================================|
|                                                                 |
|  STEP 1: System boots, only procnto is running                 |
|                                                                 |
|   procnto ADDRESS BOOK:                                         |
|   +---------------------------+-----------------------+         |
|   | Path                      | Owner Process         |         |
|   |---------------------------|-----------------------|         |
|   | /                         | procnto               |         |
|   | /proc/boot/               | procnto               |         |
|   +---------------------------+-----------------------+         |
|                                                                 |
|   Almost EMPTY! No /dev/ser1, no /dev/audio, NOTHING           |
|                                                                 |
|================================================================|
|                                                                 |
|  STEP 2: Serial driver starts (devc-ser8250)                   |
|                                                                 |
|   +-------------+     resmgr_attach()     +----------+          |
|   | devc-ser8250| ----------------------> | procnto  |          |
|   | (serial drv)|  "I own /dev/ser1"      | (kernel) |          |
|   +-------------+                         +----------+          |
|                                                                 |
|   ADDRESS BOOK now:                                             |
|   +---------------------------+-----------------------+         |
|   | /                         | procnto               |         |
|   | /proc/boot/               | procnto               |         |
|   | /dev/ser1                 | devc-ser8250  ✨ NEW   |         |
|   +---------------------------+-----------------------+         |
|                                                                 |
|================================================================|
|                                                                 |
|  STEP 3: Audio driver starts (io-audio)                        |
|                                                                 |
|   +-------------+     resmgr_attach()     +----------+          |
|   | io-audio    | ----------------------> | procnto  |          |
|   | (audio drv) |  "I own /dev/audio"     | (kernel) |          |
|   +-------------+                         +----------+          |
|                                                                 |
|   ADDRESS BOOK now:                                             |
|   +---------------------------+-----------------------+         |
|   | /                         | procnto               |         |
|   | /proc/boot/               | procnto               |         |
|   | /dev/ser1                 | devc-ser8250           |         |
|   | /dev/audio                | io-audio      ✨ NEW   |         |
|   +---------------------------+-----------------------+         |
|                                                                 |
|================================================================|
|                                                                 |
|  STEP 4: USB filesystem starts (fs-usb)                        |
|                                                                 |
|   ADDRESS BOOK now:                                             |
|   +---------------------------+-----------------------+         |
|   | /                         | procnto               |         |
|   | /proc/boot/               | procnto               |         |
|   | /dev/ser1                 | devc-ser8250           |         |
|   | /dev/audio                | io-audio               |         |
|   | /fs/usb/                  | fs-usb        ✨ NEW   |         |
|   +---------------------------+-----------------------+         |
|                                                                 |
|   The address book GROWS as more processes start!               |
+================================================================+
```

**What happens if we KILL a process?**

```
+================================================================+
|  KILL the serial driver:  kill devc-ser8250                    |
|                             💥                                  |
|                                                                 |
|   ADDRESS BOOK now:                                             |
|   +---------------------------+-----------------------+         |
|   | /                         | procnto               |         |
|   | /proc/boot/               | procnto               |         |
|   | /dev/ser1                 | ████ GONE ████        |         |
|   | /dev/audio                | io-audio               |         |
|   | /fs/usb/                  | fs-usb                 |         |
|   +---------------------------+-----------------------+         |
|                                                                 |
|   /dev/ser1 DISAPPEARS!                                        |
|   open("/dev/ser1") --> ERROR! Path not found!                 |
|                                                                 |
|   But EVERYTHING ELSE still works!                              |
|   Kernel: fine. Audio: fine. USB: fine.                         |
+================================================================+
```

**What happens if we RESTART the process?**

```
+================================================================+
|  RESTART:  $ devc-ser8250 &                                    |
|                                                                 |
|   devc-ser8250 starts again and calls resmgr_attach()          |
|                                                                 |
|   ADDRESS BOOK now:                                             |
|   +---------------------------+-----------------------+         |
|   | /                         | procnto               |         |
|   | /proc/boot/               | procnto               |         |
|   | /dev/ser1                 | devc-ser8250  ✨ BACK! |         |
|   | /dev/audio                | io-audio               |         |
|   | /fs/usb/                  | fs-usb                 |         |
|   +---------------------------+-----------------------+         |
|                                                                 |
|   /dev/ser1 is BACK without rebooting the system!              |
+================================================================+
```

**Summary of Pillar 2:**

```
  +------------------+                  +------------------+
  | Process STARTS   |  ------>         | Path APPEARS     |
  +------------------+                  +------------------+

  +------------------+                  +------------------+
  | Process DIES     |  ------>         | Path DISAPPEARS  |
  +------------------+                  +------------------+

  +------------------+                  +------------------+
  | Process RESTARTS |  ------>         | Path comes BACK  |
  +------------------+                  +------------------+
```

> **PATH = PROCESS. No process = No path.**

---

### Pillar 3: The Path Space is Virtual

The **Path Space** is the collection of ALL paths from ALL processes,
managed by the **Path Manager** inside `procnto`.

```
+================================================================+
|              THE PATH SPACE                                     |
|                                                                 |
|   /                                                             |
|   ├── proc/                                                     |
|   │   └── boot/                                                 |
|   │       ├── procnto                                           |
|   │       ├── ls                                                |
|   │       └── .script                                           |
|   ├── dev/                                                      |
|   │   ├── ser1            (from devc-ser8250)                   |
|   │   ├── audio           (from io-audio)                       |
|   │   └── null            (from procnto)                        |
|   ├── fs/                                                       |
|   │   └── usb/            (from fs-usb)                         |
|   └── tmp/                (from ram filesystem)                 |
|                                                                 |
|   This TREE is the Path Space.                                  |
|   It is NOT stored on any disk.                                 |
|   It is ASSEMBLED from many processes at runtime.               |
+================================================================+
```

**The Path Space is built LIVE at boot time:**

```
  TIME ──────────────────────────────────────────►

  t=0  Power ON
       Path Space: (empty)

  t=1  procnto starts
       /
       /proc/boot/

  t=2  devc-ser8250 starts
       /dev/ser1  ✨

  t=3  io-audio starts
       /dev/audio  ✨

  t=4  fs-qnx6 starts
       /fs/hd0/  ✨

  t=5  fs-usb starts
       /fs/usb/  ✨

  THE TREE GROWS WITH TIME AS PROCESSES START
```

**At shutdown:**

```
  System running:

   /
   ├── proc/boot/
   ├── dev/ser1
   ├── dev/audio
   ├── fs/hd0/
   └── fs/usb/

              POWER OFF 💥

   Path Space = COMPLETELY GONE
   It was in RAM only.
   Nothing on disk holds this tree.

              POWER ON AGAIN ⚡

   Processes start again.
   They register the same paths again.
   Path Space is RE-ASSEMBLED from scratch.
```

**Path Manager Role:**

```
  When app calls open("/dev/ser1"):

  +-------+    open("/dev/ser1")    +------------------+
  |  App  | ----------------------> | PATH MANAGER     |
  +-------+                        | (inside procnto) |
                                    +------------------+
                                           |
                                    Looks up address book
                                    "/dev/ser1" --> devc-ser8250
                                           |
                                           v
                                    +------------------+
                                    | devc-ser8250     |
                                    | (serial driver)  |
                                    +------------------+
```

> **The Path Manager is the ROUTER of QNX.
> It connects apps to the right process.**

---

## 3. What is /proc/boot?

`/proc/boot` contains the files from the **IFS (Image Filesystem)** that
was loaded into RAM at boot time.

```
+================================================================+
|                  QNX BOOT PROCESS                               |
|                                                                 |
|  STEP 1: System powers ON                                       |
|                                                                 |
|  STEP 2: IFS (Image Filesystem) loads into RAM                  |
|                                                                 |
|   IFS = one single file that contains EVERYTHING                |
|         needed to boot the system                               |
|                                                                 |
|   +------------------------------------------------------+      |
|   |              IFS (Image Filesystem)                   |      |
|   |                                                      |      |
|   |   +------------+  +--------------+  +------------+   |      |
|   |   |  procnto   |  | devc-ser8250 |  | startup    |   |      |
|   |   |  (kernel)  |  | (serial drv) |  | script     |   |      |
|   |   +------------+  +--------------+  +------------+   |      |
|   |                                                      |      |
|   |   +------------+  +--------------+                   |      |
|   |   |  ls        |  | config files |                   |      |
|   |   |  (command) |  |              |                   |      |
|   |   +------------+  +--------------+                   |      |
|   +------------------------------------------------------+      |
|                                                                  |
|  STEP 3: procnto registers all these files                       |
|          under the path /proc/boot/                              |
+==================================================================+
```

```
  /proc/boot/
      ├── procnto          (the kernel itself)
      ├── startup-apic     (startup code)
      ├── devc-ser8250     (serial driver binary)
      ├── ls               (list command)
      ├── cat              (cat command)
      ├── .script          (boot script)
      └── config.txt       (config files)
```

**Properties of /proc/boot files:**
- ✓ Loaded into RAM at boot
- ✓ Available IMMEDIATELY (no disk needed)
- ✓ READ-ONLY (cannot modify)
- ✓ Managed by procnto
- ✗ NOT on a hard disk

**IFS is typically 5MB - 50MB** and contains only the minimum
needed to boot the system.

---

## 4. Are /proc/boot Files Virtual?

**Not exactly!** They are **real data** but stored in **RAM**, not on disk.

There are **three types** of files in QNX:

```
+================================================================+
|              THREE TYPES OF FILES IN QNX                        |
|================================================================|
|                                                                 |
|  TYPE 1: REAL FILES ON DISK                                     |
|  ─────────────────────────                                      |
|   Example: /fs/hd0/data.txt                                    |
|                                                                 |
|   ✓ Persistent (stays after shutdown)                           |
|   ✓ Read + Write                                                |
|   ✓ Real data on physical disk                                  |
|                                                                 |
|================================================================|
|                                                                 |
|  TYPE 2: /proc/boot FILES (IFS in RAM)                          |
|  ─────────────────────────────────────                          |
|   Example: /proc/boot/ls                                        |
|                                                                 |
|   ✓ REAL DATA (actual binary programs)                          |
|   ✓ You CAN execute them                                        |
|   ✗ Stored in RAM not disk                                      |
|   ✗ READ-ONLY                                                   |
|   ✗ Disappears when power off                                   |
|                                                                 |
|================================================================|
|                                                                 |
|  TYPE 3: PURE VIRTUAL FILES                                     |
|  ──────────────────────────                                     |
|   Example: /dev/ser1                                            |
|                                                                 |
|   ✗ No real file content at all                                 |
|   ✗ Just a PATH registered by a process                         |
|   ✗ No bytes stored anywhere                                    |
|   ✗ Writing sends data to hardware                              |
|                                                                 |
+================================================================+
```

**Where does /proc/boot fit on the spectrum?**

```
  VIRTUAL ◄────────────────────────────────► REAL ON DISK
     |                  |                         |
  /dev/ser1      /proc/boot/ls             /fs/hd0/data.txt
  /dev/null      /proc/boot/procnto        /fs/usb/file.txt
     |                  |                         |
  No data         Real data                  Real data
  Just a path     But in RAM                 On physical disk
  Process only    Read-only                  Read + Write
                  From IFS image             Persistent

  /proc/boot = IN THE MIDDLE
  Real content + Virtual location
```

---

## 5. Big Files & Disk Mounting

The IFS (in RAM) is small (5-50MB). Big files like applications,
databases, and media **must live on a real disk** and be mounted.

**This happens in two phases:**

```
+================================================================+
|   PHASE 1: BOOT (IFS only - RAM)                               |
|   ════════════════════════════════                              |
|                                                                 |
|   RAM                                                           |
|   +----------------------------------+                          |
|   | /proc/boot/                      |                          |
|   |   ├── procnto      (kernel)      |                          |
|   |   ├── devb-eide     (disk drv)   |  <-- Disk DRIVER         |
|   |   ├── fs-qnx6       (fs process)|  <-- Filesystem PROCESS  |
|   |   └── .script        (boot cmds)|                          |
|   +----------------------------------+                          |
|                                                                 |
|   ✓ Kernel running                                              |
|   ✓ Basic drivers loaded                                        |
|   ✗ No disk files visible yet!                                  |
|                                                                 |
|================================================================|
|                                                                 |
|   PHASE 2: MOUNT DISK FILESYSTEM                               |
|   ════════════════════════════════                              |
|                                                                 |
|   The boot script (.script) runs:                               |
|                                                                 |
|     devb-eide &                                                 |
|     fs-qnx6 /dev/hd0t177 /fs/hd0                              |
|                                                                 |
+================================================================+
```

**Mounting step by step:**

```
+================================================================+
|   STEP 1: devb-eide starts (disk hardware driver)              |
|                                                                 |
|   +------------+    resmgr_attach()     +----------+            |
|   | devb-eide  | --------------------> | procnto  |            |
|   | (disk drv) | "I own /dev/hd0"      |          |            |
|   +------------+                       +----------+            |
|                                                                 |
|   Path Space:                                                   |
|   +-------------------+------------------+                      |
|   | /                 | procnto          |                      |
|   | /proc/boot/       | procnto          |                      |
|   | /dev/hd0          | devb-eide  ✨     |                      |
|   +-------------------+------------------+                      |
|                                                                 |
|   /dev/hd0 = raw access to hard disk                           |
|   Can read sectors, but CANNOT see files yet!                  |
|                                                                 |
|================================================================|
|                                                                 |
|   STEP 2: fs-qnx6 starts (filesystem process)                 |
|                                                                 |
|   +----------+     reads disk via      +------------+           |
|   | fs-qnx6  | --------------------->  | devb-eide  |           |
|   |          |     /dev/hd0            | (disk drv) |           |
|   +----------+                         +------------+           |
|        |                                                        |
|        |  resmgr_attach("/fs/hd0")                              |
|        v                                                        |
|   +----------+                                                  |
|   | procnto  |                                                  |
|   +----------+                                                  |
|                                                                 |
|   Path Space:                                                   |
|   +-------------------+------------------+                      |
|   | /                 | procnto          |                      |
|   | /proc/boot/       | procnto          |                      |
|   | /dev/hd0          | devb-eide        |                      |
|   | /fs/hd0/          | fs-qnx6   ✨     |                      |
|   +-------------------+------------------+                      |
|                                                                 |
|   NOW disk files are accessible!                                |
|     /fs/hd0/my_program                                         |
|     /fs/hd0/database.db                                        |
|     /fs/hd0/video.mp4                                          |
+================================================================+
```

**Key insight: Path Space vs Actual Data**

```
+-------------------------------+-------------------------------+
|     PATH SPACE (in RAM)       |     ACTUAL DATA               |
|-------------------------------|-------------------------------|
| Virtual address book          | Real bytes on disk            |
| Just maps paths to processes  | The actual file content       |
| Small (just a table)          | Can be huge (GBs, TBs)       |
| Disappears at shutdown        | Stays on disk permanently    |
| Managed by procnto            | Managed by fs-qnx6           |
+-------------------------------+-------------------------------+
```

---

## 6. Full Boot-to-File-Access Example

Here is the complete flow from power on to accessing a 200MB file:

### Phase 1: Power ON — RAM is empty

```
+================================================================+
|                   POWER ON                                      |
|                                                                 |
|   RAM                          HARD DISK                        |
|   +-----------+                +-------------------------+      |
|   |           |                | my_program   (200MB)    |      |
|   |  (empty)  |                | database.db  (1GB)      |      |
|   |           |                | video.mp4    (500MB)    |      |
|   +-----------+                +-------------------------+      |
|                                                                 |
|   RAM is empty. Disk has files. Nobody can see them yet.       |
+================================================================+
```

### Phase 2: IFS loads into RAM

```
+================================================================+
|   RAM                                                           |
|   +--------------------------------------+                      |
|   |  IFS (Image Filesystem) - 20MB       |                      |
|   |  /proc/boot/                         |                      |
|   |    ├── procnto       (kernel)        |                      |
|   |    ├── devb-eide     (disk driver)   |                      |
|   |    ├── fs-qnx6       (fs process)   |                      |
|   |    ├── ls            (command)       |                      |
|   |    └── .script       (boot script)  |                      |
|   +--------------------------------------+                      |
|                                                                 |
|   HARD DISK (untouched)                                         |
|   +-------------------------+                                   |
|   | my_program   (200MB)    |  <-- Still invisible!             |
|   | database.db  (1GB)      |                                   |
|   | video.mp4    (500MB)    |                                   |
|   +-------------------------+                                   |
+================================================================+
```

### Phase 3: Boot script mounts the disk

```
+================================================================+
|   .script runs:                                                 |
|                                                                 |
|   devb-eide &                    --> /dev/hd0 appears           |
|   fs-qnx6 /dev/hd0t177 /fs/hd0  --> /fs/hd0/ appears          |
|                                                                 |
|   devb-eide reads RAW BLOCKS from disk hardware                |
|   fs-qnx6 reads through devb-eide, UNDERSTANDS files          |
|   fs-qnx6 registers /fs/hd0/ in path space                    |
|                                                                 |
|   Path Space:                                                   |
|   +-------------------+------------------+                      |
|   | /                 | procnto          |                      |
|   | /proc/boot/       | procnto          |                      |
|   | /dev/hd0          | devb-eide        |                      |
|   | /fs/hd0/          | fs-qnx6         |                      |
|   +-------------------+------------------+                      |
|                                                                 |
|   Disk files NOW visible:                                       |
|     /fs/hd0/my_program                                         |
|     /fs/hd0/database.db                                        |
|     /fs/hd0/video.mp4                                          |
+================================================================+
```

### Phase 4: App opens a big file

```
+================================================================+
|    App calls: open("/fs/hd0/my_program")                       |
|                                                                 |
|   STEP 1: App --> open() --> procnto (Path Manager)            |
|                                                                 |
|   STEP 2: procnto looks up "/fs/hd0/" --> owned by fs-qnx6    |
|           Sends message to fs-qnx6                             |
|                                                                 |
|   STEP 3: fs-qnx6 needs actual data from disk                 |
|           Talks to devb-eide                                   |
|                                                                 |
|   STEP 4: devb-eide reads from physical hard disk              |
|                                                                 |
|   STEP 5: Data flows back up:                                  |
|                                                                 |
|           HARD DISK                                             |
|              │ raw blocks                                       |
|              v                                                  |
|           devb-eide                                             |
|              │ raw data                                         |
|              v                                                  |
|           fs-qnx6                                               |
|              │ organized file data                              |
|              v                                                  |
|           App receives the 200MB file data                     |
|                                                                 |
|   The big file is NEVER fully loaded into RAM.                 |
|   fs-qnx6 reads it piece by piece from disk                   |
|   as the app requests it.                                      |
+================================================================+
```

### Complete Architecture View

```
+================================================================+
|              THE COMPLETE PICTURE                               |
|                                                                 |
|   PATH SPACE (virtual address book in RAM)                     |
|                                                                 |
|   /                                                             |
|   ├── proc/boot/          ← FROM RAM (IFS image)               |
|   │   ├── procnto              Small, boot essentials          |
|   │   ├── ls                   Read-only                        |
|   │   └── .script              Loaded at boot                   |
|   │                                                             |
|   ├── dev/                ← FROM DRIVER PROCESSES               |
|   │   ├── hd0                  (devb-eide)                     |
|   │   ├── ser1                 (devc-ser8250)                  |
|   │   └── audio                (io-audio)                      |
|   │                                                             |
|   └── fs/                 ← FROM FILESYSTEM PROCESSES           |
|       ├── hd0/                 (fs-qnx6)                       |
|       │   ├── my_program       ← BIG files on DISK             |
|       │   ├── database.db      ← BIG files on DISK             |
|       │   └── logs/            ← BIG files on DISK             |
|       └── usb/                 (fs-usb)                        |
|           └── movie.mp4        ← Files on USB                  |
|                                                                 |
|   The PATH SPACE is virtual (in RAM)                           |
|   But the ACTUAL DATA of big files is on DISK                  |
|   fs-qnx6 reads/writes that data on demand                    |
+================================================================+
```

---

## Quick Reference Summary

```
+---------------------------------------------------------------+
|                                                               |
|  PILLAR 1: No Single Root Disk                                |
|  ➜ "/" is a virtual address book in RAM                       |
|  ➜ Managed by procnto                                        |
|  ➜ NOT on any disk partition                                  |
|                                                               |
|  PILLAR 2: Every Path is Registered by a Process              |
|  ➜ No process = No path                                      |
|  ➜ Kill process = Path disappears                            |
|  ➜ Start process = Path appears                              |
|  ➜ Uses resmgr_attach() to register                          |
|                                                               |
|  PILLAR 3: The Path Space is Virtual                          |
|  ➜ Entire tree exists ONLY in RAM                            |
|  ➜ Built LIVE at boot by independent processes               |
|  ➜ Disappears at shutdown                                    |
|  ➜ Path Manager (in procnto) is the ROUTER                   |
|                                                               |
|  /proc/boot:                                                  |
|  ➜ IFS contents in RAM                                       |
|  ➜ Real data, read-only, temporary                           |
|                                                               |
|  Big Files:                                                   |
|  ➜ Live on real disk                                         |
|  ➜ Accessed via devb-eide (disk driver) + fs-qnx6 (fs)      |
|  ➜ Mounted into path space at boot                           |
|  ➜ Path Space holds addresses, Disk holds actual data        |
|                                                               |
|  THE QNX FILESYSTEM IS NOT A DISK.                            |
|  IT IS A LIVE ADDRESS BOOK ASSEMBLED AT RUNTIME               |
|  BY INDEPENDENT PROCESSES.                                    |
|                                                               |
+---------------------------------------------------------------+
```