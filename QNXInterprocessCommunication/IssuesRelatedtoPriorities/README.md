# QNX IPC — Message Priorities and Server Priority Inheritance

## Core Idea

QNX message passing is priority-aware.

When several clients send messages to the same server, the server receives work in this order:

```text
1. Highest priority first
2. If same priority, oldest waiting first
```

This applies to:

```text
messages
pulses
messages and pulses together
```

QNX also changes the server thread priority during message handling to avoid two problems:

```text
1. fake priority boost
2. priority inversion
```

---

## 1. Message Receive Order

If multiple clients are waiting on the same server channel:

```text
Client A priority 10
Client B priority 20
Client C priority 15
```

The server receives first from:

```text
priority 20
then priority 15
then priority 10
```

Priority wins before time.

---

## 2. Same Priority Rule

If multiple clients have the same priority:

```text
Client A priority 10 sent first
Client B priority 10 sent later
```

The server receives:

```text
Client A first
Client B second
```

So the rule is:

```text
priority order first
then time order
```

---

## 3. Pulse Receive Order

Pulses follow the same rule:

```text
highest pulse priority first
if same priority, oldest pulse first
```

---

## 4. Messages and Pulses Together

Messages are not always before pulses.

Pulses are not always before messages.

They are mixed together by:

```text
priority first
time second
```

Example:

```text
Message priority 10
Pulse priority 20
```

The server receives the pulse first.

---

## 5. Server Priority When Handling a Lower Priority Client

Example:

```text
Server receive thread priority = 22
Client t2 priority = 10
```

Client t2 sends a message.

When the server receives t2 message, the server thread drops to:

```text
priority 10
```

Why?

To prevent the low-priority client from getting work done at the server's high priority.

This prevents fake priority boost.

---

## 6. Fake Priority Boost Problem

Without priority drop:

```text
low-priority client sends request
a high-priority server does the work
low-priority client indirectly gets high-priority execution
```

That would be unfair.

So QNX makes the server execute that client work at the client priority.

---

## 7. Higher Priority Client Arrives While Server Is Busy

Example:

```text
Server currently handling t2 at priority 10
New client t1 sends a message at priority 13
```

At the time t1 sends the message, QNX raises the server priority to:

```text
priority 13
```

Even if the server is still finishing t2 work.

---

## 8. Why Raise Server Priority?

This prevents priority inversion.

Without raising the server:

```text
t1 is high priority and waiting for server
server is running at low priority 10
other medium-priority threads may run before server
high-priority t1 waits too long
```

That is priority inversion.

QNX solves it by raising the server to the highest priority of clients waiting for it.

---

## 9. Priority Inheritance in QNX IPC

QNX message passing applies priority inheritance-like behavior.

The server priority reflects the work it is doing and the clients waiting for it.

Basic idea:

```text
server should not run higher than needed for a low-priority client
server should not stay too low when a high-priority client is waiting
```

---

## 10. Full Scenario

Initial state:

```text
Server thread priority = 22
Client t2 priority = 10
Client t1 priority = 13
```

Step 1:

```text
t2 sends message
server receives t2 message
server priority drops to 10
```

Reason:

```text
prevent fake priority boost
```

Step 2:

```text
t1 sends message while server is busy
server priority rises to 13
```

Reason:

```text
prevent priority inversion
```

Step 3:

```text
server finishes t2 work
server then handles t1 work
```

---

## 11. Important Rules

```text
If several messages are queued:
    highest priority first
    then oldest first

If several pulses are queued:
    highest priority first
    then oldest first

If messages and pulses are queued:
    mixed by priority and time

When server receives a low-priority client's message:
    server drops to that client priority

When a higher-priority client waits for the server:
    server is raised to that higher priority
```

---

## 12. Why This Matters in Real-Time Systems

This behavior is important because QNX is a real-time OS.

It helps maintain:

```text
predictability
fairness
fast response to high-priority work
protection from priority inversion
protection from fake priority boosts
```

---

## 13. Common Mistakes

### Mistake 1: Thinking FIFO always wins

Wrong.

Priority wins first.

FIFO only applies when priority is the same.

### Mistake 2: Thinking messages always arrive before pulses

Wrong.

Messages and pulses are interleaved by priority and time.

### Mistake 3: Thinking server always runs at its original priority

Wrong.

During message handling, QNX may adjust the server thread priority.

### Mistake 4: Thinking a low-priority client can use a high-priority server to boost itself

QNX prevents this by dropping the server priority when handling that client's message.

### Mistake 5: Ignoring priority inversion

QNX prevents priority inversion by raising the server priority when a higher-priority client is waiting.

---

## 14. Quick Quiz

### Q1: If messages from priority 10 and priority 20 clients are queued, which is received first?

```text
priority 20
```

### Q2: If two messages have the same priority, which is received first?

```text
the one waiting longer
```

### Q3: Are pulses received before messages always?

```text
No.
Messages and pulses are interleaved by priority and time.
```

### Q4: Why does the server drop from priority 22 to priority 10 when handling a priority 10 client?

```text
To prevent fake priority boost.
```

### Q5: Why does QNX raise the server priority when a higher-priority client sends a message?

```text
To prevent priority inversion.
```

### Q6: What is the general receive order rule?

```text
priority order first, then time order
```

---

## Final Summary

```text
QNX receives messages and pulses by priority first and time second.

A server handling a low-priority client drops to that client priority to prevent fake priority boost.

If a higher-priority client becomes blocked on the server, QNX raises the server priority to prevent priority inversion.
```

Golden sentence:

```text
QNX IPC scheduling is priority-aware: work is delivered in priority/time order, and the server priority is adjusted to avoid both fake priority boosts and priority inversion.
```