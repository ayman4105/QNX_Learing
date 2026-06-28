# QNX Resource Manager Initialization — Quick README

## 1. Goal of This Section

This section explains how to initialize a simple QNX Resource Manager.

The example Resource Manager is registered as:

```text
/dev/example
```

Expected client behavior:

```text
read():
    Always returns 0 bytes

write():
    Accepts any write size

other operations:
    Use normal/default Resource Manager behavior
```

This is a training Resource Manager, not a real hardware driver.

---

## 2. Full Initialization Flow

A simple Resource Manager setup follows this order:

```text
main()
  |
  v
1. dispatch_create_channel()
  |
  v
2. initialize connect function table
  |
  v
3. initialize I/O function table
  |
  v
4. initialize device attributes
  |
  v
5. secpol_resmgr_attach("/dev/example")
  |
  v
6. dispatch_context_alloc()
  |
  v
7. receive loop forever
```

Diagram:

```text
Resource Manager Process
================================================

Create dispatch/channel
        |
        v
Prepare connect handlers
        |
        v
Prepare I/O handlers
        |
        v
Prepare device attributes
        |
        v
Attach /dev/example to pathname space
        |
        v
Allocate dispatch context
        |
        v
while (1):
    block waiting for messages
    dispatch message to handler
```

---

## 3. Step 1 — `dispatch_create_channel()`

The Resource Manager must receive messages from clients.

For that, it needs a channel.

QNX Resource Manager framework creates this using:

```c
dispatch_create_channel(...)
```

Conceptually:

```text
Client
  |
  | open/read/write messages
  v
Resource Manager channel
```

This call returns a dispatch pointer, commonly called:

```text
dpp
```

The `dpp` object is used later by other Resource Manager framework calls.

Typical parameters mentioned in the script:

```text
channel ID = -1
DISPATCH_FLAG_NOLOCK
```

Meaning:

```text
-1:
    Let the framework create/select the channel.

DISPATCH_FLAG_NOLOCK:
    Disable unnecessary locking.
    Useful for simple/single-threaded Resource Managers.
```

---

## 4. Step 2 — Connect Function Table

Connect functions handle pathname-based operations.

Examples:

```text
open()
unlink()
rename()
```

For a simple `/dev/example` Resource Manager, the important connect operation is usually:

```text
open()
```

Concept:

```text
open("/dev/example")
        |
        v
connect function table
        |
        v
open handler
```

---

## 5. Step 3 — I/O Function Table

I/O functions handle file-descriptor-based operations after `open()` succeeds.

Examples:

```text
read(fd)
write(fd)
close(fd)
devctl(fd)
```

For this example:

```text
read handler:
    return 0 bytes

write handler:
    accept any size
```

Diagram:

```text
Client API
        |
        v
QNX message
        |
        v
Framework checks table
        |
        +-- open()  -> connect table -> open handler
        |
        +-- read()  -> I/O table     -> read handler
        |
        +-- write() -> I/O table     -> write handler
```

---

## 6. Step 4 — Device Attributes

Device attributes describe the resource.

They may include:

```text
permissions
owner
group
file type
size
timestamps
custom device data
```

The script uses:

```c
iofunc_attr_init(...)
```

This initializes the attribute structure.

For the example Resource Manager:

```text
No real hardware
No custom data
Use default attributes
```

You can extend the attribute structure if your driver needs custom state.

Example custom data:

```text
device mode
buffer pointer
hardware base address
configuration state
```

---

## 7. Step 5 — `secpol_resmgr_attach()`

This is the step that registers the Resource Manager into the QNX pathname space.

Example path:

```text
/dev/example
```

Before attach:

```text
/dev/example does not exist for clients
```

After attach:

```text
/dev/example -> this Resource Manager
```

Diagram:

```text
example Resource Manager
        |
        | secpol_resmgr_attach("/dev/example")
        v
procnto pathname space
        |
        v
/dev/example -> example Resource Manager PID + CHID
```

---

## 8. `secpol_resmgr_attach()` vs `resmgr_attach()`

Older code may use:

```c
resmgr_attach()
```

The script uses:

```c
secpol_resmgr_attach()
```

Both attach the Resource Manager to the pathname space.

The difference:

```text
secpol_resmgr_attach():
    supports security policy integration
```

Security policy can affect:

```text
user ID
group ID
permissions
access control list
```

---

## 9. Important Parameters to `secpol_resmgr_attach()`

Conceptual parameters:

```text
secpol_handle:
    usually NULL in the simple example

dpp:
    dispatch pointer returned by dispatch_create_channel()

resource manager attributes:
    often NULL for simple examples

path:
    the pathname to register, e.g. /dev/example

file type / permissions:
    usually default values

flags:
    usually 0 unless special behavior is needed

connect functions:
    connect handler table

I/O functions:
    I/O handler table

ioattr:
    device attribute structure

perms_set:
    tells whether security policy updated permissions/attributes
```

---

## 10. Attach Return Value

`secpol_resmgr_attach()` returns an ID for the pathname registration.

This ID can be used later if you want to detach the Resource Manager pathname.

Concept:

```text
attach_id = secpol_resmgr_attach(...)
```

---

## 11. Important Rule — Attach Last

After calling:

