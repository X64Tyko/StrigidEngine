# StrigidEngine

**A high-performance, data-oriented game engine for R&D and experimentation**

---

## Executive Summary

**Purpose:** A personal R&D sandbox designed to strip away modern engine abstractions and validate that a strict
data-oriented architecture can deliver sub-millisecond latency—**without giving up the comfort and mental model of OOP.**

**Objective:** A high-performance, data-oriented engine prioritizing mechanical elegance and input-to-photon latency,
while maintaining as close to existing OOP style and structure on the user end as possible.

**Primary Target:** 100,000+ dynamic entities at 512Hz fixed update (1.95ms per frame budget)

**Philosophy:** White-box architecture - users can understand, debug, and modify the engine without black-box
abstractions. "Build it to break it." I use this project to stress-test architectural theories (like the History Slab
and lock-free communication) that are too risky to implement directly into a live commercial product.

---

## Quick Stats (Week 3)

**Performance:**
- **Sentinel (Main):** 1.0ms per frame (1000 Hz) ✅
- **Brain (Logic):** ~4.8ms per frame @ 128Hz (target: 1.95ms @ 512Hz)
- **Encoder (Render):** ~3.1ms per frame (321 FPS)
- **1M entities:** ~3.0ms PrePhysics (stress test only)

**Architecture:**
- ✅ Three-thread architecture (Sentinel/Brain/Encoder)
- ✅ Lock-free communication (triple-buffer mailbox)
- ✅ SoA component decomposition (FieldProxy system)
- ✅ EntityView hydration (zero virtual calls)
- ✅ SIMD-friendly batch processing
- 🔄 History Slab (design complete, implementation pending)
- ⏳ Physics integration (Jolt, not started)
- ⏳ State-sorted rendering (not started)

---

## Core Features

### The Strigid Trinity (Three-Thread Architecture)

- **Sentinel (Main Thread):** 1000Hz input polling, GPU resource management, frame pacing
- **Brain (Logic Thread):** 512Hz fixed timestep simulation, deterministic physics
- **Encoder (Render Thread):** Variable-rate rendering, interpolation, GPU command encoding

### Memory Model: History Slab

A custom arena allocator that stores multiple frames of simulation history for:
- **Zero-copy rendering:** Direct page access eliminates snapshot overhead
- **Rollback netcode:** GGPO-style prediction and correction
- **Lag compensation:** Server-side rewind for hit detection
- **Replay system:** Read historical frames for playback

See [Architecture Documentation](docs/ARCHITECTURE.md) for details.

### Data-Oriented Components

Components decompose into Structure-of-Arrays via `FieldProxy<T>`:

```cpp
struct Transform {
    FieldProxy<float> PositionX, PositionY, PositionZ;
    FieldProxy<float> RotationX, RotationY, RotationZ;
    FieldProxy<float> ScaleX, ScaleY, ScaleZ;
};

class CubeEntity : public EntityView<CubeEntity> {
    Transform transform;
    Velocity velocity;

    void PrePhysics(double dt) {
        // OOP-style syntax, SoA performance
        transform.PositionX += velocity.VelocityX * dt;
    }
};
```

Users write natural OOP code while the engine handles SoA layout automatically.

---

## Architectural Constraints

These rules drive all design decisions:

### Hard Constraints
1. **Macro-First Registration** - Components and entities must register with reflection system
2. **PoD Components Only** - No `std::vector` or `std::string` in components
3. **FieldProxy Hot Components** - Hot-path data lives in History Slab
4. **Zero Frame Allocations** - No memory allocation in PrePhysics/Update/PostPhysics/Render
5. **Lock-Free Communication** - Atomics and wait-free data structures only
6. **No Logic in Physics** - Physics solver never calls virtual functions
7. **No Rendering in Logic** - GPU calls only on Encoder thread

### Design Goals
8. **White Box Philosophy** - Understand and debug everything
9. **OOP Facade** - Natural syntax despite SoA layout
10. **Cache Locality First** - Sequential access patterns
11. **SIMD-Friendly** - Vectorizable loops with compiler hints
12. **Deterministic Option** - Configurable determinism for netcode

