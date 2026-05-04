# QNX RPi5 Image Build - Quick Setup Guide

---

## 1. Prerequisites

- QNX SDP 8.0 installed at `~/qnx800/`
- QNX Software Center CLI installed at `~/qnx/qnxsoftwarecenter/`
- Internet connection (first build downloads ~4-6 GB of packages)

---

## 2. Clone & Navigate to Project

```bash
cd ~/ITI/QNX/repo
```

## 3. Create Symlink for QNX SDP

The Makefile expects `qnx800/` inside the project directory.

```bash
ln -s ~/qnx800 qnx800
```

Verify:
```bash
ls -la qnx800
# Expected: qnx800 -> /home/ayman/qnx800
```

---

## 4. Create Config File

Set `QSC_CLT_PATH` without modifying the tracked Makefile.

```bash
mkdir -p qnx
nano qnx/config.mk
```

Add this line:
```makefile
QSC_CLT_PATH := /home/ayman/qnx/qnxsoftwarecenter/qnxsoftwarecenter_clt
```

Verify:
```bash
cat qnx/config.mk
```

---

## 5. Check Available Targets

```bash
ls targets/
# Output: qemu  README.md  rpi4  rpi5
```

---

## 6. Build the Image

```bash
make all TARGET=rpi5 2>&1 | tee build_log.txt
```

| Flag | Purpose |
|------|---------|
| `TARGET=rpi5` | Build for Raspberry Pi 5 |
| `2>&1` | Redirect errors to stdout |
| `tee build_log.txt` | Display output + save to file |

---

## 7. Build Output

```
build/rpi5/rpi5.img    # Final SD Card image
```

---

## 8. Useful Make Commands

| Command | Description |
|---------|-------------|
| `make all TARGET=rpi5` | Full build |
| `make boot TARGET=rpi5` | Build boot only |
| `make src TARGET=rpi5` | Build source only |
| `make apk TARGET=rpi5` | Build APK packages only |
| `make assets` | Build assets only |
| `make clean TARGET=rpi5` | Clean everything (re-downloads SDP!) |

> **Warning:** `make clean` removes the SDP. Avoid unless necessary.

---

## 9. Snippet Locations (Priority Order)

Snippets are copied in order. **Last one wins** (override).

| Priority | Location | Description |
|----------|----------|-------------|
| 1 (lowest) | `snippets/` | Common snippets for all targets |
| 2 | `apk/stage/snippets/` | APK-generated snippets |
| 3 (highest) | `targets/rpi5/snippets/` | RPi5-specific snippets (overrides) |

---

## 10. Key Files Reference

| File | Purpose |
|------|---------|
| `Makefile` | Main build rules (do NOT edit) |
| `qnx/config.mk` | Your local config (not tracked) |
| `make_image.sh` | Image generation wrapper script |
| `qsc_install_packages.list` | Common packages list |
| `targets/rpi5/qsc_install_packages.list` | RPi5-specific packages |
| `targets/rpi5/variables.mk` | RPi5 build variables |
| `targets/rpi5/rules.mk` | RPi5 build rules |


---

## Issue 1: `options_file` Not Found

**Error:**

make: *** No rule to make target 'options_file', needed by '.../qnx800'. Stop.


**Cause:** The Makefile expects an `options_file` for QNX Software Center authentication.

**Solution:**
```bash
cd ~/ITI/QNX/repo
touch options_file

If authentication is required, add your QNX account credentials:

bash

nano options_file

text

-user YOUR_EMAIL@example.com
-password YOUR_PASSWORD

```


## Issue 2: Meson Clone Hangs or Fails

**Error:**

error: RPC failed; curl 92 HTTP/2 stream 5 was not closed cleanly: CANCEL (err 8)

Or clone hangs indefinitely at:
```bash

cd apk/stage && git clone https://github.com/mesonbuild/meson.git
```
Cloning into 'meson'...

Cause: Meson repo is large. Full clone may timeout or hang on slow connections.

**Solution:**

