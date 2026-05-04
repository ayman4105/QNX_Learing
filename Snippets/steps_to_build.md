# QNX RPi5 Image Build - Complete Guide (A to Z)

---

## Prerequisites

### Host Packages
```bash
sudo apt install scdoc imagemagick bmap-tools
```

### Git Config (prevent clone failures on slow connections)
```bash
git config --global http.postBuffer 524288000
git config --global http.version HTTP/1.1
```

---

## Step 1: Navigate to Project
```bash
cd ~/ITI/QNX/repo
```

## Step 2: Create Symlink for QNX SDP
```bash
ln -s ~/qnx800 qnx800
```

## Step 3: Create Config File
```bash
mkdir -p qnx
nano qnx/config.mk
```
```makefile
export QSC_CLT_PATH := /home/ayman/qnx/qnxsoftwarecenter/qnxsoftwarecenter_clt
```

## Step 4: Create Options File
```bash
touch options_file
```
If authentication required:
```
-user YOUR_EMAIL@example.com
-password YOUR_PASSWORD
```

## Step 5: Fix SLM Bug (slm.~60.custom.network)
```bash
sed -i 's|apk_start/SLM:depend>|apk_start</SLM:depend>|g' \
  snippets/slm.~60.custom.network
```

## Step 6: Pre-Clone Large Repos (avoid timeout issues)
```bash
# Meson for apk
cd ~/ITI/QNX/repo/apk/stage
mkdir -p ~/ITI/QNX/repo/apk/stage
git clone --depth 1 https://github.com/mesonbuild/meson.git

# Meson for src (specific version)
mkdir -p ~/ITI/QNX/repo/src/source
cd ~/ITI/QNX/repo/src/source
git clone --depth 1 --branch 1.8.1 https://github.com/mesonbuild/meson.git
touch ~/ITI/QNX/repo/src/source/stage-ready

# Maelstrom
mkdir -p ~/ITI/QNX/repo/src/qnxuser_projects
cd ~/ITI/QNX/repo/src/qnxuser_projects
git clone --depth 1 https://github.com/libsdl-org/Maelstrom.git -b Maelstrom3
```

## Step 7: Build the Image
```bash
cd ~/ITI/QNX/repo
make all TARGET=rpi5 2>&1 | tee build_log.txt
```

Expected output:
```
Image generation is successful.
```

Verify:
```bash
ls -lh build/rpi5/rpi5.img
```

## Step 8: Flash to SD Card

### Identify SD Card
```bash
lsblk
```
Look for the SD Card (e.g., `/dev/sdb`).

### Unmount & Flash
```bash
sudo umount /media/ayman/* 2>/dev/null
sudo dd if=~/ITI/QNX/repo/build/rpi5/rpi5.img \
       of=/dev/sdX \
       bs=4M \
       status=progress
sync
```
> **⚠️ Replace `/dev/sdX` with your actual SD Card device!**

## Step 9: Boot RPi5

1. Insert SD Card into RPi5
2. Connect Serial (USB-TTL):
   ```
   USB-TTL TX  → RPi5 GPIO 15 (Pin 10 / RX)
   USB-TTL RX  → RPi5 GPIO 14 (Pin 8 / TX)
   USB-TTL GND → RPi5 Pin 6 (GND)
   ```
3. Open minicom:
   ```bash
   minicom -D /dev/ttyUSB0 -b 115200
   ```
   Settings: 115200 8N1, No Flow Control
4. Power on RPi5
5. Login:
   ```
   login: qnxuser
   password: qnxuser
   ```

---

## Troubleshooting

### Issue: `options_file` not found
```bash
touch ~/ITI/QNX/repo/options_file
```

### Issue: `scdoc` not found
```bash
sudo apt install scdoc
```

### Issue: `convert` not found
```bash
sudo apt install imagemagick
```

### Issue: Git clone hangs or fails
```bash
git config --global http.postBuffer 524288000
git config --global http.version HTTP/1.1
# Clone manually with --depth 1
```

### Issue: `apk-tools already exists`
```bash
rm -rf ~/ITI/QNX/repo/apk/stage/apk-tools
rm -rf ~/ITI/QNX/repo/apk/stage/apk-tools-ready
```

### Issue: `meson already exists` (src)
```bash
rm -rf ~/ITI/QNX/repo/src/source/meson
rm -rf ~/ITI/QNX/repo/src/source/stage-ready
# Re-clone and touch stage-ready
```

### Issue: `QSC_CLT_PATH` not passed to sub-make
Use `export` in `qnx/config.mk`:
```makefile
export QSC_CLT_PATH := /path/to/qnxsoftwarecenter_clt
```

### Issue: SLM error at line 54
```bash
sed -i 's|apk_start/SLM:depend>|apk_start</SLM:depend>|g' \
  snippets/slm.~60.custom.network
```

### Issue: Google Fonts zip download fails
Download on stable connection (mobile hotspot):
```bash
cd ~/ITI/QNX/repo/assets/fonts/build
wget --tries=10 \
  -O google_fonts_48d15b3193a0d26077155ab1a174c148c9ba6f06.zip \
  https://github.com/google/fonts/archive/48d15b3193a0d26077155ab1a174c148c9ba6f06.zip
```

---

## Useful Make Commands

| Command | Description |
|---------|-------------|
| `make all TARGET=rpi5` | Full build |
| `make boot TARGET=rpi5` | Build boot only |
| `make src TARGET=rpi5` | Build source only |
| `make apk TARGET=rpi5` | Build APK packages only |
| `make assets` | Build assets only |
| `make clean TARGET=rpi5` | Clean everything (re-downloads SDP!) |

> **Warning:** `make clean` removes the SDP. Avoid unless necessary.
> **Note:** Second build is much faster — completed steps are cached.
