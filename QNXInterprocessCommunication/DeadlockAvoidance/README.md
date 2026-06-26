# QNX IPC — Deadlock Avoidance and Send Hierarchy

## Core Idea

QNX message passing is synchronous.

When a process calls:

```c
MsgSend()
```

it waits until:

```text
1. the server receives the message
2. the server replies with MsgReply() or MsgError()
```

This is powerful, but it can create deadlocks if two processes send to each other at the same time.

Golden sentence:

```text
To avoid deadlock in QNX message passing, design clear client/server directions: clients send to servers, servers reply or use non-blocking notifications such as pulses.
```

---

## 1. The Deadlock Problem

Imagine two processes:

```text
Process A
Process B
```

A sends to B:

```text
A ---> MsgSend() ---> B
```

At the same time, B sends to A:

```text
B ---> MsgSend() ---> A
```

Now both processes are blocked:

```text
A is waiting for B to receive and reply
B is waiting for A to receive and reply
```

Result:

```text
Deadlock
```

Diagram:

```text
Process A                         Process B
---------                         ---------
MsgSend to B  ----------------->  waiting
waiting       <-----------------  MsgSend to A

Both are waiting.
Nobody is receiving.
```

---

## 2. Why QNX Does Not Detect This Automatically

Deadlock detection in a general operating system is very complex.

The script says detecting the general case can be approximately:

```text
N! complexity
```

where `N` is the number of threads.

That is not acceptable in a real-time OS.

Why?

```text
too expensive
does not scale
adds overhead to blocking operations
breaks real-time predictability
```

So QNX expects you to avoid deadlocks at design time.

---

## 3. Design-time Solution

You must decide clearly:

```text
who is the client?
who is the server?
```

Rule:

```text
Client sends to server.
Server never sends back to client using MsgSend().
Server replies using MsgReply().
```

Normal flow:

```text
Client ---> MsgSend() ---> Server
Client <--- MsgReply() <-- Server
```

This is safe because only one side sends synchronously.

---

## 4. If Client Has Data for Server

This is simple.

Client sends the data:

```text
Client:
    MsgSend("here is data")

Server:
    process data
    MsgReply("processed")
```

Diagram:

```text
Client ------------------ data ------------------> Server
Client <------------- reply / status ------------- Server
```

---

## 5. If Server Has Data for Client

The server should not call `MsgSend()` to the client.

Why?

Because that can create a circular synchronous wait.

Wrong design:

```text
Client sends to Server
Server sends to Client
```

Better design:

```text
Server sends a pulse to Client
Client sends MsgSend() to Server asking for the data
Server replies with the data
```

---

## 6. Pulse-based Safe Flow

If server has data for client:

```text
Server ---> Pulse ---> Client
Client ---> MsgSend("give me data") ---> Server
Client <--- MsgReply(data) <------------ Server
```

Why is this safe?

```text
Pulse is non-blocking.
Only the client does MsgSend().
Server only replies.
```

So no deadlock.

---

## 7. Clients Can Have Channels

A process can be both:

```text
a client to one process
and a server to another process
```

Some clients create channels only to receive pulses from many servers.

Example:

```text
Client channel:
    receives pulses from Server A
    receives pulses from Server B
    receives pulses from Driver C
```

But receiving pulses is not the same as creating a reverse blocking send path.

---

## 8. Deadlocks in Multi-process Systems

With two processes, deadlock is easy to see:

```text
A sends to B
B sends to A
```

But real systems may have:

```text
8 processes
20 processes
50 processes
100 processes
```

Then deadlock cycles become harder to see.

---

## 9. Directed Graph View

Represent processes as nodes.

Represent possible `MsgSend()` paths as arrows.

Example:

```text
1 -> 4 -> 6 -> 7 -> 2 -> 1
```

This is a cycle.

A cycle means possible deadlock.

Another example:

```text
1 -> 4 -> 6 -> 3 -> 7 -> 2 -> 1
```

Also a cycle.

Rule:

```text
Any cycle in the synchronous send graph is a potential deadlock.
```

---

## 10. Send Hierarchy

A common design solution is to build a send hierarchy.

Rule:

```text
Processes send down only.
Never send up.
Never send sideways.
```

Example:

```text
Level 1:        UI / Controller
                  |
Level 2:        Middleware
                /      \
Level 3:     Services   Managers
              /           \
Level 4:   Drivers      Loggers
```

Safe send direction:

```text
top ---> down
```

Not allowed as blocking `MsgSend()`:

```text
bottom ---> up
sideways ---> sideways
```

---

## 11. Why Hierarchy Prevents Deadlock

If every arrow goes down, you can never create a circle.

No cycle means:

```text
no circular wait
no synchronous send deadlock
```

So the graph becomes acyclic.

In graph terms:

```text
Directed Acyclic Graph (DAG)
```

---

## 12. What If Two Same-level Processes Need to Talk?

Example:

```text
Process 3 and Process 4 are on the same level.
```

They cannot send to each other directly because that is sideways.

First choice:

```text
try to move one process up or down in the hierarchy
```

