# Pattern Race - Cross-Compilation for QNX 8.0 on Raspberry Pi 5

## Project Summary

| Item | Detail |
|------|--------|
| Game | Pattern Race (SDL2-based racing game) |
| Source | `https://gitlab.com/ad2lahav/pattern-race.git` |
| Target OS | QNX 8.0 (Neutrino RTOS) |
| Target HW | Raspberry Pi 5 (aarch64le) |
| Build System | CMake + QNX Toolchain |
| Dependencies | SDL2, SDL2_ttf, SDL2_image, freetype, brotli, zlib, libc++ |
| Builder | Ayman @ ITI |

---

## Files Created / Modified

| File | Action | Purpose |
|------|--------|---------|
| `src/Makefile` | Modified | Added clone + build + stage rules |
| `snippets/system_files.custom.pattern_race` | Created | Include binary + assets in image |

---

## Step 1: Source Code Structure

```
~/ITI/QNX/pattern-race/
├── CMakeLists.txt                              ← CMake build system
├── main.cpp                                    ← Entry point
├── functions.cpp / .h                          ← Game logic
├── platform.cpp / .h                           ← Platform rendering
├── icons.cpp / .h                              ← Icon rendering
├── mouse.cpp / .h                              ← Mouse input
├── globals.h / structs.h / player.h            ← Data structures
├── text.h / texture.h / timer.h                ← Helpers
├── assets/                                     ← Game textures + fonts
│   ├── ground.png, platform.png, title.png
│   ├── player_sheet.png, player_sheet_2.png
│   ├── icon_sheet.png, empty_icon.png
│   ├── help_button.png
│   ├── ground_outline.png, platform_outline.png
│   └── Roboto-Regular.ttf
├── qnx/                                       ← QNX cross-compile files
│   ├── aarch64le/
│   │   ├── cmake-toolchain-qnx-aarch64le.cmake
│   │   └── Makefile
│   └── x86_64/
└── LICENSE, README.md
```

---

## Step 2: Toolchain File Analysis

File: `qnx/aarch64le/cmake-toolchain-qnx-aarch64le.cmake`

```cmake
set(CMAKE_SYSTEM_NAME QNX)                          # Target OS = QNX
set(arch gcc_ntoaarch64le)                          # ARM64 little-endian

set(CMAKE_C_COMPILER qcc)                          # QNX C compiler
set(CMAKE_CXX_COMPILER q++)                        # QNX C++ compiler
set(CMAKE_C_COMPILER_TARGET ${arch})
set(CMAKE_CXX_COMPILER_TARGET ${arch})

set(CMAKE_SYSROOT $ENV{QNX_TARGET})                # Headers/libs from QNX SDP

set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)        # Only use QNX includes
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)        # Only use QNX libraries
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)        # Only use QNX packages
```

### What it does:
- Uses `qcc`/`q++` instead of `gcc`/`g++`
- Targets `gcc_ntoaarch64le` (QNX ARM64)
- Only searches QNX SDP for headers and libraries

---

## Step 3: Dependencies

### CMakeLists.txt requires:

```cmake
find_package(SDL2 REQUIRED)
find_package(Threads REQUIRED)
target_link_libraries(patrace SDL2 SDL2_ttf SDL2_image Threads::Threads)
```

### Full dependency chain:

```
patrace
├── libSDL2.so              (graphics/window/input)
├── libSDL2_image.so        (image loading: PNG)
├── libSDL2_ttf.so          (font rendering)
│   └── libfreetype.so      (font engine)
│       ├── libbrotlidec.so  (Brotli decompression)
│       │   └── libbrotlicommon.so
│       └── libz.so          (zlib compression)
├── libpthread.so           (threading)
└── libc++.so.2             (C++ standard library)
```

### Where each library lives:

```
libc++.so.2         → qnx800/target/qnx/aarch64le/usr/lib/
libSDL2.so          → src/stage/nto/aarch64le/usr/lib/
libSDL2_ttf.so      → src/stage/nto/aarch64le/usr/lib/
libSDL2_image.so    → src/stage/nto/aarch64le/usr/lib/
libfreetype.so      → src/stage/nto/aarch64le/usr/lib/
libbrotlidec.so     → apk/stage/apk_root/usr/lib/
libbrotlicommon.so  → apk/stage/apk_root/usr/lib/
libz.so             → qnx800/target/qnx/aarch64le/usr/lib/
```

---

## Step 4: Failed Attempts and Root Causes

### Attempt 1: Basic CMake