```c
secpol_resmgr_attach()
```

the Resource Manager becomes visible to clients.

A client can immediately call:

```c
open("/dev/example", O_RDWR);
```

So you must finish all setup before attaching.

Correct order:

```text
initialize framework
initialize handlers
allocate buffers
detect hardware
prepare attributes
then attach /dev/example
```

Wrong order:

```text
attach /dev/example
then allocate buffers
then detect hardware
```

Why wrong?

```text
A client may open /dev/example while the driver is not ready yet.
```

---

## 12. Important Rule — Structures Must Stay Alive

The Resource Manager library does not copy the structures you pass to it.

So these must remain valid for the lifetime of the Resource Manager:

```text
connect function table
I/O function table
device attribute structure
```

Good choices:

```text
global variables
static variables
malloc-allocated memory
```

Avoid local variables that disappear after an initialization function returns.

Bad idea:

```c
void init(void)
{
    resmgr_connect_funcs_t connect_funcs;
    resmgr_io_funcs_t io_funcs;

    secpol_resmgr_attach(..., &connect_funcs, &io_funcs, ...);
}
```

Better idea:

```c
static resmgr_connect_funcs_t connect_funcs;
static resmgr_io_funcs_t io_funcs;
static iofunc_attr_t ioattr;
```

---

## 13. Step 6 — Dispatch Context

After attaching the Resource Manager, allocate a dispatch context:

```c
dispatch_context_alloc(...)
```

The returned pointer is commonly called:

```text
ctp
```

The dispatch context points to receive-related information.

It contains or references things like:

```text
receive buffer
rcvid
message information
client request data
```

It is passed to:

```text
dispatch_block()
dispatch_handler()
connect handlers
I/O handlers
```

Concept:

```text
ctp = context for receiving and handling one incoming message
```

---

## 14. Step 7 — Receive Loop

Once everything is initialized, the Resource Manager enters a forever loop.

Conceptual loop:

```c
while (1) {
    ctp = dispatch_block(ctp);
    dispatch_handler(ctp);
}
```

Meaning:

```text
dispatch_block():
    block and wait for a client message

dispatch_handler():
    inspect the message and call the correct handler
```

Diagram:

```text
Resource Manager Loop
================================================

while running:
    |
    v
    dispatch_block(ctp)
        waits for message
    |
    v
    message arrives
    |
    v
    dispatch_handler(ctp)
        decides message type
        calls open/read/write handler
```

---

## 15. Full Final Flow

```text
1. Create dispatch/channel
        |
        v
2. Prepare connect table
        |
        v
3. Prepare I/O table
        |
        v
4. Prepare device attributes
        |
        v
5. Attach /dev/example
        |
        v
6. Allocate dispatch context
        |
        v
7. Run dispatch loop forever
```

Detailed version:

```text
dispatch_create_channel()
        |
        v
iofunc_func_init(connect_funcs, io_funcs)
        |
        v
set open/read/write handlers
        |
        v
iofunc_attr_init(ioattr)
        |
        v
secpol_resmgr_attach(
    path = "/dev/example",
    connect_funcs,
    io_funcs,
    ioattr
)
        |
        v
dispatch_context_alloc()
        |
        v
while (1):
    dispatch_block()
    dispatch_handler()
```

---

## 16. What Happens After Initialization?

After initialization, the client can do:

```c
fd = open("/dev/example", O_RDWR);
```

Then:

```c
read(fd, buf, size);
```

Expected result:

```text
returns 0 bytes
```

And:

```c
write(fd, data, size);
```

Expected result:

```text
returns size
```

---

## 17. Common Mistakes

### Mistake 1 — Attaching Too Early

```text
Do not call secpol_resmgr_attach() before your Resource Manager is ready.
```

### Mistake 2 — Local Structures

```text
Do not pass pointers to local structures that will go out of scope.
```

### Mistake 3 — Missing Handlers

```text
If read/write behavior is custom, make sure the I/O function table points to your handlers.
```

### Mistake 4 — Forgetting the Receive Loop

```text
Attaching the pathname is not enough.
The Resource Manager must enter a loop and wait for messages.
```

---

## 18. Quick Review Questions

```text
1. What creates the channel/dispatch object?
2. What table handles open()?
3. What table handles read() and write()?
4. What initializes device attributes?
5. What makes /dev/example visible to clients?
6. Why should secpol_resmgr_attach() be called after all setup?
7. What is ctp?
8. What does dispatch_block() do?
9. What does dispatch_handler() do?
```

Answers:

```text
1. dispatch_create_channel()
2. connect function table
3. I/O function table
4. iofunc_attr_init()
5. secpol_resmgr_attach()
6. Because clients can access the path immediately after attach.
7. dispatch context pointer
8. waits for incoming messages
9. dispatches messages to the correct handler
```

---

## 19. Final Summary

```text
Resource Manager initialization prepares the framework,
sets handler tables,
initializes attributes,
registers a pathname,
allocates receive context,
and starts a blocking receive loop.
```

Golden sentence:

```text
A QNX Resource Manager becomes usable only after it creates a dispatch channel, registers its connect and I/O handlers, initializes its attributes, attaches a pathname with secpol_resmgr_attach(), allocates a dispatch context, and enters the dispatch loop.
```