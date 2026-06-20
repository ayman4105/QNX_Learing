# CommonAPI + SOME/IP — Persistent QNX Image Setup

This documents the changes made to the QNX CTI repo so that every time you
flash a new SD card, the CommonAPI/SOME/IP environment is ready without any
manual steps on the RPi5.

---

## Problem

After flashing a fresh QNX image, the following were missing every time:

- Versioned symlinks (`libCommonAPI.so.3.2.4`, `libCommonAPI-SomeIP.so.3.2.4`)
- Boost libraries (`libboost_thread`, `libboost_filesystem`, etc.)
- `/var/vsomeip/` directory (required by vsomeip routing manager)
- `/etc/commonapi.ini` (tells CommonAPI to use SOME/IP binding)
- `/etc/vsomeip-client.json` (vsomeip network configuration)
- `HelloWorldClient` binary

---

## Solution: QNX Image Snippets

Two snippet files were added to the repo, and the `mkqnximage.config` was
updated to include them at image build time.

---

## Files Added

### 1. `snippets/system_files.custom.commonapi`

Adds the following to the QNX system partition at build time:

| What | Where in image |
|------|---------------|
| `libCommonAPI.so.3.2.4` symlink | `/usr/lib/` |
| `libCommonAPI-SomeIP.so.3.2.4` symlink | `/usr/lib/` |
| `libboost_thread.so.1.82.0` | `/usr/lib/` |
| `libboost_system.so.1.82.0` | `/usr/lib/` |
| `libboost_filesystem.so.1.82.0` | `/usr/lib/` |
| `libboost_atomic.so.1.82.0` | `/usr/lib/` |
| `libboost_chrono.so.1.82.0` | `/usr/lib/` |
| `commonapi.ini` | `/etc/` |
| `vsomeip-client.json` | `/etc/` |
| `HelloWorldClient` binary | `/usr/bin/` |

```
# Versioned symlinks (ldd requires these exact names)
[type=link uid=0 gid=0 perms=0755] usr/lib/libCommonAPI.so.3.2.4=libCommonAPI.so.3
[type=link uid=0 gid=0 perms=0755] usr/lib/libCommonAPI-SomeIP.so.3.2.4=libCommonAPI-SomeIP.so.3

# Boost libraries (vsomeip depends on these, missing from base image)
[type=file uid=0 gid=0 perms=0755] usr/lib/libboost_thread.so.1.82.0=...
[type=file uid=0 gid=0 perms=0755] usr/lib/libboost_system.so.1.82.0=...
[type=file uid=0 gid=0 perms=0755] usr/lib/libboost_filesystem.so.1.82.0=...
[type=file uid=0 gid=0 perms=0755] usr/lib/libboost_atomic.so.1.82.0=...
[type=file uid=0 gid=0 perms=0755] usr/lib/libboost_chrono.so.1.82.0=...

# CommonAPI runtime config (tells CAPI to use SOME/IP binding, not dbus)
etc/commonapi.ini = {
[default]
binding=someip
libpath=/usr/lib
}

# vsomeip client config (unicast IP, app name, service discovery settings)
[type=file uid=0 gid=0 perms=0644] etc/vsomeip-client.json=...

# HelloWorldClient cross-compiled binary (ARM64 QNX)
[type=file uid=0 gid=0 perms=0755] usr/bin/HelloWorldClient=...
```

---

### 2. `targets/rpi5/snippets/post_start.~50.commonapi`

Runs at every boot to create the vsomeip runtime directory.
vsomeip uses a Unix domain socket at `/var/vsomeip-0` for local IPC between
the routing manager and its clients. Without this directory, the client
crashes immediately with "No such file or directory".

```sh
mkdir -p /var/vsomeip
chmod 777 /var/vsomeip
```

The `~50` in the filename controls boot order — runs after networking (~15)
and before any user apps.

---

### 3. `targets/rpi5/mkqnximage.config` — Two lines added

```diff
  SNIPPETS += system_files.custom.qt6
+ SNIPPETS += system_files.custom.commonapi
+ SNIPPETS += post_start.~50.commonapi
```

---

## Why These Changes Were Needed

### Versioned symlinks
The `HelloWorldClient` binary was linked against `libCommonAPI.so.3.2.4`
(the exact version built from source). The base QNX image only ships
`libCommonAPI.so.3` without the full version symlink, so `ldd` failed at
runtime with "unable to load".

### Boost libraries
`libvsomeip3.so` depends on `libboost_thread`, `libboost_filesystem`, and
others at runtime. These were not included in the base QNX image.

### `/etc/commonapi.ini`
Without this file, CommonAPI defaults to the `dbus` binding. Since QNX
does not run D-Bus, the client crashed immediately. Setting `binding=someip`
tells CommonAPI to load `libCommonAPI-SomeIP.so` instead.

### `/var/vsomeip/`
vsomeip's routing manager creates a Unix socket at `/var/vsomeip-0` for
internal communication between the routing host and proxy clients.
QNX does not create this directory by default.

### App name in vsomeip config
The vsomeip application name in `vsomeip-client.json` must exactly match
the name passed to `buildProxy()` in the C++ code. A mismatch causes vsomeip
to start as an unnamed `ffff` proxy that can never become a routing host,
resulting in infinite reconnection loops.

---

## How to Rebuild After Changes

```bash
# If you changed the HelloWorldClient binary
source ~/ITI/QNX/repo/qnx800/qnxsdp-env.sh
cmake --build ~/ITI/QNX/test_common_api/build-qnx -j$(nproc)

# If you changed vsomeip-client.json IP address
# edit Config/vsomeip-client.json directly

# Rebuild the image
cd ~/ITI/QNX/repo
make

# Flash to SD card
sudo bmaptool copy build/rpi5/rpi5.img /dev/sdX
```

---

## Running the System

**Lab PC (Server):**
```bash
export VSOMEIP_CONFIGURATION=~/ITI/QNX/test_common_api/Config/vsomeip-server.json
export VSOMEIP_APPLICATION_NAME=HelloWorldService
~/ITI/QNX/test_common_api/build-server-linux/HelloWorldServer
```

**RPi5 (Client) — runs automatically if added to post_start, or manually:**
```bash
export VSOMEIP_CONFIGURATION=/etc/vsomeip-client.json
export LD_LIBRARY_PATH=/usr/lib
export VSOMEIP_APPLICATION_NAME=HelloWorldClient
/usr/bin/HelloWorldClient
```