```bash
cmake -S ~/ITI/QNX/pattern-race -B . \
  -DCMAKE_TOOLCHAIN_FILE=.../cmake-toolchain-qnx-aarch64le.cmake \
  -DSDL2_DIR=.../src/stage/nto/aarch64le/usr/lib/cmake/SDL2
make
```

❌ **Failed:** `undefined reference to std::__2::cout`, `FT_Init_FreeType`

**Why:** CMake adds deprecated `-lang-c++` flag that breaks `q++` auto-linking.
No `-L` path to `qnx800/` where `libc++.so.2` lives.
Missing transitive dependencies (freetype → brotli → zlib).

---

### Attempt 2: Add `-lc++` `-lfreetype` via CMAKE_EXE_LINKER_FLAGS

```bash
cmake ... -DCMAKE_EXE_LINKER_FLAGS="-lfreetype -lc++"
```

❌ **Failed:** Same errors.

**Why:** `CMAKE_EXE_LINKER_FLAGS` is placed BEFORE `.o` files.
GNU linker processes left-to-right → sees `-lc++` before knowing
what symbols are needed → skips it.

```
BROKEN:  q++ -lc++ -lfreetype  *.o  -lSDL2 ...
              ↑ skipped          ↑ needs cout → too late!
```

---

### Attempt 3: Add libs via CMAKE_CXX_STANDARD_LIBRARIES

```bash
cmake ... -DCMAKE_CXX_STANDARD_LIBRARIES="-lc++ -lfreetype"
```

❌ **Failed:** Libraries in correct order, but linker cannot find them.

**Why:** No `-L` path to `qnx800/target/qnx/aarch64le/usr/lib/`.
Linker sees `-lc++` but doesn't know WHERE `libc++.so.2` is.

---

### ✅ Attempt 4: Full -L paths + ALL dependencies

```bash
cmake ... -DCMAKE_EXE_LINKER_FLAGS="\
  -L/home/ayman/ITI/QNX/repo/qnx800/target/qnx/aarch64le/usr/lib \
  -L$(STAGE_TARGET)/usr/lib \
  -L$(APK_STAGE_ROOT)/usr/lib \
  -lfreetype -lbrotlidec -lbrotlicommon -lz -lc++"
```

✅ **Success!**

**Why it works:**
- `-L qnx800/.../usr/lib` → linker finds `libc++.so.2`
- `-L stage/.../usr/lib` → linker finds SDL2 libraries
- `-L apk/.../usr/lib` → linker finds brotli libraries
- All transitive dependencies explicitly listed

---

## Step 5: Final Makefile Entry

Added to `src/Makefile`:

```makefile
source/pattern-race-ready:
	mkdir -p source
	cd source && git clone https://gitlab.com/ad2lahav/pattern-race.git
	cd source/pattern-race && git checkout $(PATTERN_RACE_SHA)
	touch $@

source/pattern-race-built-$(QNX_ARCH): source/pattern-race-ready source/simple-terminal-built-$(QNX_ARCH)
	mkdir -p source/pattern-race/build-$(QNX_ARCH)
	cd source/pattern-race/build-$(QNX_ARCH) && cmake .. \
		-DCMAKE_TOOLCHAIN_FILE=../qnx/$(QNX_ARCHDIR)/cmake-toolchain-qnx-$(QNX_ARCHDIR).cmake \
		-DSDL2_DIR=$(STAGE_TARGET)/usr/lib/cmake/SDL2 \
		-DCMAKE_EXE_LINKER_FLAGS="-L$(QNX_TARGET)/$(QNX_ARCHDIR)/usr/lib \
		-L$(STAGE_TARGET)/usr/lib \
		-L$(APK_STAGE_ROOT)/usr/lib \
		-lfreetype -lbrotlidec -lbrotlicommon -lz -lc++"
	cd source/pattern-race/build-$(QNX_ARCH) && make -j4
	mkdir -p $(STAGE_TARGET)/usr/bin/pattern-race
	cp source/pattern-race/build-$(QNX_ARCH)/patrace $(STAGE_TARGET)/usr/bin/pattern-race/
	cp -r source/pattern-race/assets $(STAGE_TARGET)/usr/bin/pattern-race/
	touch $@
```

### Line-by-line:

| Line | What it does |
|------|-------------|
| `source/pattern-race-ready` | Clone repo + checkout specific commit |
| `mkdir -p .../build-$(QNX_ARCH)` | Create build directory |
| `cmake ...` | Configure with QNX toolchain + SDL2 + linker flags |
| `make -j4` | Compile with 4 parallel jobs |
| `mkdir -p .../pattern-race` | Create stage output directory |
| `cp .../patrace ...` | Copy binary to stage |
| `cp -r .../assets ...` | Copy game assets to stage |
| `touch $@` | Create flag file (skip rebuild next time) |

