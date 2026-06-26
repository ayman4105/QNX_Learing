# QNX Handling Hardware Interrupts — Quick README

## Core Idea

A hardware interrupt is a signal from a hardware device telling the CPU that the device needs attention.

Examples:

```text
Serial port received a byte
Network card received a packet
CAN controller received a CAN frame
Disk controller completed a transfer
```

In QNX, a device driver registers with the microkernel and tells it:

```text
When this hardware interrupt happens, notify my driver.
```

---

## 1. Basic Interrupt Flow

```text
Hardware Device
      |
      v
Interrupt Controller
(GIC on ARM / APIC on x86)
      |
      v
CPU Core
      |
      v
QNX Microkernel
      |
      v
Driver Thread Notification
```

The interrupt controller manages interrupts from hardware devices and forwards them to the CPU.

The QNX microkernel catches the interrupt and wakes the correct driver logic.

---

## 2. Main QNX Interrupt Handling Methods

QNX provides two main ways for a driver thread to be notified about hardware interrupts:

```text
1. InterruptAttachThread()
2. InterruptAttachEvent()
```

---

## 3. `InterruptAttachThread()`

This method makes the calling thread become an Interrupt Service Thread.

```text
Driver thread becomes the IST
```

The thread waits for interrupts using:

```c
InterruptWait();
```

Typical flow:

```text
InterruptAttachThread()

while (1) {
    InterruptWait()
    handle hardware interrupt
    acknowledge hardware
    InterruptUnmask()
}
```

### When to use it

Use this method when you need:

```text
Lowest latency
Fast interrupt response
A dedicated interrupt handling thread
```

### Main limitation

The thread is dedicated to waiting for that interrupt.

If the driver has multiple interrupts, it may need multiple interrupt service threads.

---

## 4. `InterruptAttachEvent()`

This method tells the kernel:

```text
When the interrupt happens, deliver an event to my process.
```

The event is usually a pulse.

Typical flow:

```text
Create channel
Create self connection
Initialize pulse event
Attach interrupt event
Wait using MsgReceive()
```

Example concept:

```text
Hardware interrupt
      |
      v
Kernel IST
      |
      v
Pulse/Event delivered
      |
      v
Driver wakes from MsgReceive()
```

### When to use it

Use this method when you want:

```text
A single-threaded driver
Interrupts and client messages handled in the same MsgReceive() loop
More flexible design
A thread pool or event-based architecture
```

### Main limitation

It has more overhead and higher latency than a direct interrupt service thread.

---

## 5. Masking and Unmasking Interrupts

When an interrupt happens, QNX usually masks the interrupt before waking the driver thread.

This prevents the same interrupt source from repeatedly interrupting before the driver is ready.

After the driver handles and acknowledges the hardware, it should unmask the interrupt:

```c
InterruptUnmask();
```

Simple idea:

```text
Interrupt happens
Kernel masks interrupt
Driver handles hardware
Driver acknowledges hardware
Driver unmasks interrupt
```

---

## 6. Acknowledge the Hardware

The driver must usually talk to the hardware registers and acknowledge the interrupt.

This tells the hardware:

```text
I saw your interrupt. You can stop asserting it now.
```

If the driver does not acknowledge the hardware, the interrupt may keep firing.

---

## 7. Avoid `InterruptDisable()` and `InterruptEnable()` in Normal Drivers

QNX provides:

```c
InterruptDisable();
InterruptEnable();
```

But these are usually not used in normal drivers.

They can disable interrupts at a CPU-core level, which is a strong global effect.

Most drivers should use:

```text
InterruptMask()
InterruptUnmask()
```

instead of globally disabling interrupts.

---

## 8. Required Privileges

To attach to hardware interrupts, the process needs interrupt privilege:

```text
PROCMGR_AID_INTERRUPT
```

This is required for:

```c
InterruptAttachThread();
InterruptAttachEvent();
```

Low-level interrupt enable/disable operations may also need I/O privilege, usually obtained using:

```c
ThreadCtl();
```

Most normal interrupt handling drivers need interrupt privilege, not global I/O interrupt disable/enable behavior.

---

## 9. Useful Attach Flags

### `_NTO_INTR_FLAGS_NO_UNMASK`

Normally, attaching to an interrupt may unmask it automatically.

Use this flag if you want to attach first, finish setup, and manually unmask later.

```text
Attach now
Do more setup
Explicitly call InterruptUnmask() when ready
```

### CPU-local interrupt flag

Some interrupts are local to a specific CPU core.

For CPU-local interrupts:

```text
The thread must be bound to one CPU core
The interrupt attach must specify CPU-local behavior
The thread must stay on that same core
```

---

## 10. Choosing Between Thread and Event

| Method                    | Best For                     | Main Advantage                       | Main Cost              |
| ------------------------- | ---------------------------- | ------------------------------------ | ---------------------- |
| `InterruptAttachThread()` | Dedicated interrupt handling | Lowest latency                       | Needs dedicated thread |
| `InterruptAttachEvent()`  | Flexible event/message loop  | Can combine interrupts with messages | More overhead          |

---

## 11. Common Event Choices

When using `InterruptAttachEvent()`, the event can notify the driver using different mechanisms.

Common choices:

```text
SIGEV_PULSE
SIGEV_SEM
Signals with sigwaitinfo()
```

The most common flexible driver design is usually:

```text
InterruptAttachEvent() + SIGEV_PULSE + MsgReceive()
```

Avoid signal handlers for normal driver interrupt work because they are harder to control and usually have worse latency and structure.

---

## 12. Recommended Practical Choices

For lowest latency:

```text
Use InterruptAttachThread()
Wait using InterruptWait()
Handle hardware directly
Unmask when ready
```

For single-threaded or flexible driver design:

```text
Use InterruptAttachEvent()
Deliver a SIGEV_PULSE
Receive it using MsgReceive()
Handle client messages and hardware interrupts in the same loop
```

---

## 13. Check Your Knowledge

### Question

What methods does QNX provide for a device driver thread to be notified about hardware interrupts?

Select all that apply.

### Options

1. It can directly program the interrupt controller.
2. It can ask for the kernel to deliver an event to the thread.
3. It can be registered as an interrupt service thread (IST).
4. An entry can be placed into the system page with the interrupt number and thread ID.

### Correct Answers

```text
2. It can ask for the kernel to deliver an event to the thread.
3. It can be registered as an interrupt service thread (IST).
```

### Explanation

Option 2 is correct because QNX provides:

```c
InterruptAttachEvent();
```

This lets the kernel catch the interrupt and deliver an event, such as a pulse, to the driver thread.

Option 3 is correct because QNX provides:

```c
InterruptAttachThread();
```

This registers the calling thread as an Interrupt Service Thread.

Option 1 is wrong because the driver should not directly program the interrupt controller. The QNX microkernel manages interrupt controller interaction.

Option 4 is wrong because the system page provides hardware information from startup code. A driver does not connect an interrupt to a thread by adding an entry to the system page.

---

## Final Summary

```text
QNX hardware interrupts can notify a driver in two main ways:

1. InterruptAttachThread()
   The driver thread becomes an interrupt service thread and waits using InterruptWait().

2. InterruptAttachEvent()
   The kernel catches the interrupt and delivers an event, usually a pulse, to the driver.
```

Golden sentence:

```text
Use InterruptAttachThread() for the lowest interrupt latency, and use InterruptAttachEvent() when you want flexible event-based interrupt handling inside a normal message loop.
```