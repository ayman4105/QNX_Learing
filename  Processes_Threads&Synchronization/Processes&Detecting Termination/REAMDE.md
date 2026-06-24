# QNX Detecting Process Death — Quick Study README

## Core Idea

This section explains how a QNX program can know that another process has died.

There are three main ways:

```text
1. Parent detects child death
2. Client/server detects connection loss
3. Death pulse detects any process death
```

Golden sentence:

```text
Use waitpid() for child processes, client/server notification for connected processes, and death pulses for monitoring any process.
```

---

## 1. Parent / Child Process Death

This is the POSIX-standard way.

A parent process creates a child process using something like:

```text
posix_spawn()
fork()
```

When the child process dies, the parent receives:

```text
SIGCHLD
```

Then the parent can call:

```c
waitpid();
```

or one of the:

```text
wait*()
```

family functions.

Flow:

```text
Parent
  |
  | posix_spawn()
  v
Child Process
  |
  | exits / crashes / gets killed
  v
Parent receives SIGCHLD
  |
  | waitpid()
  v
Parent gets child PID + status
```

---

## 2. What Does waitpid() Tell You?

`waitpid()` can tell the parent:

```text
which child died
whether it exited normally
whether it died because of a signal
exit status
```

Example idea:

```c
pid_t dead_pid = waitpid(-1, &status, 0);
```

Meaning:

```text
-1:
    wait for any child process
```

Return value:

```text
PID of the child that died
```

---

## 3. Blocking Behavior

`waitpid()` can block.

```text
Parent calls waitpid()
      |
      | child still running
      v
Parent blocks
      |
      | child dies
      v
waitpid() returns
```

This is useful for a launcher/supervisor process.

---

## 4. Launcher Process

A launcher process starts other processes and monitors them.

Example:

```text
launcher
   |
   +-- sensor_service
   +-- logger_service
   +-- network_service
   +-- control_app
```

If one child dies:

```text
child dies
   |
   v
launcher receives SIGCHLD
   |
   v
launcher calls waitpid()
   |
   v
launcher can restart / log / enter safe state
```

---

## 5. Zombie Process

If a child dies and the parent has not called `waitpid()` yet, the child becomes a zombie.

A zombie is:

```text
dead process
not using CPU
memory/resources cleaned
process table entry still exists
```

Why does it still exist?

```text
To store:
    which child died
    why it died
    exit status
```

Flow:

```text
Child dies
   |
   | parent did not call waitpid()
   v
Zombie
   |
   | parent calls waitpid()
   v
Zombie disappears
```

---

## 6. Avoiding Zombies

If the parent does not care about child death status, it can ignore `SIGCHLD`.

```c
signal(SIGCHLD, SIG_IGN);
```

Then children will not remain as zombies.

```text
child dies
   |
   v
no zombie kept
```

Use this only when you really do not need the child exit status.

---

## 7. Client / Server Relationship

This is QNX-specific and applies to message passing.

A server has a channel.

A client connects to that channel.

```text
Client Process
      |
      | connection
      v
Server Channel
      |
      v
Server Process
```

If the relationship goes away, one side can detect it.

Important:

```text
This does not always mean the process died.
It may mean the connection/channel relationship disappeared.
```

Examples:

```text
client died
server died
connection closed
channel disappeared
```

---

## 8. Death Pulses

Death pulses are the most flexible method.

They let a process get notified when **any process** dies.

QNX function:

```c
procmgr_event_notify();
```

Event type:

```c
PROCMGR_EVENT_PROCESS_DEATH
```

Typical notification method:

```text
pulse
```

Flow:

```text
Monitor Process
      |
      | procmgr_event_notify(PROCMGR_EVENT_PROCESS_DEATH)
      v
QNX OS
      |
      | any process dies
      v
Monitor receives pulse
      |
      v
Pulse contains dead process PID
```

Use death pulses when you want to monitor the system generally.

---