---

## Step 6: Snippet for Image Inclusion

File: `snippets/system_files.custom.pattern_race`

```
[type=file uid=0 gid=0 perms=0755]
    usr/bin/pattern-race/patrace = usr/bin/pattern-race/patrace

[type=file uid=0 gid=0 perms=0644]
    usr/bin/pattern-race/assets/help_button.png = usr/bin/pattern-race/assets/help_button.png
    usr/bin/pattern-race/assets/player_sheet_2.png = usr/bin/pattern-race/assets/player_sheet_2.png
    usr/bin/pattern-race/assets/ground_outline.png = usr/bin/pattern-race/assets/ground_outline.png
    usr/bin/pattern-race/assets/player_sheet.png = usr/bin/pattern-race/assets/player_sheet.png
    usr/bin/pattern-race/assets/ground.png = usr/bin/pattern-race/assets/ground.png
    usr/bin/pattern-race/assets/Roboto-Regular.ttf = usr/bin/pattern-race/assets/Roboto-Regular.ttf
    usr/bin/pattern-race/assets/empty_icon.png = usr/bin/pattern-race/assets/empty_icon.png
    usr/bin/pattern-race/assets/title.png = usr/bin/pattern-race/assets/title.png
    usr/bin/pattern-race/assets/platform_outline.png = usr/bin/pattern-race/assets/platform_outline.png
    usr/bin/pattern-race/assets/icon_sheet.png = usr/bin/pattern-race/assets/icon_sheet.png
    usr/bin/pattern-race/assets/platform.png = usr/bin/pattern-race/assets/platform.png
```

### Snippet format:

```
[type=file uid=0 gid=0 perms=XXXX]        ← file attributes
    destination/on/image = source/in/stage  ← where to put it

perms=0755 → rwxr-xr-x (executable binary)
perms=0644 → rw-r--r-- (read-only assets)
```

---

## Step 7: Build and Verify

```bash
# Build the full image
cd ~/ITI/QNX/repo
make

# Verify pattern-race is in system.build
grep "pattern-race" build/rpi5/output/build/system.build

# Verify all 12 files are included
grep -c "pattern-race" build/rpi5/output/build/system.build
# Expected output: 12
```

---

## Step 8: Run on RPi5

```bash
# Flash SD card
sudo dd if=~/ITI/QNX/repo/build/rpi5/rpi5.img of=/dev/sdX bs=4M status=progress

# SSH to RPi5
ssh qnxuser@aymanqnx

# Run the game
cd /system/usr/bin/pattern-race
./patrace
```

---

## Verification One-Liner

```bash
MYFILE="pattern-race" && echo "=== STAGE ===" && \
find ~/ITI/QNX/repo/src/stage/ -path "*${MYFILE}*" -type f 2>/dev/null && \
echo "=== SNIPPET ===" && \
grep -rn "$MYFILE" ~/ITI/QNX/repo/snippets/ 2>/dev/null && \
echo "=== BUILD FILE ===" && \
grep "$MYFILE" ~/ITI/QNX/repo/build/rpi5/output/build/system.build 2>/dev/null && \
echo "=== TIMESTAMPS ===" && \
echo -n "Snippet: " && stat ~/ITI/QNX/repo/snippets/system_files.custom.pattern_race 2>/dev/null | grep Modify && \
echo -n "Image:   " && stat ~/ITI/QNX/repo/build/rpi5/rpi5.img 2>/dev/null | grep Modify
```

---

## Key Lessons

| # | Lesson |
|---|--------|
| 1 | QNX 8.0 uses `libc++` (LLVM) with `std::__2::` symbol prefix |
| 2 | CMake adds deprecated `-lang-c++` for QCC — breaks auto-linking |
| 3 | Must provide explicit `-L` path to `qnx800/target/qnx/aarch64le/usr/lib/` |
| 4 | GNU linker is order-sensitive: `-l` flags must come AFTER `.o` files |
| 5 | Must list ALL transitive deps: `SDL2_ttf → freetype → brotli → zlib` |
| 6 | Snippet must be created BEFORE running `make` to rebuild image |
| 7 | Binary needs `perms=0755`, assets need `perms=0644` in snippets |
| 8 | Use `grep` on `.build` files to verify inclusion in image |
```
