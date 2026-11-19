# Complete Guide to Compile LLVM in VS Code

## Prerequisites & System Requirements

### Hardware Requirements
- **RAM:** 8GB minimum (16GB+ recommended for faster builds)
- **Disk Space:** 30-50GB free (10GB for source, 20-40GB for build)
- **CPU:** Multi-core processor (build time varies by cores)

### Software Requirements
- **OS:** Linux (Ubuntu/Debian/Fedora), macOS, or Windows (with MSYS2/WSL2)
- **VS Code:** Latest version
- **CMake:** 3.13.4 or newer
- **Git:** For cloning LLVM repository
- **Compiler:** GCC 7.1+, Clang 5.0+, MSVC 2019+, or Apple Clang
- **Build Tools:** Make or Ninja (Ninja is faster)

---

## Step 1: Install Dependencies

### **Ubuntu/Debian**
```bash
sudo apt update
sudo apt install -y \
  build-essential \
  cmake \
  git \
  ninja-build \
  python3 \
  python3-dev \
  zlib1g-dev \
  libssl-dev \
  libffi-dev
```

### **Fedora/RHEL**
```bash
sudo dnf groupinstall -y "Development Tools"
sudo dnf install -y \
  cmake \
  git \
  ninja-build \
  python3-devel \
  zlib-devel \
  openssl-devel
```

### **macOS**
```bash
# Install Xcode Command Line Tools
xcode-select --install

# Install via Homebrew
brew install cmake git ninja python@3.11
```

### **Windows (WSL2 / Ubuntu on Windows)**
```bash
# Run the Ubuntu/Debian commands above
```

---

## Step 2: Clone LLVM Repository

### **Standard Clone**
```bash
# Create a working directory
mkdir -p ~/llvm-project
cd ~/llvm-project

# Clone the full LLVM repository (large download)
git clone https://github.com/llvm/llvm-project.git
cd llvm-project

# List available branches
git branch -a

# (Optional) Switch to a specific release
# git checkout release/17.x
```

### **Shallow Clone (Faster for Latest)**
```bash
# Clone only recent history
git clone --depth 1 https://github.com/llvm/llvm-project.git
cd llvm-project
```

### **Directory Structure**
```
llvm-project/
├── llvm/              # Core LLVM compiler
├── clang/             # C/C++/Objective-C frontend
├── clang-tools-extra/ # Additional tools
├── compiler-rt/       # Runtime libraries
├── libcxx/            # C++ Standard Library
├── lld/               # Linker
├── lldb/              # Debugger
└── CMakeLists.txt     # Build configuration
```

---

## Step 3: Configure CMake

### **Create Build Directory**
```bash
cd ~/llvm-project/llvm-project
mkdir build
cd build
```

### **Basic CMake Configuration**
```bash
cmake -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=~/llvm-install \
  ../llvm
```

### **Optimized Configuration (Recommended)**
```bash
cmake -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=~/llvm-install \
  -DCMAKE_C_COMPILER=gcc \
  -DCMAKE_CXX_COMPILER=g++ \
  -DCMAKE_CXX_FLAGS="-O3 -march=native" \
  -DLLVM_ENABLE_PROJECTS="clang;clang-tools-extra;lld;lldb" \
  -DLLVM_PARALLEL_COMPILE_JOBS=4 \
  -DLLVM_PARALLEL_LINK_JOBS=2 \
  -DLLVM_USE_LINKER=lld \
  ../llvm
```

### **Key CMake Options Explained**

| Option | Purpose | Example |
|--------|---------|---------|
| `CMAKE_BUILD_TYPE` | Build configuration | `Release`, `Debug`, `RelWithDebInfo` |
| `CMAKE_INSTALL_PREFIX` | Installation directory | `~/llvm-install` |
| `LLVM_ENABLE_PROJECTS` | Subprojects to build | `clang;lld;lldb` |
| `LLVM_TARGETS_TO_BUILD` | CPU architectures | `X86;ARM;MIPS` (all = default) |
| `LLVM_PARALLEL_COMPILE_JOBS` | Compile parallelism | Number of cores |
| `LLVM_ENABLE_ASSERTIONS` | Runtime checks | `ON`, `OFF` |
| `LLVM_ENABLE_RTTI` | Runtime type info | `ON`, `OFF` |
| `BUILD_SHARED_LIBS` | Build as .so/.dylib | `ON`, `OFF` |

