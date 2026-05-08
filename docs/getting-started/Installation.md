# Installation

> [Home](../Home.md) | [Build Options →](Build-Options.md)

---

## System Requirements

| Requirement | Minimum |
|---|---|
| CMake | 3.20+ |
| Compiler | GCC 10+ / Clang 12+ (Linux) or MSVC 2022+ (Windows) |
| C++ Standard | C++20 |
| Disk space | 5GB free (build artifacts) |

---

## Clone

```bash
# With submodules (recommended for first clone)
git clone --recursive https://github.com/X64Tyko/TrinyxEngine.git

# Dev stream (Dev-Main branch — may be unstable)
git clone --recursive --branch Dev-Main --single-branch https://github.com/X64Tyko/TrinyxEngine.git

# If already cloned without --recursive
cd TrinyxEngine
git submodule update --init --recursive
```

**First-time build takes 20–30 minutes** due to OpenSSL and Protobuf. Subsequent builds: 1–2 minutes.

---

## Submodule Dependencies

All dependencies are managed as git submodules. Running without initialized submodules will fail at CMake configure time.

| Library | Version | Approx. Size | Approx. Build Time | Purpose |
|---|---|---|---|---|
| Jolt Physics | v5.5.0 | ~50MB | ~2 min | Physics simulation |
| Tracy | v0.13.1-391 | ~20MB | ~1 min | Performance profiler |
| Dear ImGui | v1.92.6-docking | ~15MB | <1 min | Editor UI (docking branch required) |
| ImGuizmo | master | ~5MB | <1 min | 3D gizmo manipulation |
| GameNetworkingSockets | v1.4.0-244 | ~140MB | ~3 min | Networking transport |
| OpenSSL | 3.3.3 | ~480MB | ~10 min | Crypto for GNS |
| Protocol Buffers | v3.29.2 | ~1.2GB | ~15 min | Serialization |

**Total download:** ~1.8GB

**Vendored (no action needed):**
- `libs/SDL3` — Window / input / audio
- `libs/volk` — Vulkan meta-loader
- `libs/vma` — Vulkan Memory Allocator
- `libs/slang` — Slang shader compiler

---

## Build

### Quick Start (RelWithDebInfo — recommended for development)

```bash
cmake -B cmake-build-relwithdebinfo -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build cmake-build-relwithdebinfo
./cmake-build-relwithdebinfo/Testbed/Testbed
```

### Debug

```bash
cmake -B cmake-build-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build cmake-build-debug
```

### Editor Build

```bash
cmake -B build-editor -DCMAKE_BUILD_TYPE=RelWithDebInfo -DTNX_ENABLE_EDITOR=ON
cmake --build build-editor
```

### Rollback / Networked Build

```bash
cmake -B build-netcode -DCMAKE_BUILD_TYPE=RelWithDebInfo -DTNX_ENABLE_ROLLBACK=ON
cmake --build build-netcode
```

### Windows — Visual Studio Solution

```bash
cmake -S . -B build-vs -G "Visual Studio 17 2022" -A x64
cmake --build build-vs --config RelWithDebInfo
# Open build-vs/TrinyxSolution.sln in Visual Studio or Rider
```

---

## Speed Up First Build

OpenSSL and Protobuf are the slow parts. Use parallel jobs:

```bash
# Linux / macOS
cmake --build cmake-build-relwithdebinfo -j$(nproc)

# Windows
cmake --build build-vs --config RelWithDebInfo --parallel
```

---

## Troubleshooting

### Submodules not initialized

```
CMake Error: Could not find git for clone of tracy
```

```bash
git submodule update --init --recursive
```

### ImGui docking features missing

```
Cannot resolve symbol 'ImGuiConfigFlags_DockingEnable'
```

ImGui must be on the `docking` branch:

```bash
cd libs/imgui
git branch   # should show: * docking
# If not:
git checkout docking
```

### Wrong compiler version

```
This project requires a C++20 compiler
```

Update to GCC 10+ / Clang 12+ / MSVC 2022+.

### Out of disk space

OpenSSL and Protobuf build artifacts are large (~2–3GB). Ensure 5GB free.

### Stale CMake cache

```bash
# Delete cache and reconfigure
rm -rf build-vs
cmake -S . -B build-vs -G "Visual Studio 17 2022" -A x64
```
