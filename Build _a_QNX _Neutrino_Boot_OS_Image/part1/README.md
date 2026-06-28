# QNX Boot Sequence, Buildfiles, and OS Images — Full Quick README

## 1. Big Picture

This README explains how a QNX system boots, how the OS image is created, and how the `buildfile` controls what goes into that image.

Main topics:

```text
Boot Sequence
Secure Boot / Chain of Trust
QNX OS Image File
/proc/boot
mkifs
Buildfile Format
Boot Script
Boot Script Built-in Commands
Complex Startup Logic
mkifs Search Path
```

---

## 2. QNX Boot Sequence

The exact early boot flow depends on the target board.

For example:

- A virtual machine may start with BIOS or virtual BIOS.
- An x86 system may use BIOS or UEFI.
- Embedded boards may use ROM monitor, U-Boot, or IPL.
- Some boards may jump directly to QNX startup code.

General QNX boot flow:

```text
Power ON
   |
   v
CPU starts executing
   |
   v
Board-specific boot firmware
BIOS / UEFI / ROM Monitor / U-Boot / IPL
   |
   v
QNX startup code
   |
   v
procnto
Microkernel + Process Manager
   |
   v
Boot Script
   |
   v
Drivers / Servers / Applications
   |
   v
System Running
```

---

## 3. IPL

`IPL` means:

```text
Initial Program Loader
```

It is early boot code that may come with the Board Support Package.

Typical IPL responsibilities:

```text
Initialize basic hardware
Set up chip selects
Set up RAM
Load or jump to startup code
```

Not every board uses the same IPL flow.

The exact route depends on the board and BSP.

---

## 4. Startup Code

The startup code is board-specific.

It usually comes from the BSP.

Responsibilities:

```text
Initialize hardware
Discover hardware
Prepare system information
Create system page data
Prepare the environment for procnto
Start procnto
```

Example names may look like:

```text
startup-x86
startup-imx8
startup-boardname
```

---

## 5. `procnto`

`procnto` is one of the most important QNX components.

It combines:

```text
Microkernel
Process Manager
```

After startup code finishes, `procnto` starts running.

Then `procnto` runs the boot script.

Common variants:

```text
procnto-smp-instr
procnto-smp-instr-safety
```

Meaning:

```text
smp:
    Supports multicore systems

instr:
    Instrumented version, useful for tracing/debugging

safety:
    Safety-certified version
```

---

## 6. Boot Script

The boot script is executed by `procnto` after the kernel and process manager are ready.

It starts the system services.

Typical boot script tasks:

```text
Start drivers
Start resource managers
Start servers
Create symbolic links
Wait for devices to appear
Redirect standard I/O
Start shells
Start applications
```

Example idea:

```text
display_msg Starting serial driver...
devc-ser8250 &
waitfor /dev/ser1
reopen /dev/ser1
esh &
```

---

## 7. Secure Boot / Chain of Trust

Secure Boot requires hardware support.

The idea is to verify each boot stage before running it.

```text
Secure hardware
    |
    v
Verify ROM Monitor / U-Boot
    |
    v
Verify IPL / Startup / OS Image
    |
    v
Start QNX
    |
    v
Verify runtime executables if needed
```

### Chain of Trust

Each stage verifies the next stage.

```text
Hardware Root of Trust
        |
        v
ROM Monitor / U-Boot
        |
        v
IPL / Startup Code
        |
        v
QNX OS Image
        |
        v
Boot Script / Drivers / Applications
```

If a stage has been modified, its hash or signature will not match, and boot can be stopped.

---

## 8. QNX Trusted Disk / QTD

Sometimes not all executables are inside the OS image.

A minimal OS image may start a flash filesystem driver, then run additional programs from flash.

To keep trust beyond the OS image, QNX can use:

```text
QNX Trusted Disk
QTD
```

QTD verifies files on the filesystem to make sure they were not tampered with.

This extends the chain of trust from boot hardware to files loaded later.

---

## 9. What Is a QNX OS Image?

A QNX OS image is a file created on the host machine.

It can contain:

```text
Startup code
procnto
Boot script
Drivers
Shared libraries
Applications
Data files
Configuration files
```