### **Common Configurations**

**Minimal Build (Fast)**
```bash
cmake -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=~/llvm-install \
  -DLLVM_TARGETS_TO_BUILD="X86" \
  ../llvm
```

**Full Build (Comprehensive)**
```bash
cmake -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=~/llvm-install \
  -DLLVM_ENABLE_PROJECTS="all" \
  -DLLVM_ENABLE_RUNTIMES="all" \
  ../llvm
```

**Debug Build (Development)**
```bash
cmake -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_INSTALL_PREFIX=~/llvm-install \
  -DLLVM_ENABLE_ASSERTIONS=ON \
  -DLLVM_ENABLE_PROJECTS="clang;lldb" \
  ../llvm
```

---

## Step 4: Build LLVM

### **Using Ninja (Fastest)**
```bash
cd ~/llvm-project/llvm-project/build

# Build all targets
ninja

# Build specific target (faster if you only need Clang)
ninja clang

# Build with progress indicator
ninja -v

# Show build commands
ninja -d keeprsp
```

### **Using Make (Slower but Universal)**
```bash
cd ~/llvm-project/llvm-project/build

# Build all
make -j4  # Use 4 cores

# Build specific target
make clang
```

### **Build Time Estimates**
- **Minimal (X86 only):** 15-30 minutes (4 cores)
- **Standard (Clang + tools):** 45-90 minutes
- **Full (all projects):** 2-4 hours

### **Monitor Build Progress**
```bash
# Watch build status
watch -n 5 'ps aux | grep -E "c\+\+|cc1plus"'

# Or use system monitor
htop
```

---

## Step 5: Install LLVM

```bash
cd ~/llvm-project/llvm-project/build

# Install to prefix directory
ninja install
# or
make install

# Verify installation
ls -la ~/llvm-install/bin/
```

### **Setup PATH**
```bash
# Add to ~/.bashrc or ~/.zshrc
export PATH="$HOME/llvm-install/bin:$PATH"

# Reload shell
source ~/.bashrc
# or
source ~/.zshrc

# Verify
llvm-config --version
clang --version
```

---

## Step 6: Setup VS Code for LLVM Development

### **Install VS Code Extensions**

1. **C/C++ Extension Pack**
   - ID: ms-vscode.cpptools-extension-pack
   - Includes: IntelliSense, debugging, CMake tools

2. **CMake Tools**
   - ID: ms-vscode.cmake-tools

3. **LLVM Extension** (Optional)
   - ID: llvm-vs-code-extensions.llvm

4. **Code Spell Checker**
   - ID: streetsidesoftware.code-spell-checker

### **Install Extensions via Command Line**
```bash
code --install-extension ms-vscode.cpptools-extension-pack
code --install-extension ms-vscode.cmake-tools
code --install-extension ms-vscode-remote.remote-ssh
```

---

## Step 7: Configure VS Code Workspace

### **Create .vscode/settings.json**
```json
{
  "cmake.configureOnOpen": true,
  "cmake.generator": "Ninja",
  "cmake.sourceDirectory": "${workspaceFolder}/llvm",
  "cmake.buildDirectory": "${workspaceFolder}/build",
  "cmake.buildType": "Release",
  "C_Cpp.default.configurationProvider": "ms-vscode.cmake-tools",
  "C_Cpp.formatting": "clangFormat",
  "C_Cpp.inlayHints.enabled": true,
  "editor.formatOnSave": true,
  "[cpp]": {
    "editor.defaultFormatter": "ms-vscode.cpptools",
    "editor.formatOnSave": true
  }
}
```