## 9. Exercise Files

In the `proc_thread` project, there are two important examples:

```text
death_pulse.c
spawn_example.c
```

---

## 10. death_pulse.c

This demonstrates death pulse notification.

It monitors the death of any process.

Test idea:

```text
1. Run death_pulse
2. Open another terminal
3. Run pidin
4. pidin exits
5. death_pulse prints the PID that died
```

Flow:

```text
death_pulse running
      |
      | registered for death pulse
      v
pidin runs and exits
      |
      v
QNX sends pulse
      |
      v
death_pulse prints dead PID
```

---

## 11. spawn_example.c

This demonstrates parent/child detection.

Usually it spawns a child process such as:

```text
sleep
```

Flow:

```text
spawn_example
      |
      | posix_spawn("sleep")
      v
sleep child process
      |
      | exits
      v
spawn_example receives SIGCHLD
      |
      | waitpid()
      v
gets child PID + status
```

This example also helps you see:

```text
SIGCHLD
waitpid()
zombies
parent/child relationship
```

---

## 12. Checking Parent / Child Relationship

Use:

```sh
pidin family
```

Look at the:

```text
PPID
```

Meaning:

```text
Parent Process ID
```

Example:

```text
spawn_example PID = 1000
sleep PID         = 1001
sleep PPID        = 1000
```

This means:

```text
sleep is a child of spawn_example
```

Diagram:

```text
spawn_example
PID 1000
   |
   +-- sleep
       PID 1001
       PPID 1000
```

---

## 13. Checking in Momentics IDE

In the IDE:

```text
System Information Perspective
      |
      v
Target Navigator View
      |
      v
View Menu
      |
      v
Group by PID Family
```

Then processes appear as a family tree.

If you run `spawn_example` from the IDE:

```text
procnto
   |
   +-- qconn
        |
        +-- spawn_example
             |
             +-- sleep
```

Why?

```text
The IDE asks qconn to launch the program on the target.
```

So `spawn_example` becomes a child of `qconn`.

---

## 14. Zombie Extra Note

If the child becomes a zombie:

```text
child died
parent did not call waitpid()
zombie remains
```

If the parent then dies, the zombie also goes away.

```text
zombie exists
   |
   | parent dies
   v
zombie disappears
```

Reason:

```text
No parent remains to collect the exit status.
```

---

## 15. Method Comparison

```text
Method                  Scope                       Standard?
--------------------------------------------------------------
SIGCHLD + waitpid       child processes only         POSIX
Client/server notify    connected client/server      QNX
Death pulse             any process                  QNX
```

---

## 16. Common Quiz Questions

### Q1: How does a parent detect that a child died?

```text
It receives SIGCHLD and calls waitpid().
```

### Q2: What does waitpid() return?

```text
The PID of the child process that died.
```

### Q3: What is a zombie process?

```text
A dead child process whose resources are cleaned,
but whose process table entry remains until the parent calls waitpid().
```

### Q4: How can you avoid zombies if you do not care about child status?

```text
signal(SIGCHLD, SIG_IGN);
```

### Q5: What file demonstrates death pulses?

```text
death_pulse.c
```

### Q6: What file demonstrates parent/child process death detection?

```text
spawn_example.c
```

### Q7: What command shows parent/child process relationships?

```sh
pidin family
```

### Q8: What is PPID?

```text
Parent Process ID
```

### Q9: What does death_pulse.c print when pidin exits?

```text
The PID of the process that died.
```

---

## Final Summary

```text
Parent/child:
    SIGCHLD + waitpid()

Zombie:
    dead child waiting for parent to collect status

Client/server:
    detects connection/channel relationship loss

Death pulse:
    notifies when any process dies

Exercise:
    death_pulse.c -> any process death
    spawn_example.c -> child process death
```

Final key sentence:

```text
In QNX, parent/child monitoring is POSIX, while death pulses provide a flexible QNX-specific way to monitor any process death.
```