If it is bootable, it also contains bootstrap information.

Conceptual structure:

```text
QNX Bootable Image
+---------------------------+
| Bootstrap information     |
+---------------------------+
| Startup code              |
| procnto                   |
| Boot script               |
| Drivers                   |
| Shared libraries          |
| Applications              |
| Data/config files         |
+---------------------------+
```

---

## 10. `/proc/boot`

After the system boots, files inside the OS image appear by default under:

```text
/proc/boot
```

`/proc/boot` is:

```text
Read-only
In-memory
Created from the image contents
```

Example:

If the image contains:

```text
hello
devc-ser8250
libc.so
```

Then after boot:

```text
/proc/boot/hello
/proc/boot/devc-ser8250
/proc/boot/libc.so
```

You can inspect it with:

```sh
ls /proc/boot
```

---

## 11. Big Image vs Minimal Image

### Big Image

Put most system files directly in the image.

```text
Image contains:
    drivers
    apps
    libraries
    configs
```

Pros:

```text
Everything is available immediately from /proc/boot
```

Cons:

```text
Larger image
Harder updates
```

### Minimal Image

Put only the minimum required boot components in the image.

```text
Image contains:
    startup
    procnto
    boot script
    flash driver
    essential libraries
```

Then the boot script starts the flash filesystem driver, and other programs are loaded from flash.

Pros:

```text
Smaller image
More flexible runtime filesystem
```

Cons:

```text
Need filesystem driver early
More boot dependency management
```

---

## 12. `mkifs`

QNX OS images are created using:

```sh
mkifs
```

`mkifs` means:

```text
Make Image File System
```

Basic flow:

```text
Buildfile
   |
   v
mkifs
   |
   v
QNX OS Image
```

Typical command:

```sh
mkifs buildfile image.ifs
```

The output `image.ifs` is placed on the target using a board-specific method.

For example:

```text
SD card
Flash
TFTP
Boot partition
Board-specific storage
```

The BSP documentation usually explains where the image must go.

---

## 13. What Is a Buildfile?

A buildfile is a text file used by `mkifs`.

It describes:

```text
Which startup code to use
Which procnto to use
What boot script to run
Which files to place in the image
Where files should appear at runtime
Which attributes to apply
```

Basic relationship:

```text
buildfile = recipe
mkifs     = builder
image.ifs = output
```

---

## 14. Required Parts of a Bootable Buildfile

A bootable image usually needs:

```text
Bootstrap section
Startup code
procnto
Boot script
Required shared libraries
Drivers/applications needed at boot
```

---

## 15. Bootstrap Section

The bootstrap section tells `mkifs` which startup code and `procnto` to place in the bootable image.

Conceptual example:

```text
[virtual=x86_64] .bootstrap = {
    startup-x86
    PATH=/proc/boot procnto-smp-instr
}
```

Key idea:

```text
.bootstrap contains the early boot components.
```

---

## 16. Important Shared Libraries

Most dynamically linked QNX executables need some shared libraries.

Common important files:

```text
libc.so
libgcc_s.so
ldqnx-64.so.2
libsecpol.so
```

### `libc.so`

Provides common C/POSIX APIs:

```text
open()
read()
write()
printf()
malloc()
kernel call wrappers
```

### `libgcc_s.so`

GCC runtime support library.

### `ldqnx-64.so.2`

Runtime loader and linker.

It loads dynamic executables and shared libraries.

Without it, dynamic programs may fail to start.

### `libsecpol.so`

Security policy library.

Many QNX services and resource managers may require it, especially in secure systems.

---

## 17. Buildfile Line Format

The general buildfile format is:

```text
attribute   filename   contents
```

Or more commonly:

```text
[attributes] runtime_filename = source_contents
```

All parts are optional except that `contents` cannot appear alone without a filename.

---

## 18. Buildfile Examples

### Filename Only

```text
devc-ser8250
```

Meaning:

```text
Find devc-ser8250
Put it in the image
It appears by default as /proc/boot/devc-ser8250
```

### Runtime Name = Source File

```text
/etc/hosts = /home/ayman/qnx/hosts
```

Meaning:

