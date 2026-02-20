# Current Status (Week 3)

> **Navigation:** [← Back to README](../README.md) | [← Configuration](CONFIGURATION.md)

---

## Timeline Context

**Project Start:** February 2, 2025 (Week 1)
**Current Date:** February 20, 2025 (End of Week 3)
**This Document:** Planning and architecture documentation for weeks ahead

---

## Completed Work (Weeks 1-3)

### ✅ Week 1-2: Foundation & Trinity Architecture

**Threading Model:**
- Strigid Trinity (Sentinel/Brain/Encoder) fully operational
- Lock-free FramePacket mailbox (CAS-based triple buffer)
- Frame synchronization handshake (Encoder ↔ Sentinel)
- GPU resource atomics (command buffer, swapchain handoff)
- FramePacer with 3 frames in flight

**Rendering Pipeline:**
- SDL3 GPU backend (Vulkan/Metal/D3D12)
- Instanced rendering with dynamic instance buffer
- Transfer buffer resize-on-demand
- Snapshot + interpolation system (double-buffered)
- Zero stutters, zero Vulkan validation errors

**Performance Baseline:**
- Main Thread: ~2.0ms per frame (~500 Hz)
- Logic Thread: ~4.8ms per frame (~207 FPS @ 128Hz fixed)
- Render Thread: ~3.1ms per frame (~321 FPS)
- 100k entities: Stable rendering, smooth interpolation

### ✅ Week 3: Component Decomposition & SoA Architecture

**FieldProxy System:**
- Full component decomposition into SoA field arrays
- `FieldProxy<T>` zero-overhead indirection
- Components maintain struct-like syntax
- Perfect cache locality for individual fields

**EntityView Hydration:**
- `EntityView<T>::Hydrate()` binds all FieldProxies to chunk arrays
- Direct component access (no `Ref<T>` wrappers needed)
- Schema reflection drives automatic binding
- OOP-style user code with SoA performance

**Batch Processing:**
- `Archetype::BuildFieldArrayTable()` - stack-allocated pointer tables
- Per-archetype, per-chunk iteration
- `InvokePrePhysicsImpl<T>()` with `#pragma loop(ivdep)` hints
- Compiler can auto-vectorize field operations

**Lifecycle Integration:**
- PrePhysics, Update, PostPhysics all functional
- Template-based dispatch (zero virtual calls)
- Each entity class compiles to optimized kernel

**Reflection System:**
- `STRIGID_REGISTER_ENTITY(T)` - entity type registration
- `STRIGID_REGISTER_COMPONENT(T)` - component type registration
- `STRIGID_REGISTER_FIELDS(...)` - field decomposition
- `STRIGID_REGISTER_SCHEMA(...)` - entity schema definition
- Minimal boilerplate, compile-time generation

**Stress Testing:**
- 1M entities: ~3.0ms PrePhysics (transform updates only)
- 1M entities: ~19.6ms render frame (51 FPS, no culling)
- 100k entities: ~0.3ms PrePhysics, ~3.1ms render
- SoA benefits confirmed: cache-friendly, SIMD-ready

---

## Current Performance Metrics (Week 3)

All measurements on RelWithDebInfo build, Tracy profiler

### Logic Thread (128Hz Fixed Update)

| Test                          | Entity Count | Time      | Notes                         |
|-------------------------------|--------------|-----------|-------------------------------|
| PrePhysics (Transform only)   | 10k          | 0.03ms    | Well under budget             |
| PrePhysics (Transform only)   | 100k         | 0.30ms    | ✅ On track for 512Hz target  |
| PrePhysics (Transform only)   | 1M           | 3.0ms     | Stress test (not target)      |
| Full Frame (PrePhys+overhead) | 100k         | 4.8ms     | Includes ECS dispatch, Tracy  |

**Analysis:**
- PrePhysics scales linearly with entity count
- 100k @ 0.3ms leaves 1.65ms budget for physics solver
- 512Hz target (1.95ms) is achievable with optimizations

### Render Thread (Variable Rate)

| Test                          | Entity Count | Time      | FPS Equiv | Notes                     |
|-------------------------------|--------------|-----------|-----------|---------------------------|
| Full Frame (no culling)       | 100k         | 3.1ms     | 321 FPS   | ✅ Comfortable            |
| Snapshot Copy (bottleneck)    | 100k         | 5-8ms     | N/A       | Will be eliminated        |
| Full Frame (no culling)       | 1M           | 19.6ms    | 51 FPS    | Stress test               |