```bash

# Fix git config
git config --global http.postBuffer 524288000
git config --global http.version HTTP/1.1

# Clone meson manually with --depth 1
cd ~/ITI/QNX/repo/apk/stage
rm -rf meson
git clone --depth 1 https://github.com/mesonbuild/meson.git

# Return and rebuild
cd ~/ITI/QNX/repo
make all TARGET=rpi5 2>&1 | tee build_log.txt
```

## Issue 3: apk-tools already exists

**Error:**

fatal: destination path 'apk-tools' already exists and is not an empty directory.
make[1]: *** [Makefile:160: .../apk-tools-ready] Error 128

Cause: Previous failed build left apk-tools directory without completing setup.

**Solution:**

```bash

rm -rf ~/ITI/QNX/repo/apk/stage/apk-tools
rm -rf ~/ITI/QNX/repo/apk/stage/apk-tools-ready

cd ~/ITI/QNX/repo
make all TARGET=rpi5 2>&1 | tee build_log.txt

General Tip: Clean Rebuild

If multiple issues persist:

bash

# Nuclear option - clean apk stage completely
rm -rf ~/ITI/QNX/repo/apk/stage/apk-tools
rm -rf ~/ITI/QNX/repo/apk/stage/apk-tools-ready
rm -rf ~/ITI/QNX/repo/apk/stage/meson

# Pre-clone meson with --depth 1
cd ~/ITI/QNX/repo/apk/stage
git clone --depth 1 https://github.com/mesonbuild/meson.git

# Build
cd ~/ITI/QNX/repo
make all TARGET=rpi5 2>&1 | tee build_log.txt
```

##Note: Completed steps (SDP download, SSL certs) are cached and won't re-download.


## Issue 1: `options_file` Not Found

**Error:**
```
make: *** No rule to make target 'options_file', needed by '.../qnx800'. Stop.
```

**Solution:**
```bash
cd ~/ITI/QNX/repo
touch options_file
```

If authentication is required:
```bash
nano options_file
```
```
-user YOUR_EMAIL@example.com
-password YOUR_PASSWORD
```

---

## Issue 2: `scdoc` Not Found

**Error:**
```
/bin/sh: 1: scdoc: not found
make[2]: *** [Makefile:76: newapkbuild.1] Error 127
```

**Solution:**
```bash
sudo apt install scdoc
```

---

## Issue 3: Meson Clone Hangs or Fails

**Error:**
```
error: RPC failed; curl 92 HTTP/2 stream 5 was not closed cleanly: CANCEL (err 8)
```

**Solution:**
```bash
git config --global http.postBuffer 524288000
git config --global http.version HTTP/1.1

cd ~/ITI/QNX/repo/apk/stage
rm -rf meson
git clone --depth 1 https://github.com/mesonbuild/meson.git
```

---

## Issue 4: `apk-tools already exists`

**Error:**
```
fatal: destination path 'apk-tools' already exists and is not an empty directory.
```

**Solution:**
```bash
rm -rf ~/ITI/QNX/repo/apk/stage/apk-tools
rm -rf ~/ITI/QNX/repo/apk/stage/apk-tools-ready
```

---

## Issue 5: `QSC_CLT_PATH` Not Passed to Sub-Make

**Error:**
```
--swc-cli-path  \
realpath: unrecognized option '--swc-cli-options'
```

**Cause:** `QSC_CLT_PATH` not exported to sub-make processes.

**Solution:** Edit `qnx/config.mk`:
```bash
nano ~/ITI/QNX/repo/qnx/config.mk
```
```makefile
export QSC_CLT_PATH := /home/ayman/qnx/qnxsoftwarecenter/qnxsoftwarecenter_clt
```

> **Note:** The `export` keyword ensures the variable is visible to sub-make processes.

---

## Clean Rebuild (If Multiple Issues)

```bash
rm -rf ~/ITI/QNX/repo/apk/stage/apk-tools
rm -rf ~/ITI/QNX/repo/apk/stage/apk-tools-ready
rm -rf ~/ITI/QNX/repo/apk/stage/meson

cd ~/ITI/QNX/repo/apk/stage
git clone --depth 1 https://github.com/mesonbuild/meson.git

cd ~/ITI/QNX/repo
make all TARGET=rpi5 2>&1 | tee build_log.txt
```

> **Note:** Completed steps (SDP download, SSL certs) are cached and won't re-download.

