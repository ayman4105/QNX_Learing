# QNX PCI Bus Devices — Quick README

## Core Idea

To program a PCI device in QNX, your application or driver should communicate with the PCI subsystem through the QNX PCI APIs.

Before using those APIs, the PCI server must be running:

```sh
pci-server
```

The basic model is:

```text
Application / Driver
        |
        v
PCI APIs
        |
        v
pci-server
        |
        v
PCI Device
```

---

## 1. Start `pci-server`

The `pci-server` process manages access to PCI devices.

It allows applications and drivers to discover, attach to, configure, and access PCI devices using QNX PCI APIs.

---

## 2. Find the PCI Device

Use:

```c
pci_device_find()
```

This finds a PCI device by:

```text
Vendor ID
Device ID
```

The function fills a `pci_bdf_t` value.

`BDF` means:

```text
Bus
Device
Function
```

This identifies where the PCI device is located on the PCI bus.

---

## 3. Attach to the Device

After finding the device, use:

```c
pci_device_attach()
```

This connects your process to the selected PCI device.

The PCI server may allocate the device to your process exclusively or non-exclusively depending on the parameters and device state.

---

## 4. Read and Write PCI Configuration Space

PCI devices have configuration registers.

Use APIs such as:

```c
pci_device_read()
pci_device_write()
```

These are used to read and write PCI configuration information.

You may use them to access information such as:

```text
Interrupt configuration
Device capabilities
PCI configuration registers
```

---

## 5. Base Address Registers / BARs

PCI devices expose memory or I/O regions through Base Address Registers.

Use:

```c
pci_device_read_ba()
```

This gives information about the base addresses supported by the device.

After reading the BARs, the driver can map the device memory region if needed.

---

## 6. Address Mapping for DMA / Bus Mastering

If the PCI device performs DMA or bus mastering, the device needs an address it can use.

A CPU virtual address is not enough for the PCI device.

Use:

```c
pci_device_map_as()
```

This translates CPU-side addresses or buffers into addresses suitable for the PCI device.

Example idea:

```text
CPU virtual address / buffer
        |
        v
pci_device_map_as()
        |
        v
PCI-device-usable address
```

This is important when programming device registers with buffer addresses for DMA.

---

## 7. Reset the Device

Use:

```c
pci_device_reset()
```

This resets the PCI device when needed.

---

## 8. Detach When Finished

When your driver or application finishes using the device, call:

```c
pci_device_detach()
```

This releases the device and cleans up PCI resources.

---

## 9. Typical PCI Programming Flow

```text
1. Start pci-server
2. Find the device using pci_device_find()
3. Get the device BDF: Bus / Device / Function
4. Attach using pci_device_attach()
5. Read BAR information using pci_device_read_ba()
6. Read/write configuration registers if needed
7. Map or translate addresses if DMA is needed
8. Use the device
9. Reset the device if required
10. Detach using pci_device_detach()
```

---

## 10. Quick Function Summary

| Function               | Purpose                                                |
| ---------------------- | ------------------------------------------------------ |
| `pci_device_find()`    | Find a PCI device using vendor ID and device ID        |
| `pci_device_attach()`  | Attach to a selected PCI device                        |
| `pci_device_reset()`   | Reset the PCI device                                   |
| `pci_device_detach()`  | Release the PCI device                                 |
| `pci_device_read()`    | Read PCI configuration information                     |
| `pci_device_write()`   | Write PCI configuration information                    |
| `pci_device_read_ba()` | Read base address / BAR information                    |
| `pci_device_map_as()`  | Translate CPU addresses to PCI-device-usable addresses |

---

## Final Summary

To program a PCI device in QNX:

```text
Run pci-server
Find the device by vendor ID and device ID
Attach to it
Read its configuration and BARs
Map or translate addresses if needed
Use the device
Detach when finished
```

Golden sentence:

```text
QNX PCI programming starts with pci-server, then uses PCI APIs to find, attach, configure, map, and release PCI devices safely.
```