**Bottlenecks:**
- Snapshot copy: 5-8ms for 100k (60-70% of render time)
- No frustum culling: rendering all entities
- No state sorting: suboptimal GPU state changes

**Post-History-Slab Projection:**
- Eliminate 5-8ms snapshot copy
- Add culling: 50-70% reduction in visible entities
- Expected: 100k entities @ 2-4ms render (250-500 FPS)

### Main Thread (1000Hz Target)

| Task                | Time    | Notes                              |
|---------------------|---------|------------------------------------|
| Full Frame          | 1.0ms   | ✅ 1000 Hz - hitting target exactly |
| Event Polling       | 0.1ms   | SDL3, input sampling               |
| GPU Resource Handoff| 0.3ms   | Acquire cmd/swapchain              |
| GPU Submit          | 0.2ms   | Submit, signal fence               |
| Frame Pacing        | 0.3ms   | Proper timing to maintain 1000Hz   |
| Overhead            | 0.1ms   | Thread coordination, atomics       |

**Analysis:** Sentinel thread now has proper frame pacing and consistently hits 1000Hz target

---

## Architecture Status

### ✅ Completed

- [x] Three-thread architecture (Sentinel/Brain/Encoder)
- [x] Lock-free FramePacket mailbox
- [x] Frame synchronization handshake
- [x] GPU resource atomics
- [x] FramePacer (3 frames in flight)
- [x] SDL3 GPU rendering (Vulkan/Metal/D3D12)
- [x] Instanced rendering
- [x] Snapshot + interpolation
- [x] Component decomposition (FieldProxy)
- [x] EntityView hydration pattern
- [x] Per-chunk field array iteration
- [x] Batch processing with SIMD hints
- [x] Three lifecycle phases (PrePhysics/Update/PostPhysics)
- [x] Reflection system (minimal boilerplate)
- [x] Tracy profiler integration (3-level zones)

### 🔄 In Progress

- [ ] **History Slab** - Design phase, not implemented
  - Custom arena allocator
  - Section-based history buffering
  - Ownership tracking (atomic bitfield)
  - HistorySectionHeader replaces FramePacket
  - Migration plan defined

### ⏳ Planned (Next 4-6 Weeks)

- [ ] **State-Sorted Rendering** - 64-bit sort keys, radix sort
- [ ] **Frustum Culling** - Camera visibility, SIMD tests
- [ ] **Physics Integration** - Jolt Physics, zero-copy mapping
- [ ] **Input System** - 1000Hz sampling, action maps
- [ ] **Networking Foundation** - Packet serialization, delta compression
- [ ] **Job System** - Parallel rendering, async tasks

### 🔬 Research Phase

- [ ] **Rollback Netcode** - GGPO-style, prediction binding
- [ ] **Defragmentation** - Slab-level and entity-level
- [ ] **Replay System** - Read historical frames from slab
- [ ] **Lag Compensation** - Server-side rewind

---

## Known Issues & Technical Debt

### High Priority

1. **Snapshot Copy Bottleneck**
   - Issue: RenderThread copies 5-8ms of archetype data every frame
   - Impact: 60-70% of render thread time wasted on memcpy
   - Solution: History Slab eliminates this (direct pointer access)
   - ETA: Week 4-5

2. **No Frustum Culling**
   - Issue: Rendering all entities regardless of camera view
   - Impact: Wasted GPU time, lower FPS in large scenes
   - Solution: Implement camera frustum tests during interpolation
   - ETA: Week 5-6

3. **No State Sorting**
   - Issue: Naive draw order, suboptimal GPU state changes
   - Impact: More draw calls, higher GPU overhead
   - Solution: 64-bit sort keys, radix sort
   - ETA: Week 6-7

### Medium Priority

4. **FramePacket Mailbox**
   - Issue: Will be replaced by History Slab headers
   - Impact: Extra 64 bytes per frame, triple-buffer overhead
   - Solution: Remove when History Slab is implemented
   - ETA: Week 5

5. **Fixed 128Hz**
   - Issue: Currently hardcoded to 128Hz, target is 512Hz
   - Impact: Not testing full target performance yet
   - Solution: Increase Hz once physics and History Slab are ready
   - ETA: Week 7-8

6. **No Physics**
   - Issue: Only transform updates, no collision/response
   - Impact: Can't test full simulation load
   - Solution: Integrate Jolt Physics
   - ETA: Week 8-10

### Low Priority

7. **Boilerplate Macros**
   - Issue: Requires 4 macros per entity/component
   - Impact: Slight user friction, easy to forget
   - Solution: Investigate reducing to 2-3 macros
   - ETA: Week 12+

