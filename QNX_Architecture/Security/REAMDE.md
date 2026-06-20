# QNX Security — Quick Study README

## Core Idea

QNX security is not only:

```text
root vs non-root
```

QNX uses layered security:

```text
POSIX file permissions
        +
UID/GID control
        +
Process Manager abilities
        +
Security policies
```

Key sentence:

```text
Give each process only the permissions and abilities it actually needs.
```

---

## 1. POSIX File Permissions

QNX uses Unix/Linux-style file permissions.

You can see them with:

```sh
ls -l
```

Example:

```text
-rwxr-xr--
```

Permissions are divided into:

```text
user
group
other
```

And control:

```text
read
write
execute
```

They apply to:

```text
files
directories
devices
```

---

## 2. Root Problem in the IFS

`IFS` means:

```text
Image File System
```

During development, many things inside the boot image may run as:

```text
root
UID 0
```

This is convenient for development, but bad for production.

```text
Development:
    running as root is easy

Production:
    running everything as root is dangerous
```

Why?

```text
If a root process has a bug or is attacked,
it may control too much of the system.
```

---

## 3. Least Privilege

The goal is:

```text
Do not give a process more access than it needs.
```

Example:

```text
A CAN driver may need hardware I/O access
but it should not be able to kill every process.
```

This is called:

```text
Least Privilege
```

---

## 4. How to Avoid Running Everything as Root

Possible methods:

```text
launcher program
login utility
setuid()
setuid executable feature
C APIs for UID/GID control
```

Idea:

```text
Start each process using the user/permissions it needs.
```

Example:

```text
logger service  -> logger user
network service -> network user
main app        -> app user
```

---

## 5. qconn Warning

`qconn` is the development agent used by Momentics IDE.

It is used for:

```text
copying files to/from target
launching pdebug
debugging from Momentics
IDE target actions
```

Important:

```text
qconn runs as root by default.
```

So:

```text
Use qconn during development.
Do not ship qconn in production.
```

---

## 6. Root vs Fine-Grained Abilities

Traditional model:

```text
root:
    can do almost everything

non-root:
    cannot do privileged operations
```

This is coarse.

QNX provides a finer model:

```text
Process Manager abilities
```

Instead of:

```text
all privileges or no privileges
```

QNX can allow specific abilities only.

---

## 7. Process Manager Abilities

Process Manager abilities control privileged operations.

Examples:

```text
I/O access
interrupt access
mmap / memory mapping
timer access
thread priority management
process control / kill
changing UID
```

Mental model:

```text
Process has ability:
    operation allowed

Process does not have ability:
    operation denied
```

---

## 8. procmgr_ability()

The function used to control these abilities is:

```c
procmgr_ability();
```

It gives or removes fine-grained privileges.

Example idea:

```text
Driver process:
    allow I/O
    allow mmap
    deny unnecessary process control
```

Key point:

```text
procmgr_ability() is more precise than just root/non-root.
```

---

## 9. What Does root Mean Here?

In QNX, think of root like this:

```text
root = process with many/all Process Manager abilities
```

Non-root processes usually do not have all these abilities.

```text
root is powerful because it has the abilities.
```

---

## 10. Security Policies

For production/secured systems, the better approach is:

```text
Security Policies
```

A security policy defines:

```text
which processes belong to which category
what abilities each process gets
what access each process is allowed to have
```

Diagram:

```text
Security Policy
      |
      +--> process type A -> abilities A
      +--> process type B -> abilities B
      +--> process type C -> abilities C
```

---

## 11. secpolgenerate

`secpolgenerate` is a tool that observes system behavior and generates a starting policy.

Flow:

```text
Run minimal system
      |
      v
Run secpolgenerate
      |
      v
Observe process behavior
      |
      v
Generate policy text file
      |
      v
System integrator reviews/edits it
      |
      v
Policy loaded at boot
```

Example observations:

```text
driver uses mmap()
    -> policy includes mmap ability

app kills another process
    -> policy includes process-control ability

driver accesses hardware
    -> policy includes I/O ability
```

---

## 12. Policy File

The generated policy is a text file.

The system integrator can:

```text
add needed permissions
remove unnecessary permissions
tighten access
review each process ability
```

Then it is loaded at boot time.

```text
Boot
 |
 v
Kernel loads security policy
 |
 v
Processes get assigned abilities
```

---

## 13. POSIX Permissions vs Security Policies

```text
POSIX file permissions:
    control access to files/devices
    read/write/execute
    user/group/other

Security policies:
    control deeper system privileges
    I/O, mmap, interrupts, timers, priorities, kill, etc.
```

Both work together.

---

## 14. Common Quiz Questions

### Q1: What file permission model does QNX use?

```text
Unix/POSIX-style file permissions.
```

---

### Q2: Why should production systems avoid running everything as root?

```text
Because root gives too much access.
A bug or attack in one process can affect the whole system.
```

---

### Q3: What is qconn?

```text
A development agent used by Momentics IDE.
It runs as root by default and should not be shipped in production.
```

---

### Q4: What are Process Manager abilities?

```text
Fine-grained privileges that control operations like I/O, mmap, interrupts, timers, priorities, and process control.
```

---

### Q5: What function controls Process Manager abilities?

```text
procmgr_ability()
```

---

### Q6: What is secpolgenerate?

```text
A tool that observes system behavior and generates a starting security policy file.
```

---

### Q7: What is the recommended production approach?

```text
Use security policies and least privilege.
Do not rely only on root/non-root.
```

---

## Final Mental Model

```text
QNX Security
 |
 +-- POSIX file permissions
 |
 +-- avoid running everything as root
 |
 +-- Process Manager abilities
 |
 +-- security policies
 |
 +-- no production qconn
```

Final key sentence:

```text
QNX security uses least privilege: each process should get only the file access and system abilities it needs.
```