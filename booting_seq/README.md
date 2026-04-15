# QNX on Raspberry Pi 5 - Boot Sequence & SD Card Setup

## Boot Sequence Overview

```
Power ON
   |
   v
+------------------+
| BCM2712 Boot ROM |  Hardcoded in chip. Cannot modify.
| (inside SoC)     |  Looks for bootloader on SD/eMMC/USB
+--------+---------+
         |
         v
+------------------+
| EEPROM Bootloader|  Stored on RPi5 board (SPI EEPROM)
| (RPi5 specific)  |  Reads config.txt from SD card
+--------+---------+
         |
         | reads config.txt → finds kernel=ifs-rpi5.bin
         v
+------------------+
| Loads IFS image  |  RPi firmware loads ifs-rpi5.bin
| into RAM         |  directly from SD card FAT32 partition
+--------+---------+
         |
         v
+------------------+
| startup code     |  First code inside IFS to run
| (startup-bcm2712 |  Initializes CPU, caches, MMU
|  -rpi5)          |  Creates system page (HW info)
+--------+---------+  Hands control to procnto
         |
         v
+------------------+
| procnto          |  Microkernel + Process Manager
| (PID 1)          |  Initializes scheduler, IPC, memory
|                  |  Registers /proc/boot/ path
+--------+---------+  Executes boot script
         |
         v
+------------------+
| Boot Script      |  YOUR script inside the build file
| (.script)        |  Launches drivers, services, apps
+--------+---------+
         |
         v
    SYSTEM READY!
    # _  (shell prompt)
```

---

## What's on the SD Card

```
SD Card (FAT32)
|
+-- start4.elf              RPi5 GPU firmware
+-- fixup4.dat              RPi5 memory fixup
+-- bcm2712-rpi-5-b.dtb     RPi5 device tree
+-- bcm2712d0-rpi-5-b.dtb   RPi5 D0 stepping device tree
+-- overlays/                Device tree overlays
+-- config.txt               Tells RPi firmware what to load
+-- ifs-rpi5.bin             QNX IFS image (your OS!)
```

---

## config.txt

```
[rpi5]
enable_uart=1           # enable serial console
force_turbo=1           # full CPU speed
uart_2ndstage=1         # show firmware messages on UART
dtparam=uart0_console   # use UART0 as console
kernel=ifs-rpi5.bin     # load this file as "kernel"
```

RPi firmware sees `kernel=ifs-rpi5.bin` and loads that file into RAM.
It's not actually a Linux kernel — it's the full QNX IFS image!

---

## What's Inside the IFS Image

```
ifs-rpi5.bin
|
+-- startup header          tells kernel where everything is
+-- startup-bcm2712-rpi5    initializes hardware
+-- procnto-smp-instr       microkernel + process manager
+-- .script (boot script)   commands to run at boot
+-- drivers                 devc-serpl011-rpi5, devb-sdmmc, etc.
+-- libraries               libc.so, ldqnx-64.so.2, etc.
+-- commands                ksh, ls, pidin, etc.
+-- config files            passwd, profile, etc.
+-- your applications       anything you added
```

After boot, these files appear at `/proc/boot/`
Process Manager creates this path and maps IFS contents to it.

---

## The Build File (rpi5.build)

The build file is a recipe for `mkifs` tool. It has 4 sections:

### Section 1: Image Config
```
[image=0x80000]                    # RAM load address
[virtual=aarch64le,raw -compress]  # ARM 64-bit, raw binary
```

### Section 2: Startup + Kernel
```
startup-bcm2712-rpi5 -v            # startup binary
PATH=/proc/boot:/sbin:/bin         # executable search path
procnto-smp-instr -v               # kernel binary
```

### Section 3: Boot Script
```
[+script] startup-script = {
    pipe
    display_msg "Hello Ayman!"     # printed on serial console
    slogger2
    devc-serpl011-rpi5 ...         # serial driver
    reopen /dev/ser10              # redirect console to serial
    [+session] ksh &               # start shell
}
```

### Section 4: File List
```
ldqnx-64.so.2=ldqnx-64.so.2      # dynamic linker (MUST have!)
/lib/libc.so=libc.so              # C library (MUST have!)
/bin/ksh=ksh                       # shell
/sbin/pipe=pipe                    # pipe manager
devc-serpl011-rpi5                 # serial driver
```

Format: `target_path_in_IFS = source_file_for_mkifs`

---

## Build File Attributes Quick Reference

