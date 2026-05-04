# How to Verify Your Changes Made It Into the QNX Image

## Quick Reference Commands

All commands run from: `~/ITI/QNX/repo/`

---

## Step 1: Check if the Binary Exists in Stage

Before anything gets into the image, it must be in the **stage** directory.

```bash
# For src/ built binaries:
ls -la src/stage/nto/aarch64le/usr/local/bin/<YOUR_BINARY>

# For APK installed binaries:
ls -la apk/stage/apk_root/usr/bin/<YOUR_BINARY>

# For system/ pre-built files:
ls -la system/usr/bin/<YOUR_BINARY>

# Generic search (if you don't know where it is):
find src/stage/ apk/stage/ system/ qnx800/target/qnx/ -name "<YOUR_BINARY>" 2>/dev/null
```

### Example:

```bash
ls -la src/stage/nto/aarch64le/usr/local/bin/test_qnx
ls -la src/stage/nto/aarch64le/usr/bin/pattern-race/patrace
```

**If NOT found** → the build step failed or never ran.
Check the flag file:

```bash
ls -la src/source/<PROJECT>-built-aarch64
# If missing → run: cd src && make
```

---

## Step 2: Check if the Snippet Exists

The snippet tells mkqnximage to include the file in the image.

```bash
# Search in common snippets:
grep -rn "<YOUR_BINARY>" snippets/

# Search in target-specific snippets:
grep -rn "<YOUR_BINARY>" targets/rpi5/snippets/

# Search in APK auto-generated snippets:
grep -rn "<YOUR_BINARY>" apk/stage/snippets/
```

### Example:

```bash
grep -rn "test_qnx" snippets/
grep -rn "patrace" snippets/
```

**If NOT found** → you need to create a snippet file.

---

## Step 3: Check if the Snippet Was Copied to Build Directory

During `make`, all snippets are collected into `build/rpi5/local/snippets/`.

```bash
# Check if your snippet was copied:
grep -rn "<YOUR_BINARY>" build/rpi5/local/snippets/

# Show the processed snippet content:
cat build/rpi5/local/snippets/<SNIPPET_FILENAME>

# Check for syntax errors (hidden characters):
cat -A build/rpi5/local/snippets/<SNIPPET_FILENAME>
```

### Example:

```bash
grep -rn "test_qnx" build/rpi5/local/snippets/
cat -A build/rpi5/local/snippets/system_files.custom.test_qnx
```

---

## Step 4: Check if It Made It Into the .build File

Snippets are concatenated into `.build` files based on their **prefix**:

| Snippet Prefix | Build File | Partition |
|---------------|------------|-----------|
| `ifs_*` | `ifs.build` | IFS (RAM, read-only) |
| `system_files.*` | `system.build` | System (QNX6, read-only) |
| `data_files.*` | `data.build` | Data (QNX6, writable) |

```bash
# Search in the correct .build file based on your snippet prefix:
grep "<YOUR_BINARY>" build/rpi5/output/build/system.build
grep "<YOUR_BINARY>" build/rpi5/output/build/data.build
grep "<YOUR_BINARY>" build/rpi5/output/build/ifs.build

# Or search ALL .build files at once:
grep -rn "<YOUR_BINARY>" build/rpi5/output/build/*.build
```

### Example:

```bash
grep "test_qnx" build/rpi5/output/build/system.build
grep "patrace" build/rpi5/output/build/system.build
```

**If NOT found** → snippet exists but wasn't included.
Check the snippet filename format and syntax.

---

## Step 5: Check Timestamps (Most Common Issue!)

```bash
# When was the snippet last modified?
stat snippets/<SNIPPET_FILE> | grep Modify

# When was the image built?
stat build/rpi5/rpi5.img | grep Modify
```

### The Rule:

```
Image timestamp MUST be NEWER than snippet timestamp!

✅ Snippet: 12:53  →  Image: 12:56  (image built AFTER snippet)
❌ Snippet: 12:53  →  Image: 12:30  (image built BEFORE snippet)
```

**If image is OLDER** → run `make` to rebuild.

---

## Full Verification One-Liner

Replace `MYFILE` with your binary name:

```bash
MYFILE="test_qnx" && echo "=== STAGE ===" && \
find src/stage/ apk/stage/ system/ -name "$MYFILE" 2>/dev/null && \
echo "=== SNIPPETS ===" && \
grep -rn "$MYFILE" snippets/ targets/rpi5/snippets/ 2>/dev/null && \
echo "=== BUILD SNIPPETS ===" && \
grep -rn "$MYFILE" build/rpi5/local/snippets/ 2>/dev/null && \
echo "=== BUILD FILES ===" && \
grep -rn "$MYFILE" build/rpi5/output/build/*.build 2>/dev/null && \
echo "=== TIMESTAMPS ===" && \
echo -n "Snippet: " && stat snippets/system_files.custom.$MYFILE 2>/dev/null | grep Modify && \
echo -n "Image:   " && stat build/rpi5/rpi5.img 2>/dev/null | grep Modify
```

---

## Troubleshooting Flowchart

```
Is binary in stage/?
├── NO  → Run: cd src && make
│         Check flag file: ls src/source/<project>-built-aarch64
│
└── YES → Does snippet exist in snippets/?
          ├── NO  → Create snippet file:
          │         snippets/system_files.custom.<name>
          │
          └── YES → Is snippet in build/rpi5/local/snippets/?
                    ├── NO  → Snippet filename wrong or copy failed
                    │         Run: make (rebuilds everything)
                    │
                    └── YES → Is binary in .build file?
                              ├── NO  → Check snippet syntax:
                              │         cat -A snippets/<file>
                              │         Look for hidden chars or duplicates
                              │
                              └── YES → Is image NEWER than snippet?
                                        ├── NO  → Run: make
                                        └── YES → ✅ Binary is in the image!
```

---

## Common Mistakes

| # | Mistake | Fix |
|---|---------|-----|
| 1 | Created snippet AFTER `make` | Run `make` again |
| 2 | Missing filename in snippet source path | `usr/local/bin/` → `usr/local/bin/test_qnx` |
| 3 | Duplicate attributes on same line | Put `[type=file ...]` on its own line |
| 4 | Wrong snippet prefix | `system_files.*` for system partition, `data_files.*` for data |
| 5 | Binary not in any OPT_REPOS path | Check `mkqnximage.config` OPT_REPOS paths |
```