```text
Source on host:
    /home/ayman/qnx/hosts

Runtime path on target:
    /etc/hosts
```

### Attribute + File

```text
uid=0 devc-ser8250
```

Meaning:

```text
Put devc-ser8250 in the image
Apply uid=0 to it
```

### Attribute Alone

```text
+optional
debug_tool
test_app
-optional
```

Meaning:

```text
debug_tool and test_app are optional.
After -optional, following files are no longer optional.
```

---

## 19. Comments and Blank Lines

Blank lines are allowed.

Comments begin with:

```text
#
```

Example:

```text
# Serial driver
devc-ser8250
```

---

## 20. Attribute Types

There are two main types of attributes:

```text
Boolean attributes
Value attributes
```

### Boolean Attributes

Boolean attributes use `+` or `-`.

Examples:

```text
+script
+optional
-optional
```

Meaning:

```text
+attribute:
    enable attribute

-attribute:
    disable attribute
```

### Value Attributes

Value attributes use:

```text
name=value
```

Examples:

```text
uid=0
virtual=x86_64
search=/project/bin
```

---

## 21. `+optional`

`+optional` means missing files do not necessarily fail the build.

Example:

```text
+optional
debug_tool
test_app
-optional
```

If `debug_tool` is missing, `mkifs` may continue.

Use it for non-critical files.

Do not use it for mandatory boot components.

---

## 22. Mapping Files to Different Runtime Paths

By default, image files appear under:

```text
/proc/boot
```

But you can place a file at a different runtime path.

Example:

```text
/etc/hosts = /home/ayman/qnx/hosts
```

Runtime result:

```text
/etc/hosts
```

This is useful for config files that programs expect in standard locations.

---

## 23. Inline Files

You can define small files directly inside the buildfile.

Example:

```text
readme = {
This file was created inside the buildfile.
It will appear in /proc/boot/readme.
}
```

After boot:

```text
/proc/boot/readme
```

This is useful for:

```text
Small config files
README files
Simple scripts
Test data
```

---

## 24. Boot Script Syntax

A boot script is marked with:

```text
+script
```

Example:

```text
[+script] startup-script = {
    display_msg Starting system...
    devc-ser8250 &
    waitfor /dev/ser1
    reopen /dev/ser1
    esh &
}
```

The boot script is interpreted by `procnto`.

---

## 25. Multiple Boot Scripts

A buildfile may contain multiple boot scripts.

`mkifs` concatenates them into one larger boot script in the order it sees them.

```text
script 1
script 2
script 3
```

Becomes:

```text
script 1 + script 2 + script 3
```

Because of this, it is common to use `&` even on commands that appear to be near the end.

There may be another script appended later.

---

## 26. `&` in Boot Script

The `&` means:

```text
Run this program in the background and continue to the next line.
```

Important for drivers and servers.

Bad example:

```text
devc-ser8250
esh
```

If `devc-ser8250` does not exit, `esh` may never run.

Better:

```text
devc-ser8250 &
esh &
```

---

## 27. Command Modifiers

Boot script commands can use modifiers.

Example:

```text
[pri=27f session] esh &
```

Meaning:

```text
Run esh at priority 27
Use FIFO scheduling
Create a POSIX session
Run in background
```

### `pri`

Controls priority and scheduling policy.

QNX is priority-driven, so this is important for real-time behavior.

### `session`

Creates a POSIX session and makes the process a session leader.

Useful for shell/terminal behavior.

---

## 28. Boot Script Built-in Commands

Some commands are built into the process manager.

They are not separate executables.

Common built-ins:

```text
display_msg
procmgr_symlink
waitfor
reopen
```

`mkifs` knows they are built-in and does not search for files with these names.

---

## 29. `display_msg`

Prints a boot-time message to the debug device.

Example:

```text
display_msg Starting serial driver...
```

Use it to debug boot progress.

Example:

```text
display_msg Stage 1
devc-ser8250 &
display_msg Stage 2
```

If boot stops, the last printed message helps locate the failure.

---

## 30. `procmgr_symlink`

Creates a symbolic link in the pathname space.

Example:

```text
procmgr_symlink ../../proc/boot/ldqnx-64.so.2 /usr/lib/ldqnx-64.so.2
```

