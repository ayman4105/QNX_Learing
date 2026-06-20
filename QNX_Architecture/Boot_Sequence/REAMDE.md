# QNX Boot Sequence — Quick Study README

## Core Idea

QNX boot is about moving from **board firmware** to the **QNX runtime system**.

```text
Board firmware / U-Boot / BIOS
        |
        v
IPL
        |
        v
IFS
        |
        v
startup
        |
        v
procnto
        |
        v
boot script
        |
        v
drivers / services / applications
```

Key sentence:

```text
QNX boots by loading an IFS that contains startup, procnto, and the boot script.
```

---

## 1. Why Boot Sequence Matters

You need to understand QNX boot if you are doing:

```text
Board bring-up
Embedded development
System integration
Boot image configuration
Driver/service startup
```

Because in QNX, you decide what drivers, services, and applications start at boot.

---

## 2. Board Firmware Comes First

Before QNX code starts, board/vendor firmware runs first.

Examples:

```text
ROM code
U-Boot
RedBoot
BIOS
UEFI
Bare-metal loader
```

This part is usually from the board or SoC vendor.

```text
CPU reset
   |
   v
Board firmware
   |
   v
QNX IPL
```

---

## 3. IPL

`IPL` means:

```text
Initial Program Loader
```

It is the **first QNX code** that runs after the board firmware.

Main jobs:

```text
Basic early hardware setup
Prepare RAM/stack enough to continue
Find/load the IFS
Jump to the next boot stage
```

Quiz answer:

```text
First piece of QNX code after CPU reset = IPL
```

---

## 4. IFS

`IFS` means:

```text
Image File System
```

It is the QNX boot image.

It contains:

```text
startup
procnto
boot script
boot-time drivers/services/apps
```

Diagram:

```text
IFS
 |
 +-- startup
 +-- procnto
 +-- boot script
 +-- drivers/services/apps
```

---

## 5. startup

`startup` is **board-specific code**.

Its job is to initialize and describe the board to QNX.

It handles things like:

```text
RAM size and layout
Interrupt controller
Timers and frequencies
CPU cores
Core clusters
Reserved memory
DMA memory
Graphics memory
```

Important:

```text
startup sets up core board-specific hardware
startup jumps to procnto
```

It does **not** start the boot script directly.

---

## 6. System Page

`startup` writes hardware/system information into the **system page**.

```text
startup
   |
   v
System Page
   |
   v
Read-only mapped into processes
```

The system page contains information such as:

```text
CPU info
RAM layout
Interrupts
Timers
Clusters
Hardware configuration
```

Key idea:

```text
System Page = read-only system/hardware information shared with processes.
```

---

## 7. procnto

`procnto` is:

```text
Process Manager + Neutrino Microkernel
```

Its job in boot:

```text
Start QNX kernel/process manager environment
Start executing the boot script
```

Flow:

```text
startup
   |
   v
procnto
   |
   v
boot script
```

---

## 8. Boot Script

The boot script tells QNX what to start.

Because QNX is a microkernel, many OS services are separate processes.

The boot script can start:

```text
Drivers
File systems
Network stack
Logger
Resource managers
Applications
Custom services
```

Example idea:

```text
start serial driver
start filesystem
start network service
start logger
start main application
```

Key idea:

```text
Boot script = list of processes/services to run at boot.
```

---

## 9. QNX vs Linux Boot Idea

```text
Linux / monolithic style:
    many drivers/services may be inside kernel or loaded as modules

QNX microkernel style:
    many drivers/services are processes
    boot script decides what starts
```

This makes QNX:

```text
Modular
Configurable
Good for embedded systems
Smaller when unused services are not started
```

---

## 10. Full Boot Flow

```text
1. CPU reset
2. Board firmware / U-Boot / BIOS / UEFI runs
3. Firmware jumps to QNX IPL
4. IPL performs early setup
5. IPL finds/loads IFS
6. startup runs from IFS
7. startup initializes/describes hardware
8. startup fills system page
9. startup jumps to procnto
10. procnto starts kernel + process manager
11. procnto runs boot script
12. boot script starts drivers/services/apps
```

---

## 11. Common Quiz Questions

### Q1: What is the first QNX code after CPU reset?

```text
IPL
```

---

### Q2: What is the role of startup code?

Correct:

```text
Sets up core board-specific hardware
Jumps to procnto
```

Not correct:

```text
Starts executing the boot script
```

Because `procnto` starts the boot script.

---

### Q3: What is inside the IFS?

```text
startup
procnto
boot script
boot-time files/services/apps
```

---

### Q4: What does procnto contain?

```text
Process Manager + Neutrino Microkernel
```

---

### Q5: What is the boot script used for?

```text
Starting the drivers, services, servers, and applications needed at boot.
```

---

## Final Mental Model

```text
IPL loads the image.
startup prepares/describes the board.
procnto starts QNX.
boot script starts your system services.
```

Final key sentence:

```text
In QNX, the boot script is where the system integrator decides which drivers, services, and applications run at startup.
```