### **Create .vscode/launch.json (Debugging)**
```json
{
  "version": "0.2.0",
  "configurations": [
    {
      "name": "Debug LLVM",
      "type": "cppdbg",
      "request": "launch",
      "program": "${workspaceFolder}/build/bin/clang",
      "args": ["--version"],
      "stopAtEntry": false,
      "cwd": "${workspaceFolder}",
      "environment": [],
      "MIMode": "gdb",
      "setupCommands": [
        {
          "description": "Enable pretty-printing for gdb",
          "text": "-enable-pretty-printing",
          "ignoreFailures": true
        }
      ],
      "preLaunchTask": "Build LLVM"
    }
  ]
}
```

### **Create .vscode/tasks.json (Build Tasks)**
```json
{
  "version": "2.0.0",
  "tasks": [
    {
      "label": "Build LLVM",
      "type": "shell",
      "command": "ninja",
      "args": ["-C", "build"],
      "group": {
        "kind": "build",
        "isDefault": true
      },
      "problemMatcher": ["$gcc"],
      "presentation": {
        "echo": true,
        "reveal": "always",
        "focus": false
      }
    },
    {
      "label": "Rebuild LLVM",
      "type": "shell",
      "command": "ninja",
      "args": ["-C", "build", "-j8"],
      "group": "build",
      "problemMatcher": ["$gcc"]
    },
    {
      "label": "Clean Build",
      "type": "shell",
      "command": "ninja",
      "args": ["-C", "build", "-t", "clean"],
      "group": "build"
    },
    {
      "label": "Test LLVM",
      "type": "shell",
      "command": "ninja",
      "args": ["-C", "build", "check-llvm"],
      "group": "test"
    }
  ]
}
```

---

## Step 8: Open LLVM in VS Code

### **Method 1: From Command Line**
```bash
# Navigate to LLVM project
cd ~/llvm-project/llvm-project

# Open in VS Code
code .
```

### **Method 2: Open Folder in VS Code**
1. File → Open Folder
2. Select `~/llvm-project/llvm-project`
3. Click "OK"

### **VS Code Will:**
- ✅ Detect CMake project
- ✅ Auto-configure CMake
- ✅ Generate IntelliSense database
- ✅ Show compiler errors/warnings

---

## Common Build Commands in VS Code

### **Using Command Palette (Ctrl+Shift+P)**

| Command | Shortcut | Action |
|---------|----------|--------|
| CMake: Configure | F7 | Configure CMake |
| CMake: Build | Ctrl+Shift+B | Build project |
| CMake: Run Tests | None | Run test suite |
| CMake: Delete Cache | None | Clean configuration |

### **Terminal Integration**
```bash
# Open VS Code integrated terminal (Ctrl+`)
# Commands work directly:

cd build
ninja clang              # Build just Clang
ninja check-llvm         # Run LLVM tests
ninja install            # Install binaries
```

---

## Running Tests

### **Run All Tests**
```bash
cd ~/llvm-project/llvm-project/build
ninja check-all
```

### **Run Specific Tests**
```bash
# LLVM tests only
ninja check-llvm

# Clang tests only
ninja check-clang

# Specific test directory
cd build
ctest -R "test_pattern" -VV
```

### **In VS Code**
- Run via Task: Ctrl+Shift+B → Select "Test LLVM"
- Or use Command Palette: CMake: Run Tests

---

## Debugging in VS Code

### **Debug a Simple Program**

1. **Create test file:** `test.c`
```c
#include <stdio.h>

int main() {
    printf("Hello from LLVM!\n");
    return 0;
}
```

2. **Set breakpoint** in VS Code (click line number)

3. **Press F5** to start debugging

4. **Use Debug Console** to inspect variables

### **Debug LLVM Itself**
```bash
# Build with debug symbols
cmake -DCMAKE_BUILD_TYPE=Debug -G Ninja ../llvm
ninja

