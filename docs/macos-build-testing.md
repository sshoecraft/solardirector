# macOS Build Testing Guide

## Overview
This guide helps test the macOS compilation fixes that were implemented to resolve the build system issues where the project was incorrectly using Linux settings and gcc instead of macOS-specific configuration with clang.

## Changes Made
1. **Makefile.sys**: Added automatic macOS detection and clang configuration
2. **lib/Makefile**: Fixed CPU detection to use `sysctl` instead of `/proc/cpuinfo`
3. **Build System**: Added Homebrew and MacPorts library path support

## Testing Steps

### 1. Verify Platform Detection
```bash
# Test that the build system detects macOS correctly
cd /path/to/solardirector
make -n | head -20
```
**Expected**: Should show `clang` being used instead of `gcc`

### 2. Check Compiler Configuration
```bash
# Verify the correct compiler and flags are set
cd lib/js
make -n
```
**Expected**: Should use `clang` with macOS-specific flags like `-DMACOS -D_DARWIN_C_SOURCE`

### 3. Test Library Path Detection
Look for these paths being included automatically:
- Homebrew: `/opt/homebrew/include` and `/opt/homebrew/lib`
- MacPorts: `/opt/local/include` and `/opt/local/lib`
- Standard: `/usr/local/include` and `/usr/local/lib`

### 4. Test JavaScript Library Compilation
```bash
# Clean and rebuild the JavaScript library
cd lib/js
make clean
make
```
**Expected**: 
- Should compile without "undefined symbol" errors
- Should create `libjs.a` successfully
- `jsfile.c` should compile and include File class symbols

### 5. Verify Symbol Resolution
```bash
# Check if the missing symbols are now present
cd lib/js
nm libjs.a | grep -i "js_.*file"
```
**Expected**: Should show symbols like `_js_FileClass` and `_js_InitFileClass`

### 6. Test Full Project Build
```bash
# Try building the entire project
cd /path/to/solardirector
make clean
make
```

## Troubleshooting

### If Platform Detection Fails
Check that `uname -s` returns "Darwin":
```bash
uname -s
```

### If Clang Not Found
Ensure Xcode command line tools are installed:
```bash
xcode-select --install
```

### If Library Paths Not Found
Install dependencies via Homebrew or MacPorts:
```bash
# Homebrew
brew install openssl curl readline

# MacPorts  
sudo port install openssl curl readline
```

### If JavaScript Symbols Still Missing
1. Verify `jsfile.c` is in the SRCS list in `lib/js/Makefile`
2. Check that the file compiles successfully
3. Ensure the library archive includes the object file

## Success Criteria
✅ Build uses `clang` instead of `gcc`  
✅ macOS-specific flags are applied  
✅ Library paths are automatically detected  
✅ JavaScript library compiles without undefined symbols  
✅ Full project builds successfully on macOS  

## Reporting Issues
If any step fails, please report:
1. The specific command that failed
2. Complete error output
3. Output of `uname -a` and `clang --version`
4. Which package managers you have installed (Homebrew/MacPorts)

---
*Created: 2025-06-06 as part of macOS compilation support fixes*
