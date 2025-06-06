# macOS Compilation Fix Guide

## Issue Summary
The compilation failure on macOS arm64 is due to missing symbols in the JavaScript and SD libraries:

### Missing Symbols:
1. `_js_FileClass` and `_js_InitFileClass` - JavaScript File class symbols
2. `_tmpdir` - Temporary directory function

## Root Cause Analysis
The issue occurs because the libraries need to be rebuilt for the arm64 architecture on macOS. The existing libraries were likely compiled for x86_64 or Linux.

## Solution Steps

### 1. Clean and Rebuild Libraries
Before compiling the agents, ensure all libraries are rebuilt for macOS arm64:

```bash
# Clean all libraries first
cd /Users/steve/src/sd
make clean

# Rebuild libraries in correct order
cd lib/js
make clean && make

cd ../sd  
make clean && make

# Then rebuild agents
cd ../../agents/si
make clean && make
```

### 2. Force Rebuild with Correct Architecture
If the above doesn't work, force rebuild everything:

```bash
# From project root
make clean
make -j $(sysctl -n hw.ncpu)
```

### 3. Check Library Dependencies
Verify the libraries are properly linked:

```bash
# Check if libraries exist and are correct architecture
file lib/js/libjs.a
file lib/sd/libsd-js-mqtt-influx.a

# Should show "ar archive" for arm64 if built correctly
```

### 4. Debug Missing Symbols
If symbols are still missing after rebuild:

```bash
# Check if jsfile.c is being compiled
cd lib/js
make -n | grep jsfile

# Check if tmpdir.c is being compiled  
cd ../sd
make -n | grep tmpdir

# Verify symbols in library
nm libjs.a | grep js_FileClass
nm libsd-js-mqtt-influx.a | grep tmpdir
```

### 5. Alternative: Disable File Support Temporarily
If the File class isn't critical, you can disable it by modifying the JavaScript configuration:

Edit `lib/js/jsconfig.h` and comment out file support:
```c
// #define JS_HAS_FILE_OBJECT 1
```

## Prevention
- Always run `make clean` when switching platforms
- Ensure all dependencies (paho.mqtt.c, gattlib) are also built for arm64
- Use the updated Makefile.sys which now has proper macOS support

## Verification
After fixes, compilation should succeed without undefined symbol errors.