---

## Documentation

### Core Documentation
- **[Architecture Overview](docs/ARCHITECTURE.md)** - History Slab, threading model, memory layout
- **[Performance Targets](docs/PERFORMANCE_TARGETS.md)** - Benchmarks, budgets, scalability analysis
- **[Data Structures](docs/DATA_STRUCTURES.md)** - Component patterns, FieldProxy, EntityView
- **[Configuration Guide](docs/CONFIGURATION.md)** - EngineConfig presets and tuning
- **[Current Status](docs/STATUS.md)** - Week 3 progress, roadmap, next milestones
- **[Build Options](docs/BUILD_OPTIONS.md)** - CMake configuration, Tracy profiling, vectorization
- **[Schema Error Examples](docs/SCHEMA_ERROR_EXAMPLES.md)** - Common reflection system mistakes and fixes

### Key Concepts
- **History Slab:** Multi-frame arena allocator for zero-copy rendering and netcode
- **FieldProxy:** Zero-overhead indirection for SoA component access
- **EntityView:** CRTP-style entity base with compile-time hydration
- **Triple-Buffer Mailbox:** Lock-free communication between Logic and Render threads

---

## Building

**Requirements:**
- C++20 compiler (MSVC, Clang, or GCC)
- CMake 3.20+
- SDL3 (included as submodule)
- Tracy Profiler (included as submodule)

**Quick Start:**

```bash
git clone --recursive https://github.com/YourUsername/StrigidEngine.git
cd StrigidEngine
cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
./build/Testbed/Testbed
```

See [docs/BUILD_OPTIONS.md](docs/BUILD_OPTIONS.md) for advanced configuration.

---

## Project Status

**Current Phase:** Week 3 - SoA Architecture Complete, History Slab Design

**Completed:**
- Three-thread architecture with lock-free communication
- Component decomposition via FieldProxy
- EntityView hydration with zero virtual calls
- Tracy profiler integration
- SDL3 GPU rendering (Vulkan/Metal/D3D12)

**In Progress:**
- History Slab implementation
- State-sorted rendering (64-bit sort keys)

**Next Up:**
- Frustum culling integration
- Jolt Physics integration
- Input system (1000Hz sampling)

See [docs/STATUS.md](docs/STATUS.md) for detailed weekly progress.

---

## Performance Philosophy

**Target:** 100,000 dynamic entities at 512Hz fixed update (1.95ms per frame)

We measure performance in **milliseconds per frame**, not FPS:
- **Sentinel:** 1.0ms (1000 Hz) ✅
- **Logic:** 1.95ms (512 Hz) - target
- **Render:** 8-16ms (60-120 FPS) - target

Memory footprint with 128 pages of history:
- **100k mixed entities:** ~1.89 GB (includes hot history + cold components)
- **100k simple entities:** ~1.61 GB
- **Configurable:** 4-128 pages for different use cases (rendering only → full GGPO)

See [docs/PERFORMANCE_TARGETS.md](docs/PERFORMANCE_TARGETS.md) for detailed analysis.

---

## Design Inspiration

This project explores ideas from:
- **Data-Oriented Design** - Naughty Dog
- **Overwatch Netcode** - GDC talks on lag compensation
- **GGPO** - Tony Cannon's rollback netcode
- **Jolt Physics** - Zero-copy simulation integration
- **Tracy Profiler** - Frame-accurate performance analysis

The goal is to validate that strict DOD can coexist with comfortable OOP-style user code.

---

## License

---

## Contact

- **Author:** Cody "Tyko" Pederson
- **Issues:** [GitHub Issues](https://github.com/YourUsername/StrigidEngine/issues)
- **Discussions:** [GitHub Discussions](https://github.com/YourUsername/StrigidEngine/discussions)

---

**Note:** This is a personal R&D project for experimenting with engine architecture. Production use is not recommended. The primary goal is learning and stress-testing architectural theories.