If that breaks other dependencies, second choice:

```text
add a new level
```

Example:

```text
Before:
    2
   / \
  3   4

Need 3 -> 4

After:
    2
    |
    3
    |
    4
```

Now 3 sends down to 4 safely.

---

## 13. What If Data Must Go Up?

Sometimes a lower-level process needs to notify an upper-level process.

Example:

```text
Driver has data for UI
Process 8 needs to notify Process 2
```

Blocking `MsgSend()` upward is not allowed.

Use non-blocking communication:

```text
pulse
signal
semaphore post
other non-blocking notification
```

Common QNX choice:

```text
pulse
```

Flow:

```text
Lower process ---> Pulse ---> Upper process
Upper process ---> MsgSend() down ---> Lower process
Lower process ---> MsgReply(data) ---> Upper process
```

---

## 14. Upward/Sideways Rule

Safe hierarchy rule:

```text
Blocking MsgSend:
    only downward

Upward communication:
    non-blocking only

Sideways communication:
    non-blocking only
```

This avoids cycles.

---

## 15. Dependency Graph and Startup Order

The send hierarchy also helps determine startup order.

If:

```text
Process 7 depends on Process 8
```

then:

```text
start Process 8 before Process 7
```

Because 7 needs 8 to be available.

---

## 16. Startup Example

If Process 1 is a top-level UI and must start quickly, and it depends on lower services:

```text
1 depends on 4
4 depends on 5 and 6
6 depends on 8
```

Possible startup path:

```text
8 -> 5 -> 6 -> 4 -> 1
```

Then start the other side:

```text
7 -> 3 -> 2
```

This optimizes startup for Process 1.

---

## 17. What Usually Lives at Each Level?

### Bottom levels

Usually:

```text
hardware drivers
system loggers
low-level I/O
raw device interfaces
```

### Middle levels

Usually:

```text
middleware
aggregators
protocol adapters
service managers
```

They combine data from lower layers or split commands into lower-level operations.

### Top levels

Usually:

```text
UI
human interface
system controller
command and control
final decision logic
```

They make final decisions and send commands downward.

---

## 18. Automotive Example

Top:

```text
Android IVI / QNX Cluster UI
Vehicle controller
```

Middle:

```text
Vehicle data service
Diagnostics service
Safety gateway
```

Bottom:

```text
CAN driver
Ethernet driver
Sensor driver
Logger
```

Safe blocking sends:

```text
UI -> Vehicle service -> Driver
```

Unsafe blocking send:

```text
Driver -> UI
```

Better:

```text
Driver -> pulse -> UI/service
UI/service -> MsgSend() -> Driver
Driver -> MsgReply(data)
```

---

## 19. Common Mistakes

### Mistake 1: Two processes send to each other

```text
A -> B
B -> A
```

Potential deadlock.

### Mistake 2: Server sends to client using MsgSend

Server should reply, not send back synchronously.

### Mistake 3: Ignoring cycles in the design graph

Any cycle can become a deadlock.

### Mistake 4: Allowing sideways blocking sends

Same-level processes should not `MsgSend()` to each other.

### Mistake 5: Allowing upward blocking sends

Use pulses or other non-blocking notifications for upward communication.

### Mistake 6: Depending on QNX to detect deadlocks

QNX does not solve this for you. Design must avoid it.

---

## 20. Quick Quiz

### Q1: Why can two processes sending to each other deadlock?

```text
Because MsgSend() blocks while waiting for the other side to receive and reply.
```

### Q2: Does QNX detect all IPC deadlocks automatically?

```text
No.
```

### Q3: Why not?

```text
General deadlock detection is too complex and expensive for real-time behavior.
```

### Q4: What is the basic safe client/server rule?

```text
Client sends to server. Server replies. Server does not send synchronously to client.
```

### Q5: What should server use if it has data for client?

```text
A non-blocking notification, commonly a pulse.
```

### Q6: What does a cycle in the send graph mean?

```text
Potential deadlock.
```

### Q7: How does send hierarchy avoid deadlock?

```text
All blocking sends go downward, so cycles cannot form.
```

### Q8: What should upward communication use?

```text
Pulse, signal, semaphore post, or another non-blocking mechanism.
```

### Q9: What does the hierarchy help with besides deadlock avoidance?

```text
Startup order and dependency understanding.
```

### Q10: What usually lives at the bottom of the hierarchy?

```text
Drivers, loggers, and low-level I/O services.
```

---

## Final Summary

```text
QNX MsgSend() is synchronous.
Synchronous send cycles can deadlock.
QNX does not detect all such deadlocks automatically.
Avoid deadlock by design.
```

Safe design:

```text
Client -> Server using MsgSend()
Server -> Client using MsgReply()
Server notification -> Client using pulse
Client then sends down to get data
```

Hierarchy rule:

```text
Blocking sends go down only.
Upward or sideways communication must be non-blocking.
```

Golden sentence:

```text
Design QNX message-passing systems as an acyclic send hierarchy: blocking sends go downward, replies come back upward, and any upward or sideways notification must be non-blocking.
```