### Boolean (ON/OFF)
```
+script      this is the boot script
+optional    skip if file not found
+session     create new POSIX session
+compress    compress this file
```

### Value (name=value)
```
uid=0          owner = root
gid=0          group = root
perms=0755     rwxr-xr-x
perms=4755     rwsr-xr-x (setuid)
type=link      symbolic link
type=dir       empty directory
dperms=0755    directory permissions
```

### Scope
```
[uid=0 perms=0755] /bin/ksh=ksh     applies to ksh ONLY
[uid=0 perms=0755]                  applies to ALL files below
```

---

## Boot Script Built-in Commands

| Command | What It Does |
|---------|-------------|
| `display_msg "text"` | Print to serial console (like printf for boot) |
| `reopen /dev/ser10` | Redirect stdin/stdout/stderr to device. Waits for path to exist |
| `waitfor /path [sec]` | Pause until path appears. Prevents race conditions |
| `procmgr_symlink /src /link` | Create symlink in path space at runtime |

---

## How to Build the IFS Image

```bash
# Set QNX environment
source ~/qnx800/qnxsdp-env.sh

# Go to images folder
cd ~/qnx800/bsp/BSP_raspberrypi-bcm2712-rpi5_be-800_SVN1024006_JBN381/images

# Build
make
# OR
mkifs rpi5.build ifs-rpi5.bin
```

`mkifs` searches for binaries in:
- `$QNX_TARGET/aarch64le/bin/`
- `$QNX_TARGET/aarch64le/sbin/`
- `$QNX_TARGET/aarch64le/lib/`
- Any paths given with `-r` option

At runtime, `procnto` finds executables in `/proc/boot/` (set by `PATH=/proc/boot`)

---

## SD Card Setup (Step by Step)

### 1. Partition
```bash
sudo umount /dev/mmcblk0*
sudo cfdisk /dev/mmcblk0
```
In cfdisk:
- Delete old partitions
- New → Enter (full size) → primary
- Type → c (W95 FAT32 LBA)
- Bootable
- Write → yes
- Quit

### 2. Format
```bash
sudo mkfs.vfat -F 32 -n BOOT /dev/mmcblk0p1
```

### 3. Mount
```bash
# Ubuntu auto-mounts, just re-insert SD card
# OR manually:
sudo mkdir -p /media/ayman/BOOT
sudo mount /dev/mmcblk0p1 /media/ayman/BOOT
```

### 4. Copy Files
```bash
# RPi5 firmware files
cp -r ~/Downloads/sdcard_image/* /media/ayman/BOOT/

# QNX image
cp ~/qnx800/bsp/BSP_raspberrypi-bcm2712-rpi5_be-800_SVN1024006_JBN381/images/ifs-rpi5.bin /media/ayman/BOOT/

# Verify
ls -la /media/ayman/BOOT/
```

### 5. Safe Eject (IMPORTANT!)
```bash
sync                              # flush write cache to SD card
sudo umount /dev/mmcblk0p1        # unmount properly
```

⚠️ **ALWAYS `sync` + `umount` before removing SD card! Otherwise data is lost!**

---

## Boot the RPi5

### Serial Connection
```
USB-to-Serial Adapter        RPi5 GPIO Header
     GND          ────────   Pin 6  (GND)
     RX           ────────   Pin 8  (TX / GPIO14)
     TX           ────────   Pin 10 (RX / GPIO15)
     
⚠️ TX↔RX are CROSSED! Adapter RX goes to RPi TX.
⚠️ Do NOT connect 5V/3.3V from adapter to RPi5!
```

### Open Serial Terminal
```bash
picocom -b 115200 /dev/ttyUSB0
# OR
screen /dev/ttyUSB0 115200
```

### Power On RPi5
Insert SD card → Connect serial → Connect power → Watch output!

### Expected Output
```
(RPi firmware messages)
(startup messages)
(procnto messages)

Welcome to QNX 8.0.0 on aarch64le !

==============================
      Hello Ayman!
==============================

Starting WatchDog ...
Starting PCI Server ...
Starting serial driver (/dev/ser10)
...
Starting shell ...
#
```

---

## Troubleshooting

| Problem | Check |
|---------|-------|
| Nothing on serial | Wiring (TX↔RX crossed?), baud rate = 115200 |
| Green LED off | SD card not detected. Re-format FAT32 |
| Green LED flashing | Firmware found but can't load kernel. Check config.txt |
| Garbage characters | Wrong baud rate. Must be 115200 |
| Kernel panic | IFS image corrupted. Rebuild with mkifs |