Purpose:

```text
The file exists in /proc/boot
But programs may look for it in /usr/lib
So a symlink is created
```

Conceptual result:

```text
/usr/lib/ldqnx-64.so.2 -> /proc/boot/ldqnx-64.so.2
```

---

## 31. `waitfor`

Waits until a pathname appears.

Example:

```text
waitfor /dev/ser1
```

Why?

A driver may need time to initialize and register a pathname.

Example flow:

```text
devc-ser8250 &
        |
        v
Driver initializes
        |
        v
Registers /dev/ser1

waitfor /dev/ser1
        |
        v
Continue only after /dev/ser1 exists
```

Use `waitfor` before starting programs that depend on a device or service.

---

## 32. `reopen`

Redirects standard input, standard output, and standard error.

These are file descriptors:

```text
0 = stdin
1 = stdout
2 = stderr
```

Example:

```text
reopen /dev/ser1
esh &
```

Meaning:

```text
Set stdin/stdout/stderr to /dev/ser1
Start esh
esh inherits those file descriptors
```

Then:

```text
printf output from esh -> /dev/ser1
keyboard/input from /dev/ser1 -> esh
```

This is commonly used to start a shell on a serial port.

---

## 33. Multiple Shells / Consoles

Example:

```text
reopen /dev/ser1
esh &

reopen /dev/con1
esh &
```

Result:

```text
First shell:
    stdin/stdout/stderr -> /dev/ser1

Second shell:
    stdin/stdout/stderr -> /dev/con1
```

Each program inherits the file descriptors that are active when it is started.

---

## 34. Environment Variables in Boot Script

You can pass environment variables to commands.

Example:

```text
TERM=qansi esh &
```

Meaning:

```text
Start esh with TERM=qansi
```

This is useful for terminal settings and runtime configuration.

---

## 35. Boot Script Language Is Simple

The QNX boot script language is intentionally simple.

It does not provide full shell logic such as:

```text
if statements
loops
variables
functions
branching
complex expressions
```

It is mainly a startup command list.

---

## 36. Complex Startup Logic

If you need complex logic, use one of these approaches:

```text
Run a ksh script
Run a launcher program
```

### Option 1: Korn Shell Script

Example:

```text
ksh /proc/boot/startup.ksh &
```

A Korn shell script can contain:

```text
if statements
for loops
while loops
variables
functions
```

### Option 2: Launcher Program

Write a C/C++ launcher program.

The launcher can use:

```text
posix_spawn()
```

to start drivers, services, and applications.

Example boot script:

```text
launcher &
```

Launcher program can implement:

```text
hardware checks
conditional startup
retry logic
process spawning
logging
error handling
```

---

## 37. `mkifs` Search Path

When you list a file in the buildfile, `mkifs` must find it on the host machine.

Example:

```text
devc-ser8250
libc.so
hello
```

`mkifs` searches for files using paths based on:

```text
QNX_TARGET
PROCESSOR
MKIFS_PATH
search attribute
```

---

## 38. `virtual` and `PROCESSOR`

The `virtual` attribute defines the target processor type.

Example:

```text
[virtual=x86_64]
```

This gives `mkifs` a processor value such as:

```text
PROCESSOR=x86_64
```

Other possible targets may include:

```text
aarch64
armle-v7
x86_64
```

depending on the QNX version and BSP.

---

## 39. `QNX_TARGET`

`QNX_TARGET` is an environment variable pointing to the QNX target filesystem on the host.

It is usually set when you source the QNX environment script.

Example concept:

```text
QNX_TARGET=/path/to/qnx/target
```

`mkifs` uses it to locate QNX binaries and libraries.

---

## 40. Default `mkifs` Search Path

Typical default search locations:

```text
${QNX_TARGET}/${PROCESSOR}/bin
${QNX_TARGET}/${PROCESSOR}/usr/bin
${QNX_TARGET}/${PROCESSOR}/sbin
```

Example:

If:

```text
QNX_TARGET=/qnx800/target/qnx
PROCESSOR=x86_64
```

Then `mkifs` may search:

