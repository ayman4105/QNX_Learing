# 🔧 QNX Resource Managers - Complete Guide

## Table of Contents
1. [What is a Resource Manager?](#1-what-is-a-resource-manager)
2. [How It Works](#2-how-it-works)
3. [Everything is a Resource Manager](#3-everything-is-a-resource-manager)
4. [Everything is a File](#4-everything-is-a-file)
5. [Writing Your Own Resource Manager](#5-writing-your-own-resource-manager)
6. [Build, Run, and Test](#6-build-run-and-test)

---

## 1. What is a Resource Manager?

Every path in the QNX filesystem is **owned by a resource manager**.
A resource manager is a **regular user-space process** that does 4 things:

```
+================================================================+
|              RESOURCE MANAGER = PATH OWNER                      |
|                                                                 |
|   1. STARTS UP like any normal program                          |
|                                                                 |
|   2. CLAIMS OWNERSHIP of a path                                 |
|      Example: "I own /dev/ser1"                                 |
|                                                                 |
|   3. REGISTERS that path with procnto                           |
|      Using: resmgr_attach()                                     |
|                                                                 |
|   4. ANSWERS file operations on that path:                      |
|      open()  read()  write()  close()  seek()                   |
|                                                                 |
+================================================================+
```

### Example: Serial Port Driver

```
+================================================================+
|   devc-ser8250 (serial driver) is a RESOURCE MANAGER           |
|                                                                 |
|   1. It starts:      $ devc-ser8250 &                          |
|   2. It claims:      "I own /dev/ser1"                         |
|   3. It registers:   resmgr_attach("/dev/ser1") --> procnto    |
|   4. It waits:       Any app calling open("/dev/ser1")         |
|                      gets ROUTED to this driver                |
|                                                                 |
|   +----------+    open("/dev/ser1")    +----------+             |
|   |   App    | ----------------------> | procnto  |             |
|   +----------+                         +----------+             |
|                                             |                   |
|                                    "who owns /dev/ser1?"        |
|                                    "devc-ser8250 owns it!"      |
|                                             |                   |
|                                             v                   |
|                                     +--------------+            |
|                                     | devc-ser8250 |            |
|                                     | (resource    |            |
|                                     |  manager)    |            |
|                                     +--------------+            |
|                                             |                   |
|                                     Handles open()              |
|                                     Then handles read()         |
|                                     Then handles write()        |
|                                     Then handles close()        |
|                                                                 |
+================================================================+
```

---

## 2. How It Works

### Resource Manager Lifecycle

```
+================================================================+
|        RESOURCE MANAGER LIFECYCLE                               |
|                                                                 |
|   +-------------------+                                         |
|   |  1. INITIALIZE    |  Setup internal data                    |
|   +-------------------+                                         |
|            |                                                    |
|            v                                                    |
|   +-------------------+                                         |
|   |  2. REGISTER PATH |  resmgr_attach("/dev/ser1")            |
|   +-------------------+                                         |
|            |                                                    |
|            v                                                    |
|   +-------------------+                                         |
|   |  3. WAIT LOOP     |  Sit and wait for messages              |
|   +-------------------+                                         |
|            |                                                    |
|            v                                                    |
|   +-------------------+                                         |
|   |  4. MESSAGE IN!   |  App called open("/dev/ser1")          |
|   +-------------------+                                         |
|            |                                                    |
|            v                                                    |
|   +-------------------+                                         |
|   |  5. HANDLE IT     |  Run handler, reply to app             |
|   +-------------------+                                         |
|            |                                                    |
|            v                                                    |
|   +-------------------+                                         |
|   |  6. GO BACK TO 3  |  Wait for next message                 |
|   +-------------------+                                         |
|                                                                 |
+================================================================+
```

### POSIX Operations Handled

When an app calls any of these on the resource manager's path,
the resource manager receives a message:

```
+================================================================+
|   +------------------+----------------------------------------+ |
|   | App calls        | Resource Manager receives              | |
|   |------------------+----------------------------------------| |
|   | open("/dev/ser1")| "Someone wants to OPEN a connection"   | |
|   | read(fd, ...)    | "Someone wants to READ data"           | |
|   | write(fd, ...)   | "Someone wants to WRITE data"          | |
|   | close(fd)        | "Someone wants to CLOSE connection"    | |
|   | seek(fd, ...)    | "Someone wants to MOVE position"       | |
|   | stat("/dev/ser1")| "Someone wants FILE INFO"              | |
|   +------------------+----------------------------------------+ |
+================================================================+
```

### Full Flow Example: Sending "Hello" via Serial Port

```
+================================================================+
|   App code:                                                     |
|       fd = open("/dev/ser1", O_WRONLY);                         |
|       write(fd, "Hello", 5);                                    |
|       close(fd);                                                |
|================================================================|
|                                                                 |
|   STEP 1: open("/dev/ser1")                                    |
|                                                                 |
|   App --> procnto --> devc-ser8250                              |
|                       open_handler() runs:                      |
|                       - Check permissions                       |
|                       - Setup serial hardware                   |
|                       - Return fd = 3                           |
|   App <-- fd = 3                                                |
|                                                                 |
|   STEP 2: write(3, "Hello", 5)                                 |
|                                                                 |
|   App --> devc-ser8250 (direct, no procnto needed)             |
|           write_handler() runs:                                 |
|           - Receives "Hello"                                    |
|           - Sends H-e-l-l-o to serial port HARDWARE            |
|   App <-- "wrote 5 bytes OK"                                   |
|                                                                 |
|   STEP 3: close(3)                                              |
|                                                                 |
|   App --> devc-ser8250                                          |
|           close_handler() runs:                                 |
|           - Cleanup connection                                  |
|           - Free resources                                      |
|   App <-- "closed OK"                                           |
|                                                                 |
+================================================================+
```

---

## 3. Everything is a Resource Manager

In QNX, **ALL** of these are resource managers:

```
+================================================================+
|   +--------------------+-------------+------------------------+ |
|   | Resource Manager   | Path        | What it does           | |
|   |--------------------+-------------+------------------------| |
|   | devc-ser8250       | /dev/ser1   | Serial port I/O        | |
|   | devc-ser8250       | /dev/ser2   | Serial port I/O        | |
|   | io-audio           | /dev/snd/   | Audio playback/record  | |
|   | devb-eide          | /dev/hd0    | Raw disk access        | |
|   | fs-qnx6           | /fs/hd0/    | Disk filesystem        | |
|   | fs-usb             | /fs/usb/    | USB filesystem         | |
|   | devc-pty           | /dev/pty/   | Pseudo terminals       | |
|   | io-net             | /dev/socket | Network sockets        | |
|   | procnto            | /proc/      | Process info           | |
|   | devf-generic       | /dev/fs0    | Flash filesystem       | |
|   | YOUR PROGRAM       | /dev/xxx    | Anything you want!     | |
|   +--------------------+-------------+------------------------+ |
+================================================================+
```

---

## 4. Everything is a File

Because everything is a resource manager, **everything looks
like a file** to the app:

```
+================================================================+
|                                                                 |
|   App wants to send serial data:                               |
|       fd = open("/dev/ser1"); write(fd, data);                 |
|                                                                 |
|   App wants to play audio:                                     |
|       fd = open("/dev/snd/pcm0"); write(fd, audio_data);      |
|                                                                 |
|   App wants to read a file:                                    |
|       fd = open("/fs/hd0/myfile.txt"); read(fd, buf);          |
|                                                                 |
|   App wants to send network data:                              |
|       fd = open("/dev/socket"); write(fd, packet);             |
|                                                                 |
|   SAME API FOR EVERYTHING:                                      |
|       open()  read()  write()  close()                         |
|                                                                 |
|   The app does NOT know if it is talking to:                   |
|     - A serial port                                             |
|     - A sound card                                              |
|     - A hard disk                                               |
|     - A network                                                 |
|                                                                 |
|   This is: "EVERYTHING IS A FILE" (POSIX uniform interface)   |
|                                                                 |
+================================================================+
```

```
+================================================================+
|                         +-------+                               |
|                         |  App  |                               |
|                         +-------+                               |
|                            |                                    |
|                   open() read() write() close()                |
|                   (same API for everything!)                   |
|                            |                                    |
|              +-------------+-------------+                      |
|              |             |             |                       |
|              v             v             v                       |
|       +----------+  +----------+  +----------+                  |
|       |/dev/ser1 |  |/fs/hd0/  |  |/dev/snd/ |                  |
|       |serial    |  |disk files|  |audio     |                  |
|       |driver    |  |filesystem|  |driver    |                  |
|       +----------+  +----------+  +----------+                  |
|       Resource Mgr  Resource Mgr  Resource Mgr                  |
|              |             |             |                       |
|              v             v             v                       |
|       +----------+  +----------+  +----------+                  |
|       | SERIAL   |  | HARD     |  | SOUND    |                  |
|       | PORT HW  |  | DISK HW  |  | CARD HW  |                  |
|       +----------+  +----------+  +----------+                  |
|                                                                 |
+================================================================+
```

---

## 5. Writing Your Own Resource Manager

### Concept

You can write **your own** resource manager that claims any path
and handles file operations however you want.

```
+================================================================+
|     EXAMPLE: CUSTOM RESOURCE MANAGER                           |
|                                                                 |
|   We create a process that:                                    |
|     - Registers: /dev/mydevice                                 |
|     - Has an internal buffer in RAM                            |
|     - write() stores data INTO the buffer                      |
|     - read() returns data FROM the buffer                      |
|                                                                 |
|   +-------+    open/read/write/close    +------------------+   |
|   |  App  | --------------------------> |  our_resmgr      |   |
|   | (cat) |    (via procnto routing)    |  /dev/mydevice   |   |
|   +-------+                             +------------------+   |
|                                              |                  |
|                                         Internal buffer:        |
|                                         stores data in RAM      |
+================================================================+
```

### Steps to Build a Resource Manager

```
+================================================================+
|   STEP-BY-STEP RECIPE                                          |
|================================================================|
|                                                                 |
|   STEP 1: dispatch_create()                                    |
|           Create the message-receiving system                  |
|                                                                 |
|   STEP 2: iofunc_func_init()                                  |
|           Fill handler tables with QNX DEFAULTS                |
|           (default open, read, write, close, etc.)             |
|                                                                 |
|   STEP 3: Override handlers you want to customize              |
|           ifuncs.read  = my_read;                              |
|           ifuncs.write = my_write;                             |
|                                                                 |
|   STEP 4: iofunc_attr_init()                                  |
|           Setup file attributes (permissions, size)            |
|                                                                 |
|   STEP 5: resmgr_attach("/dev/mydevice")                      |
|           Register path with procnto                           |
|           THIS is the magic moment!                            |
|           Path appears in the filesystem!                      |
|                                                                 |
|   STEP 6: dispatch_block() in a loop                           |
|           Wait for messages forever                            |
|           When message arrives, call the right handler         |
|                                                                 |
+================================================================+
```

```
+================================================================+
|   FLOW DIAGRAM                                                  |
|================================================================|
|                                                                 |
|   +------------------+                                          |
|   | dispatch_create  |  Create message system                   |
|   +------------------+                                          |
|            |                                                    |
|            v                                                    |
|   +------------------+                                          |
|   | iofunc_func_init |  Fill default handlers                   |
|   +------------------+                                          |
|            |                                                    |
|            v                                                    |
|   +------------------+                                          |
|   | Override:        |                                          |
|   | read = my_read   |  Custom read handler                    |
|   | write = my_write |  Custom write handler                   |
|   +------------------+                                          |
|            |                                                    |
|            v                                                    |
|   +------------------+                                          |
|   | iofunc_attr_init |  Permissions: 0666                      |
|   +------------------+                                          |
|            |                                                    |
|            v                                                    |
|   +------------------------+                                    |
|   | resmgr_attach          |  Register "/dev/mydevice"          |
|   | ("/dev/mydevice")      |  Path appears NOW!                |
|   +------------------------+                                    |
|            |                                                    |
|            v                                                    |
|   +------------------+         +----> my_read()                |
|   | dispatch_block   |  ------>|                                |
|   | (wait for msgs)  |  ------>|----> my_write()               |
|   | (loop forever)   |  ------>|                                |
|   +------------------+         +----> default_close()          |
|                                                                 |
+================================================================+
```

### Key QNX Functions Reference

```
+================================================================+
|   FUNCTION REFERENCE                                           |
|================================================================|
|                                                                 |
|   SETUP FUNCTIONS:                                              |
|   +---------------------------+-------------------------------+ |
|   | Function                  | Purpose                       | |
|   |---------------------------+-------------------------------| |
|   | dispatch_create()         | Create message dispatch system| |
|   | iofunc_func_init()        | Fill handler tables with      | |
|   |                           | default QNX handlers          | |
|   | iofunc_attr_init()        | Setup file attributes         | |
|   |                           | (permissions, size, type)     | |
|   | resmgr_attach()           | Register path with procnto   | |
|   |                           | THE key function!             | |
|   +---------------------------+-------------------------------+ |
|                                                                 |
|   MESSAGE LOOP FUNCTIONS:                                       |
|   +---------------------------+-------------------------------+ |
|   | dispatch_context_alloc()  | Allocate message context      | |
|   | dispatch_block()          | Wait for next message         | |
|   | dispatch_handler()        | Route message to handler      | |
|   +---------------------------+-------------------------------+ |
|                                                                 |
|   INSIDE HANDLER FUNCTIONS:                                     |
|   +---------------------------+-------------------------------+ |
|   | MsgReply()                | Send data back to app         | |
|   | resmgr_msgread()          | Read data from app message    | |
|   | _IO_SET_READ_NBYTES()     | Set how many bytes were read  | |
|   | _IO_SET_WRITE_NBYTES()    | Set how many bytes were       | |
|   |                           | written                       | |
|   +---------------------------+-------------------------------+ |
|                                                                 |
|   HANDLER TABLE (override any of these):                        |
|   +---------------------------+-------------------------------+ |
|   | cfuncs.open               | Called on open()              | |
|   | ifuncs.read               | Called on read()              | |
|   | ifuncs.write              | Called on write()             | |
|   | ifuncs.close_ocb          | Called on close()             | |
|   | ifuncs.seek               | Called on lseek()             | |
|   | ifuncs.stat               | Called on stat()              | |
|   | ifuncs.devctl             | Called on devctl()            | |
|   +---------------------------+-------------------------------+ |
|                                                                 |
|   QNX HEADERS NEEDED:                                           |
|   +---------------------------+-------------------------------+ |
|   | <sys/iofunc.h>            | Default handler functions     | |
|   | <sys/dispatch.h>          | Message dispatching framework | |
|   | <sys/resmgr.h>            | Resource manager framework    | |
|   +---------------------------+-------------------------------+ |
|                                                                 |
+================================================================+
```

---

## 6. Build, Run, and Test

```
+================================================================+
|   BUILD AND RUN                                                 |
|================================================================|
|                                                                 |
|   # Compile                                                     |
|   $ qcc -o my_resmgr my_resmgr.c                               |
|                                                                 |
|   # Run                                                         |
|   $ ./my_resmgr &                                               |
|   Resource Manager registered at /dev/mydevice                  |
|                                                                 |
|   # Verify path appeared                                        |
|   $ ls /dev/mydevice                                            |
|   /dev/mydevice          <-- IT EXISTS!                         |
|                                                                 |
+================================================================+
```

```
+================================================================+
|   TEST: READ AND WRITE                                         |
|================================================================|
|                                                                 |
|   # Read from device (triggers my_read handler)                |
|   $ cat /dev/mydevice                                           |
|   Hello from mydevice!                                          |
|                                                                 |
|   # Write to device (triggers my_write handler)                |
|   $ echo "QNX is awesome!" > /dev/mydevice                     |
|                                                                 |
|   # Read again (see new data)                                  |
|   $ cat /dev/mydevice                                           |
|   QNX is awesome!                                               |
|                                                                 |
+================================================================+
```

```
+================================================================+
|   TEST: KILL AND RESTART                                       |
|================================================================|
|                                                                 |
|   # Kill the resource manager                                  |
|   $ kill %1                                                     |
|                                                                 |
|   $ cat /dev/mydevice                                           |
|   cat: No such file or directory     <-- PATH GONE!            |
|                                                                 |
|   # Restart                                                     |
|   $ ./my_resmgr &                                               |
|                                                                 |
|   $ cat /dev/mydevice                                           |
|   Hello from mydevice!               <-- PATH IS BACK!         |
|                                                                 |
|   This proves Pillar 2:                                        |
|     Kill process  = Path disappears                            |
|     Start process = Path appears                               |
|                                                                 |
+================================================================+
```

---

## Quick Reference Summary

```
+---------------------------------------------------------------+
|                                                               |
|  RESOURCE MANAGER:                                            |
|  ➜ A user-space process that OWNS a path                     |
|  ➜ Registers with procnto via resmgr_attach()                |
|  ➜ Handles: open, read, write, close, seek, stat             |
|  ➜ Apps use standard POSIX API to talk to it                 |
|                                                               |
|  KEY PRINCIPLE:                                               |
|  ➜ EVERYTHING in QNX is a resource manager                   |
|  ➜ Serial ports, audio, disk, network, custom devices        |
|  ➜ All accessed via: open() read() write() close()           |
|  ➜ "Everything is a File" (POSIX uniform interface)          |
|                                                               |
|  BUILDING YOUR OWN:                                           |
|  ➜ dispatch_create()      - Create message system            |
|  ➜ iofunc_func_init()     - Default handlers                 |
|  ➜ Override handlers      - Custom read/write/etc            |
|  ➜ iofunc_attr_init()     - File attributes                  |
|  ➜ resmgr_attach()        - Register path (THE key step)     |
|  ➜ dispatch_block() loop  - Wait and handle messages         |
|                                                               |
+---------------------------------------------------------------+
```