# Run with debugger
gdb build/bin/clang
```

---

## Performance Optimization Tips

### **Speed Up Compilation**
```bash
# Use more CPU cores
cmake -DLLVM_PARALLEL_COMPILE_JOBS=8 ../llvm

# Use LLD linker (faster)
cmake -DLLVM_USE_LINKER=lld ../llvm

# Disable unnecessary components
cmake -DLLVM_TARGETS_TO_BUILD="X86" ../llvm
```

### **Reduce Build Size**
```bash
# Build Release instead of Debug
cmake -DCMAKE_BUILD_TYPE=Release ../llvm

# Build shared libraries
cmake -DBUILD_SHARED_LIBS=ON ../llvm

# Strip debug symbols after build
strip build/bin/clang
```

### **RAM Usage**
```bash
# Limit parallel jobs if running out of memory
cmake -DLLVM_PARALLEL_LINK_JOBS=1 ../llvm

# Reduce link time optimization
cmake -DLLVM_ENABLE_LTO=OFF ../llvm
```

---

## Troubleshooting

### **Issue: CMake Configuration Fails**

**Error: "CMake not found"**
```bash
# Install CMake
sudo apt install cmake
# or
brew install cmake
```

**Error: "C++ compiler not found"**
```bash
# Install build tools
sudo apt install build-essential
# or
xcode-select --install  # macOS
```

### **Issue: Build Fails Midway**

**"Ninja: job failed"**
```bash
# Check disk space
df -h

# Clean and rebuild
rm -rf build
mkdir build
cd build
cmake -G Ninja ../llvm
ninja
```

**"Out of memory"**
```bash
# Reduce parallel jobs
ninja -j2  # Use 2 cores instead of all

# Or limit memory
ulimit -v 4000000  # Limit to 4GB
```

### **Issue: VS Code Slow/Crashing**

**Solution:**
1. Disable IntelliSense: Settings → C_Cpp.intelliSenseEngine: Disabled
2. Use Release build: `-DCMAKE_BUILD_TYPE=Release`
3. Exclude build folder from indexing: `.vscode/settings.json` add:
```json
"files.exclude": {
  "build/**": true
}
```

---

## Quick Reference Commands

```bash
# Full workflow
cd ~/llvm-project/llvm-project
mkdir build && cd build

# Configure
cmake -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=~/llvm-install \
  -DLLVM_ENABLE_PROJECTS="clang;lld" \
  ../llvm

# Build
ninja -j8

# Install
ninja install

# Test
ninja check-llvm

# Verify
~/llvm-install/bin/clang --version
```

---

## Next Steps After Successful Build

### **Use Compiled LLVM**
```bash
# Compile C code with your LLVM
~/llvm-install/bin/clang -O3 program.c -o program
./program
```

### **Develop with LLVM**
- Access LLVM libraries: `llvm-config --cxxflags --ldflags`
- Develop new passes, optimizations
- Contribute to LLVM project

### **Update to Latest**
```bash
cd ~/llvm-project/llvm-project
git pull
cd build
ninja  # Incremental rebuild
```

---

## Resources

| Resource | Link |
|----------|------|
| LLVM Documentation | https://llvm.org/docs/ |
| Getting Started | https://llvm.org/getting-started/ |
| Developer's Manual | https://llvm.org/docs/DeveloperPolicy/ |
| GitHub Repository | https://github.com/llvm/llvm-project |
| LLVM Discord | https://discord.com/invite/xS7Z362 |

---

## Summary Checklist

- [ ] Install dependencies (cmake, ninja, git, build-tools)
- [ ] Clone LLVM repository
- [ ] Create build directory
- [ ] Configure with CMake
- [ ] Build with Ninja
- [ ] Install binaries
- [ ] Setup VS Code extensions
- [ ] Configure workspace settings
- [ ] Open project in VS Code
- [ ] Build and test in VS Code
- [ ] Debug with GDB
- [ ] Verify installation