8. **Single Mesh/Material**
   - Issue: All entities render with same cube mesh
   - Impact: Can't test state sorting benefits
   - Solution: Asset pipeline, multiple meshes
   - ETA: Week 10-12

---

## Next Milestones (Week 4-8)

### Week 4-5: History Slab Implementation

**Goals:**
- Implement `HistorySlab` allocator with section management
- Implement `HistorySectionHeader` with atomic ownership
- Migrate Transform to History Slab (first hot component)
- Update `Archetype::BuildFieldArrayTable` to point into slab
- Benchmark memory usage vs current system

**Success Criteria:**
- Logic writes to Section[(Frame+1) % N]
- Render reads from Section[Frame % N] and Section[(Frame-1) % N]
- Zero snapshot copies (verify with profiler)
- Memory usage matches calculations (685MB for 100k @ 128 pages)

**Risks:**
- Complex pointer arithmetic bugs
- Ownership race conditions
- Performance regression if implemented incorrectly

### Week 6-7: Frustum Culling + State Sorting

**Goals:**
- Implement camera frustum tests (6-plane SIMD)
- Integrate culling into interpolation loop
- Implement 64-bit sort key generation
- Radix sort InterpBuffer

**Success Criteria:**
- 50-70% reduction in InterpBuffer size (typical scenes)
- Culling < 0.5ms for 100k entities
- Sorting < 1.0ms for 100k entities
- Measurable reduction in GPU state changes

**Risks:**
- Culling false positives (visible objects culled)
- Sorting overhead negates benefits for small scenes

### Week 8-10: Physics Integration

**Goals:**
- Integrate Jolt Physics library
- Map Transform hot components to Jolt bodies
- Zero-copy physics (Jolt writes to History Slab)
- Implement collision callbacks in PostPhysics

**Success Criteria:**
- 100k dynamic rigid bodies simulated
- Physics solver < 0.8ms per frame
- PrePhysics + Physics + PostPhysics < 1.95ms (512Hz)
- No memory allocations during simulation

**Risks:**
- Jolt may not support zero-copy (may need adapter layer)
- 512Hz physics may be unrealistic (may need to scale back to 256Hz)
- Collision callbacks may break frame budget

---

## Lessons Learned (Weeks 1-3)

### What Went Well

1. **FieldProxy Design**
   - Zero-overhead indirection works beautifully
   - Compiler optimizes away proxy layer entirely
   - User code looks like OOP, performs like SoA
   - Easy to understand and debug

2. **Lock-Free Communication**
   - FramePacket mailbox has zero contention
   - Atomics perform well (no measurable overhead)
   - Thread handoffs are clean and predictable

3. **Tracy Profiler**
   - Invaluable for identifying bottlenecks
   - Zone colors help distinguish thread work
   - Confirmed snapshot copy is main problem

4. **Reflection System**
   - Macros are verbose but functional
   - Compile-time generation avoids runtime overhead
   - Schema system is flexible (inheritance works)

### What Didn't Go Well

1. **Snapshot Copy**
   - Underestimated memcpy cost (5-8ms is huge)
   - Should have started with History Slab earlier
   - Now have to refactor working system

2. **1M Entity Testing**
   - Misleading metric (not representative of real games)
   - Spent time optimizing for wrong target
   - Should focus on 100k with physics instead

3. **Documentation Lag**
   - README fell behind implementation
   - Design decisions not documented in real-time
   - This update is catching up 2 weeks of work

### Going Forward

- **Document design before implementing** (this conversation is proof of value)
- **Focus on 100k @ 512Hz** (stop chasing 1M)
- **Profile early, profile often** (Tracy saves time)
- **Iterate on working code** (History Slab is better than triple-buffer because we learned from FramePacket)

---

## Summary

**Week 3 Status: Solid Foundation, Ready for Next Phase**

The engine has a working three-thread architecture with full SoA component decomposition. Performance is good (100k @ 0.3ms PrePhysics) but render thread is bottlenecked by snapshot copies. The History Slab design will eliminate this bottleneck and enable networking features.

**Key Numbers:**
- ✅ 100k entities: 0.3ms PrePhysics (1.65ms budget remaining for physics)
- ✅ Main thread: 2.0ms (500 Hz, target 1000 Hz)
- 🔄 Render thread: 3.1ms render + 5-8ms snapshot (bottleneck)
- 🎯 Target: 100k entities @ 512Hz fixed update (1.95ms per frame)

**Next Priority:** Implement History Slab to eliminate snapshot bottleneck and enable direct render access.

---