```text
/qnx800/target/qnx/x86_64/bin
/qnx800/target/qnx/x86_64/usr/bin
/qnx800/target/qnx/x86_64/sbin
```

---

## 41. `MKIFS_PATH`

You can override or extend the search path with:

```text
MKIFS_PATH
```

Linux example:

```sh
export MKIFS_PATH=/home/ayman/project/bin:/another/path
```

Windows uses semicolon instead of colon:

```text
C:\project\bin;C:\another\path
```

---

## 42. `search` Attribute

You can specify a search path inside the buildfile.

Example:

```text
[search=/home/ayman/project/bin] hello
```

Meaning:

```text
When looking for hello, search /home/ayman/project/bin
```

This is useful for your own applications.

---

## 43. Full Path Mapping

You can avoid search path ambiguity by using full source paths.

Example:

```text
hello = /home/ayman/project/build/hello
```

Or:

```text
/etc/config.txt = /home/ayman/project/config/config.txt
```

This makes it clear where `mkifs` should get the file from.

---

## 44. Built-in Commands vs Real Executables

Built-in commands do not need to be included as files:

```text
display_msg
waitfor
reopen
procmgr_symlink
```

Real executables must be available in the image or on a filesystem:

```text
devc-ser8250
esh
io-sock
hello
launcher
```

If a real executable is used in the boot script and is not available, it will fail to run.

---

## 45. Typical Minimal Buildfile Example

```text
[virtual=x86_64] .bootstrap = {
    startup-x86
    PATH=/proc/boot procnto-smp-instr
}

[+script] startup-script = {
    display_msg Starting QNX...

    procmgr_symlink ../../proc/boot/ldqnx-64.so.2 /usr/lib/ldqnx-64.so.2

    display_msg Starting serial driver...
    devc-ser8250 &

    waitfor /dev/ser1

    reopen /dev/ser1

    [pri=27f session] esh &
}

libc.so
libgcc_s.so
ldqnx-64.so.2
libsecpol.so
devc-ser8250
esh
```

---

## 46. How to Think When Creating an Image

Ask these questions:

```text
1. What board am I targeting?
2. Which startup code does the BSP provide?
3. Which procnto variant do I need?
4. What must run in the boot script?
5. Are all required executables included?
6. Are all required shared libraries included?
7. Do I need /proc/boot only, or will I mount flash/disk?
8. Do I need waitfor before using devices?
9. Where should stdin/stdout/stderr go?
10. Where does mkifs find my custom applications?
```

---

## 47. Common Problems

### Problem: `mkifs` cannot find a file

Check:

```text
Is the file built?
Is it built for the correct architecture?
Is it in QNX_TARGET search paths?
Do I need MKIFS_PATH?
Do I need a search attribute?
Do I need a full source path?
```

### Problem: Program in boot script does not run

Check:

```text
Is the executable included in the image?
Are required shared libraries included?
Is the runtime loader included?
Is the path correct?
Is the command accidentally blocking because missing &?
```

### Problem: Shell does not show output

Check:

```text
Did the serial/console driver start?
Did /dev/ser1 or /dev/con1 appear?
Did you use waitfor?
Did you use reopen before starting the shell?
```

### Problem: File expected in `/etc` is missing

If the file was listed normally, it may be under:

```text
/proc/boot
```

Use mapping:

```text
/etc/hosts = /path/on/host/hosts
```

---

## Final Summary

```text
QNX boot flow:
    CPU
    Board firmware / IPL / U-Boot / BIOS
    Startup code
    procnto
    Boot script
    Drivers and applications

QNX OS image:
    Bootable file created by mkifs
    Contains startup, procnto, boot script, and required files
    Appears at runtime under /proc/boot by default

Buildfile:
    Text recipe used by mkifs
    Defines files, attributes, boot scripts, and runtime paths

Boot script:
    Simple command list interpreted by procnto
    Starts drivers, services, shells, and applications

mkifs search:
    Uses QNX_TARGET, PROCESSOR, MKIFS_PATH, and search attributes
```

Golden sentence:

```text
A QNX bootable image is built by mkifs from a buildfile; the buildfile describes the startup code, procnto, boot script, libraries, drivers, applications, and runtime file layout needed to bring the target system up.
```