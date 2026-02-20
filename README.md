# StrigidEngine Master Architecture & Roadmap v3.0

## Executive Summary

**Purpose:** A personal R&D sandbox designed to strip away modern engine abstractions and validate that a strict
data-oriented architecture can deliver sub-millisecond latency.

**Objective:** A high-performance, data-oriented engine prioritizing mechanical elegance and input-to-photon latency,
while maintaining as close to existing OOP style and stucture on the user end as possible.

**Constraint:** Zero "allocation-per-frame" in hot paths. Scale to 100,000+ active entities running Logic at 512Hz fixed
and input at 1Khz

**Philosophy:** White-box architecture - users can understand, debug, and modify the engine without black-box
abstractions. "Build it to break it." I use this project to stress-test architectural theories (like lock-free
triple-buffering) that are too risky to implement directly into a live commercial product.

---

## TL;DR Current Metrics

- 1M entities ~3ms PrePhysics call. Just transform updates.
- Rendering 1M at 51FPS, no culling, depth, etc..
- Trinity split:
    - Main: Input polling and window management 1KHz
    - Logic: Running the fixed update and variadic update loop
    - Render: Requests resources from Main, builds PRU commands
- Proper SoA with component decomposition, ready for sparse sets for 128(variable) history buffers and true SIMD phys
  rendering.
- OOP facade remains intact, though at the moment creating new components requires significant boilerplate.

---

## 1. Core Architecture: The Strigid Trinity

### I. Threading Model

**Three specialized threads, each with a distinct purpose:**

#### Sentinel (Main Thread)

- **OS Event Pumping** - `SDL_PollEvent()` (required on main thread)
- **Window Ownership** - SDL3 requirement
- **GPU Resource Management** - Acquire CmdBuffer/SwapchainTexture, Submit via FramePacer
- **Frame Pacing** - Wait for GPU fences to limit frames in flight (3)
- **Thread Lifecycle** - Start/Stop Logic and Render threads
- **Target Rate:** 500-1000 Hz (uncapped, minimal work)

#### Brain (Logic Thread)

- **Fixed Timestep Simulation** - FixedUpdate at `EngineConfig.FixedUpdateHz` (60/120Hz)
- **Variable Update** - Update(dt) every frame for time-sensitive logic
- **Accumulator/Substepping** - Catch up on missed fixed timesteps (spiral of death protection)
- **FramePacket Production** - Fill staging packet with camera, entity count, frame number
- **Mailbox Owner** - Allocates 3 FramePackets for triple-buffer communication
- **Target Rate:** 60-120 Hz (configurable, deterministic)

#### Encoder (Render Thread)

- **FramePacket Consumption** - Poll mailbox for new simulation state (lock-free)
- **Direct Page Access** - Read from committed triple-buffered sparse array pages (zero-copy)
- **Integrated Processing** - Culling + interpolation + dead entity filtering in single pass
- **InterpBuffer Construction** - Build render-ready data (only visible entities)
- **State Sorting** - Sort by 64-bit key (Pass → Pipeline → Material → Mesh)
- **Transfer Buffer Writes** - Upload final InterpBuffer to GPU
- **Command Buffer Building** - Encode GPU commands (copy pass, render pass, draw calls)
- **Signaling** - Request GPU resources from Sentinel, signal when ready to submit
- **Target Rate:** Uncapped (300+ FPS, limited by culling/interpolation and GPU encoding)

### II. Communication & Synchronization

#### Triple Buffer Mailbox (Lock-Free)

```
Brain (Writer)           Mailbox (Atomic)        Encoder (Reader)
    │                         │                        │
    ├─► StagingPacket         │                        │
    │   (Fill data)           │                        │
    │                         │                        │
    ├─► CAS(Staging↔Mailbox)  │                        │
    │                    ┌────┴────┐                   │
    │                    │ Packet  │                   │
    │                    └────┬────┘                   │
    │                         │   ◄──── CAS(Mailbox↔Visual)
    │                         │                        │
    │                         │                   VisualPacket
    │                         │                   (Render uses)
```

**Benefits:**

- Brain never blocks on Encoder
- Encoder always gets latest complete frame
- Zero locks, zero contention

#### Frame Synchronization Handshake

```
Encoder                                  Sentinel
   │                                        │
   ├─► bFrameSubmitted.wait(true) ────────►│
   │   (Block until previous frame done)   │
   ├─► bFrameSubmitted = false             │
   │                                        │
   ├─► [Do frame work...]                  │
   │                                        │
   ├─► SignalReadyToSubmit()               │
   │   bReadyToSubmit = true ──────────────►│
   │                                        │
   │                              FramePacer.EndFrame(cmdBuf)
   │                              Submit to GPU
   │                                        │
   │   ◄──── bFrameSubmitted = true ────────┤
   │                                        │
```

**Purpose:** Prevent Encoder from starting frame N+1 before frame N is submitted (avoids semaphore errors).

#### GPU Resource Atomics

- `bNeedsGPUResources` - Encoder → Sentinel request
- `bReadyToSubmit` - Encoder → Sentinel completion signal
- `CmdBufferAtomic` - Sentinel → Encoder → Sentinel handoff
- `SwapchainTextureAtomic` - Sentinel → Encoder (read-only)
- `bFrameSubmitted` - Sentinel → Encoder acknowledgment

### III. Memory Model: Hybrid-Sparse Relational ECS

#### The Core (Hot Path) - **In Progress**

**Triple-Buffered Bespoke Sparse Sets:**

- Components: `Transform`, `Velocity`, `BoneArray`, `HBV`
- Paged for 100% cache density
- **Page₀ (Committed - Oldest)** ← Render reads as Previous
- **Page₁ (Staging - Newest)** ← Render reads as Current
- **Page₂ (Writing - Active)** ← Logic writes

**Page Rotation (Logic Thread):**

```cpp
void LogicThread::FixedUpdate(dt) {
    // 1. Write to Page₂
    TransformSparseSet.GetWritePage()[i].Position += velocity * dt;

    // 2. Swap at end of physics
    TransformSparseSet.SwapPages();
    // Page₂ → Page₁ (newest)
    // Page₁ → Page₀ (oldest)
    // Page₀ → Page₂ (write target)
}
```

**Direct Access (Render Thread):**

```cpp
void RenderThread::ProcessEntities() {
    // Zero-copy access to committed pages
    Transform* prevPage = TransformSparseSet.GetPage(0);  // Page₀
    Transform* currPage = TransformSparseSet.GetPage(1);  // Page₁

    // Direct iteration, no copying
    for (uint32_t i = 0; i < entityCount; ++i) {
        if (currPage[i].IsInactive()) continue;  // Dead entity

        Vector3 pos = Lerp(prevPage[i].Position, currPage[i].Position, alpha);
        if (!camera.IsVisible(pos)) continue;  // Culling

        InterpBuffer.push_back({...});  // Only visible entities
    }
}
```

#### The Shell (Cold Path) - **Current Implementation**

- **Single-Buffered Archetype Chunks**
    - `Health`, `Inventory`, `AIState`, `ColorData`
    - PoD-only logic data
    - Chunk-based allocation (16KB chunks)

#### The Bridge

- **Centralized Registry** mapping `EntityID → {CorePagePtr, ShellChunkPtr}`
- **Current:** All components in Shell (Archetype system)
- **In Progress:** Migrate Transform to triple-buffered Core
- **Future:** All hot components in Core, cold components in Shell

---

## 2. Completed Milestones

### ✅ Week 4-5: ECS-Driven Rendering

- SDL3 GPU rendering pipeline (Vulkan/Metal/D3D12 backend)
- Instanced rendering (100,000 cubes @ 130 FPS single-threaded)
- ECS with Transform, ColorData, Velocity components
- Reflection-based lifecycle system (`STRIGID_REGISTER_CLASS`)
- Performance profiling with Tracy (3-level profiling zones)
- Persistent instance buffer optimization

### ✅ Week 6: The Strigid Trinity Implementation

**Three-thread architecture fully operational:**

- Sentinel (Main): 502 FPS - Event pump, GPU resource management
- Brain (Logic): 207 FPS - Fixed timestep simulation, FramePacket production
    - (The FPS was set to cap at 240, this runs well above that when not capped, I think there's an issue with either
      the FPS counting or the WaitForTime function)
- Encoder (Render): 321 FPS - Snapshot, interpolation, GPU command encoding
    - This is currently just copying the instance data straight from the archetype chunks since we don't have the triple
      buffer sparse sets created yet.

**Lock-free communication:**

- Triple-buffer mailbox (Brain → Encoder)
- Frame synchronization handshake (Encoder → Sentinel)

**Snapshot + Interpolation rendering:**

- RenderThread snapshots ECS state when FrameNumber changes
- Double-buffered snapshots (Previous/Current)
- Interpolates between snapshots at variable rate using alpha
- Buttery smooth rendering even with 60Hz fixed simulation

**GPU resource management:**

- Transfer buffer with resize-on-demand
- Instance buffer with resize-on-demand
- Proper buffer cleanup (no memory leaks)
- FramePacer with 3 frames in flight
    - Kinda wanna try 2 next, need some latency testing in so I can see input to render time.

**Performance achieved (100k entities):**

- Render: 321 FPS (3.11ms per frame)
- Logic: 207 FPS (4.81ms per frame)
- Main: 502 FPS (1.99ms per frame)
- Zero stutters, zero Vulkan crashes
- Stress tested successfully ( calling 10 min running good for this stage.)

---

## 3. Immediate Roadmap (Weeks 7-9)

### Phase 1: State-Sorted Rendering (Current Priority)

**Goal:** Replace naive rendering with CPU-driven state-sorted command stream.

**Why CPU-Driven?**

- GPU-driven (indirect draw) locks logic into compute shaders
    - May tinker with this, if it goes well could simply include it as a default option
- Harder to debug and extend for users
- CPU sorting is fast enough for 100k entities (<1ms)
- Maintains "white box" philosophy

**The 64-Bit Sort Key System:**

```cpp
| Bits    | Name     | Purpose                                          |
|---------|----------|--------------------------------------------------|
| 63-60   | Layer    | 0=Background, 1=Opaque, 2=Transparent, 3=UI     |
| 59-44   | Depth    | Distance from camera (16-bit quantized)         |
| 43-32   | Pipeline | Shader/Blend state ID (4096 unique pipelines)   |
| 31-16   | Material | Texture set ID (65536 unique materials)         |
| 15-0    | Mesh     | Geometry ID (65536 unique meshes)               |
```

**Sort Order:**

1. **Layer** - Opaque before Transparent (correctness)
2. **Depth** - Front-to-back for Opaque (early-Z), Back-to-front for Transparent
3. **Pipeline** - Minimize expensive shader switches
4. **Material** - Minimize texture binding
5. **Mesh** - Batch geometry

**Tasks:**

- [ ] **RenderSorter Class** - Generate, store, and sort 64-bit keys
- [ ] **Key Generation** - Compute keys during snapshot/interpolation phase
- [ ] **Radix Sort** - O(N) integer sort for >10k entities
- [ ] **State Change Detection** - Track last Pipeline/Material/Mesh to avoid redundant binds
- [ ] **Z-Prepass** - Optional opaque-only depth pass for complex scenes

**Success Criteria:**

- Sorting <1ms for 100k entities
- Measured reduction in GPU state changes (Tracy GPU profiling)
- Foundation for multiple materials/meshes

### Phase 2: CRTP Entity Views for 1M Scaling

**Goal:** Replace template-based iteration with CRTP for zero-overhead polymorphism.

**Current Bottleneck:**

- `RegistryPtr->InvokeAll(LifecycleType::FixedUpdate, dt)` uses function pointers
- Type lookups per entity
- Logic thread chokes at 1M entities

**CRTP Solution:**

```cpp
template<typename Derived>
class EntityView {
    void Update(double dt) {
        static_cast<Derived*>(this)->UpdateImpl(dt);
    }
};

class CubeEntity : public EntityView<CubeEntity> {
    void UpdateImpl(double dt) {
        // Direct call, fully inlined, no vtable
        transform->RotationX += dt;
    }
};

// Usage:
registry.ForEach<Transform, ColorData>([](Transform& t, ColorData& c) {
    // Cache-friendly iteration, SIMD-friendly
    t.RotationX += dt;
});
```

**Tasks:**

- [ ] **EntityView Base** - CRTP base class template
- [ ] **ForEach Iterator** - Direct component iteration, no indirection
- [ ] **Batch Operations** - Vectorize common operations (rotation, movement)
- [ ] **Remove InvokeAll** - Replace with CRTP views

**Success Criteria:**

- 1M entities at stable 60 FPS on Logic thread
- Zero virtual calls in hot path
- Auto-vectorization (verify with assembly inspection)

### Phase 3: Triple-Buffered Sparse Arrays (UPDATED - Current Architecture)

**Goal:** Eliminate expensive snapshot copies and integrate culling/interpolation into single pass.

**The Triple Buffer Flow:**

1. **Brain (Logic Thread):**
    - Writes to Page₂ during FixedUpdate
    - Calls `SwapPages()` at end of physics
    - Rotates: Page₂→Page₁, Page₁→Page₀, Page₀→Page₂
    - Increments FrameNumber in FramePacket

2. **Encoder (Render Thread):**
    - Detects new FrameNumber from mailbox
    - Gets direct pointers: `GetPage(0)` and `GetPage(1)`
    - Iterates both pages simultaneously (zero copy)
    - Performs in single pass:
        * Dead entity detection (skip inactive)
        * Frustum culling (skip invisible)
        * Interpolation (Lerp with alpha)
        * Sort key generation
    - Outputs to InterpBuffer (only visible entities)

**Implementation:**

```cpp
template<typename T>
class TripleBufferedSparseSet {
    T* Pages[3];
    size_t Capacity;

    void SwapPages() {
        // Atomic rotation
        T* temp = Pages[0];
        Pages[0] = Pages[1];  // Staging → Oldest
        Pages[1] = Pages[2];  // Writing → Staging
        Pages[2] = temp;      // Oldest → Writing
    }

    T* GetPage(int index) const { return Pages[index]; }
    T* GetWritePage() { return Pages[2]; }
};
```

**InterpEntry (Render-Ready Data):**

```cpp
struct InterpEntry {
    uint64_t sortKey;    // For state sorting
    Vector3 position;    // Interpolated
    Vector3 rotation;    // Interpolated
    Vector3 scale;       // Interpolated
    Color color;         // From ColorData
};

std::vector<InterpEntry> InterpBuffer;  // Only visible entities
```

**Tasks:**

- [ ] **TripleBufferedSparseSet Template** - Implement with page rotation
- [ ] **Migrate Transform to Triple Buffer** - Move from Archetype to Core
- [ ] **Update LogicThread** - Add SwapPages() after FixedUpdate
- [ ] **Update RenderThread** - Remove snapshot copy, add direct page access
- [ ] **Integrated Processing Kernel** - Culling + interp + sort key in one pass
- [ ] **Dead Entity Detection** - Skip inactive entities (sentinel value check)
- [ ] **Frustum Culling** - Camera visibility test during interpolation

**Success Criteria:**

- Zero memcpy in snapshot path (verify with profiler)
- ~5ms eliminated for 100k entities (snapshot copy removed)
- InterpBuffer only contains visible entities
- Culling effectiveness measured (% entities skipped)
- Foundation for 1M entity scaling

**Performance Expectations:**

| Entities | Old Flow (Copy+Interp) | New Flow (Direct+Cull+Interp) | Savings |
|----------|------------------------|-------------------------------|---------|
| 100k     | ~8ms                   | ~4ms                          | 50%     |
| 1M       | ~80ms (unacceptable)   | ~20-40ms (with SIMD)          | 50-75%  |

**Key Rules:**

1. Logic NEVER reads from Page₀/Page₁ - only writes to Page₂
2. Render NEVER reads from Page₂ - only reads Page₀/Page₁
3. SwapPages() is atomic - single instruction rotation
4. InterpBuffer is ephemeral - cleared every frame
5. Sort happens AFTER culling - don't sort invisible entities

---

## 4. Networking Architecture: The Deterministic Bridge

Philosophy: Server authority with client-side autonomy. Reject the "destroy and recreate" pattern of standard engines in
favor of "bind and reconcile."

### I. The IO Model (Sentinel-Driven)

Networking is treated as Input, not Logic.

Socket Pump: The Sentinel (Main) Thread owns the GameNetworkingSockets (GNS) context. It pumps callbacks and buffers
packets.

Handoff: Packets are atomic-swapped into the Brain (Logic) Thread's mailbox at the start of the FixedUpdate window.

Benefit: The Logic thread never burns CPU cycles waiting on socket syscalls or packet decryption. It only processes hot,
pre-fetched data.

- This is going to need soe testing and work, if our net tick is 128 and our fixed update is 60 we're creating a lot of
  our own latency.

### II. Identity & Ownership (The 8-Bit Lock)

We embed ownership directly into the 64-bit EntityID to allow for O(1) authority checks without cache misses.

```C++
// Swappable design: Implement GetIndex(), IsValid(), operator== for custom implementations
union EntityID
{
    uint64_t Value;

    // Bitfield layout
    struct
    {
        uint64_t Index      : 20; // 1 Million entities (array slot)
        uint64_t Generation : 16; // 65k recycles (server-grade stability)
        uint64_t TypeID     : 12; // 4k class types (function dispatch)
        uint64_t OwnerID    : 8;  // 256 owners (network routing)
        uint64_t MetaFlags  : 8;  // Reserved for future use
    };           |

Fast Auth Checks: bool IsMine(EntityID id) { return (id >> 56) == LocalClientID; }
```

Routing: Packets are inherently filterable by looking at the second byte of the EntityID.

### III. The "Raw Memory" Protocol

We reject high-level serialization layers (like FArchive) in favor of Memory Replication.

Packet Structure: A trimmed version of the Render Thread's InstanceData.

- This is neat, but we'll also likely need to replicate data from the Shell data as well, How do we allow quick copy
  packet creation as well as user marked replicated data?

Delta Compression: We memcpy the current simulation state against the last acknowledged state (XOR delta) directly from
the Chunk memory.

Customizability: Users can define NetSerialize hooks for specific components, but the default is a raw memory block
copy.

- Add this to the lifetime functionality in the EntityView?

### IV. R&D Hypothesis: Prediction Buffers & Object Linking

The Problem:
Standard engines (like Unreal) handle prediction errors by destroying the client's predicted actor and spawning a new
server-authoritative actor. This can break gameplay logic references and causes visual popping.

The Strigid Solution: "Prediction Binding"

Spawn Prediction: Client spawns a "Predicted Entity" immediately.

Server Handshake: Server sends the authoritative spawn.

Binding: Instead of destroying the local entity, Strigid binds the Server ID to the Local ID. The local entity adopts
the Server's authority but keeps its local memory address active.

- Leveraging rollback and resim systems for this could prove useful, so that the server can insert the predicted object
  exactly where it exists on the client.

### Experimental: "Context-Aware Prediction Paging"

To solve the memory cost of full-world rollback, we are testing a Prediction Buffer system:

Phase 1 (Identify): During FixedUpdate, any component touched by a predicted input is flagged.

Phase 2 (Cache): flagged components are copied to a linear "Prediction Page" before modification.

Phase 3 (Rollback): On misprediction, we memcpy only the Prediction Page back into the Core, rather than restoring the
entire world state.

Mitigation: To prevent "butterfly effect" desync (where a resimulation touches an object that wasn't in the buffer), we
speculatively cache objects within the bounding volume of the predicted entity.

---

## 5. Future Roadmap (Weeks 10-14)

### Phase 4: The Three-Phase Sandwich

**Goal:** Implement Pre-Physics → Sim → Post-Physics execution flow.

**The Loop:**

```cpp
// 1. Pre-Physics (Intent)
for (auto& entity : activeEntities) {
    entity.PrePhysics(dt);  // User sets velocities, apply inputs
}

// 2. The Sim (Class-Agnostic, operates on Core Pages ONLY)
PhysicsSolver.Step(CorePage_Transform, CorePage_Velocity, dt);

// 3. Post-Physics (Reaction)
for (auto& entity : activeEntities) {
    entity.PostPhysics(dt);  // User reads results, trigger events
}

// 4. Atomic Page Swap
CorePageManager.SwapPages();  // Page₁ → Page₀ (commit)
```

**Physics Integration:**

- Map `CorePage::Transform` directly to Jolt Physics bodies
- Zero-copy physics (solver writes directly to Core page)
- Brain never calls virtual functions during Sim

**Tasks:**

- [ ] **Jolt Physics Integration** - Map Core pages to physics world
- [ ] **Pre/Post-Physics Hooks** - Add to entity lifecycle
- [ ] **Collision Callbacks** - Trigger from Post-Physics phase

### Phase 5: Skeletal Animation & Assets

**Goal:** High-performance animation without per-bone objects.

**BoneArray Component:**

```cpp
struct BoneArray<64> {  // Fixed-size, inline storage
    Vector3 Deltas[64];  // Bone position offsets from bind pose
};
```

**Delta-Batching:**

- Sort by SkeletonID (all humanoids together)
- Upload bone matrices in batches
- Compute Skinning Shader applies deltas

**Tasks:**

- [ ] **Asset Pipeline** - Load glTF/OBJ into Mesh/Skeleton assets
- [ ] **BoneArray Component** - Fixed-size inline storage
- [ ] **Skinning Shader** - Compute or Vertex shader bone application
- [ ] **Animation Blending** - Blend tree system

### Phase 6: Advanced Interactions

**Sentinel Input (1000Hz):**

- High-frequency input sampling on Main thread
- Input history buffer for replay/prediction
- Action mapping (raw input → gameplay actions)

**Sentinel UI:**

- Dear ImGui on Main thread for debug overlays
- Zero impact on Logic/Render threads

**Audio:**

- Trigger events from Post-Physics phase
- 3D positional audio tied to Transform

**Camera:**

- FPS camera controller (WASD + mouse look)
- Multiple camera modes (orbit, free-fly, fixed)

**Tasks:**

- [ ] **Input System** - 1000Hz sampling, action maps
- [ ] **Dear ImGui Integration** - Debug UI on Sentinel
- [ ] **Audio System** - SDL3 Audio or OpenAL
- [ ] **Camera Controller** - Multiple modes

### Phase 7: Production Polish

**Binary Serialization:**

- `memcpy` Shell chunks directly to disk
- Instant save/load (no JSON parsing)
- Scene file format

**Entity Validation:**

- `static_assert(std::is_trivially_copyable)` in `STRIGID_REGISTER_CLASS`
- Compile-time enforcement of PoD-only components

**Automated Testing:**

- Unit tests for ECS memory integrity
- Threading handoff tests (verify lock-free correctness)
- Performance regression tests (100k entity baseline)

**Hot Reload:**

- Asset hot-reloading (textures, meshes, shaders)
- Development iteration speed

**Tasks:**

- [ ] **Binary Serialization** - Scene save/load
- [ ] **Component Validation** - Compile-time checks
- [ ] **Test Harness** - Google Test / Catch2
- [ ] **Hot Reload** - Asset watchers

---

## 6. Performance Targets

| Metric                | Target               | Current Status   |
|-----------------------|----------------------|------------------|
| **Throughput**        | 100k @ >230 FPS      | ✅ 100k @ 321 FPS |
| **Latency**           | <16ms input→photon   | 🎯 In progress   |
| **Memory**            | <1GB for 1M entities | 🎯 Target        |
| **Thread Safety**     | Zero race conditions | ✅ TSan clean     |
| **Frame Consistency** | Zero stutters        | ✅ Achieved       |
| **Scalability**       | 1M entities @ 60 FPS | 🎯 CRTP required |

---

## 7. Architectural Rules

1. **Macro-First:** Use `STRIGID_REGISTER_CLASS(T)` before class definitions.
2. **PoD Only:** No `std::vector` or `std::string` inside Components. Use fixed-size inline arrays or handles.
3. **No Logic in Sim:** Physics engine must never call virtual functions on entities.
4. **No Rendering in Logic:** Brain thread must never call `SDL_Render` functions. It only writes to Data Pages.
5. **Lock-Free Communication:** Brain → Encoder uses triple-buffer mailbox. Encoder → Sentinel uses atomic handshake.
6. **Zero Allocations:** Hot paths (simulation, rendering) must not allocate memory per frame.
7. **White Box Philosophy:** Users can understand, debug, and modify all systems. No black-box abstractions.

---

## 8. Data Structures Reference

### FramePacket

```cpp
struct FramePacket {
    ViewState   View;              // Camera matrix, projection (64 bytes)
    SceneState  Scene;             // Sun direction, color (32 bytes)
    double      SimulationTime;    // Seconds since start
    uint32_t    ActiveEntityCount; // Number of entities to render
    uint32_t    FrameNumber;       // Increments each FixedUpdate
};
```

### SnapshotEntry (1:1 with InstanceData)

```cpp
struct alignas(16) SnapshotEntry {
    float PositionX, PositionY, PositionZ;
    float RotationX, RotationY, RotationZ;
    float ScaleX, ScaleY, ScaleZ;
    float _pad0, _pad1, _pad2;
    float ColorR, ColorG, ColorB, ColorA;
};
// Total: 64 bytes (matches GPU InstanceData layout)
```

### InstanceData (GPU Upload Format)

```cpp
struct alignas(16) InstanceData {
    float PositionX, PositionY, PositionZ, _pad0;  // 16 bytes
    float RotationX, RotationY, RotationZ, _pad1;  // 16 bytes
    float ScaleX, ScaleY, ScaleZ, _pad2;           // 16 bytes
    float ColorR, ColorG, ColorB, ColorA;          // 16 bytes
};
// Total: 64 bytes (optimal for GPU vectorization)
```

### TripleBufferedSparseSet

```cpp
template<typename T>
class TripleBufferedSparseSet {
    T* Pages[3];              // Three pages for rotation
    size_t Capacity;          // Elements per page

    void SwapPages() {
        T* temp = Pages[0];
        Pages[0] = Pages[1];  // Newest → Committed
        Pages[1] = Pages[2];  // Writing → Newest
        Pages[2] = temp;      // Committed → Writing
    }

    T* GetPage(int index) const {
        return Pages[index];
    }

    T* GetWritePage() {
        return Pages[2];
    }
};
```

### InterpEntry (Replaces SnapshotEntry)

```cpp
struct InterpEntry {
    uint64_t sortKey;       // For state-sorted rendering
    Vector3 position;       // Interpolated from prev/curr pages
    Vector3 rotation;       // Interpolated from prev/curr pages
    Vector3 scale;          // Interpolated from prev/curr pages
    Color color;            // From ColorData sparse set
};

// Stored in ephemeral buffer
std::vector<InterpEntry> InterpBuffer;  // Cleared each frame
```

**Comparison:**

- **Old:** SnapshotPrevious + SnapshotCurrent (2 full copies, 9.6MB for 100k)
- **New:** InterpPrevious + InterpBuffer only (visible entities only, ~3-7MB for 100k with culling)

```

### DrawCall (Sort Key + Reference)
```cpp
struct DrawCall {
    uint64_t sort_key;      // 64-bit composite key
    uint32_t entity_index;  // Index into sparse set
};
```

---

## 9. Configuration

```cpp
struct EngineConfig {
    int TargetFPS = 0;         // Main thread limiter (0 = uncapped)
    int FixedUpdateHz = 60;    // Logic thread simulation rate
    int NetworkUpdateHz = 30;  // Network tick rate (future)

    double GetFixedStepTime() const {
        return (FixedUpdateHz > 0) ? 1.0 / FixedUpdateHz : 0.0;
    }

    double GetTargetFrameTime() const {
        return (TargetFPS > 0) ? 1.0 / TargetFPS : 0.0;
    }
};
```

---

## 10. Current Status (Week 7)

**Completed:**

- ✅ Strigid Trinity architecture fully operational
- ✅ Lock-free triple-buffer mailbox
- ✅ Frame synchronization handshake
- ✅ Snapshot + interpolation rendering
- ✅ 100k entities @ 321 FPS render, 207 FPS logic, 502 FPS main
- ✅ Zero stutters, zero crashes
- ✅ Proper buffer management (no leaks)

**In Progress:**

- 🔄 State-sorted rendering (64-bit sort keys)
- 🔄 Documentation and architecture refinement

**Next Priority:**

- 🎯 64-bit sort key system (Layer → Pipeline → Material → Mesh)
- 🎯 CRTP entity views for 1M entity scaling
- 🎯 Core/Shell ECS split with triple-buffered hot components
- 🎯 Physics integration (Jolt) with Three-Phase Sandwich

**Architecture Status:**

- 🏗️ Solid foundation with data-oriented design
- 🏗️ Lock-free threading model proven stable under stress
- 🏗️ Ready for state sorting and CRTP optimization
- 🏗️ Performance headroom for 1M entities with planned optimizations

---

## Appendix: Thread Flow Diagram

```
┌─────────────┐         ┌─────────────┐         ┌──────────────┐
│   SENTINEL  │         │    BRAIN    │         │   ENCODER    │
│ (Main/502Hz)│         │ (Logic/207Hz)│         │ (Render/321Hz)│
└─────────────┘         └─────────────┘         └──────────────┘
      │                       │                         │
      │ PumpEvents()          │                         │
      │                       │                         │
      │                       │ FixedUpdate(dt)         │
      │                       │ Update(dt)              │
      │                       │ ProduceFramePacket()    │
      │                       │   │                     │
      │                       │   └──► Mailbox (CAS)    │
      │                       │                    │    │
      │                       │                    └───►│ PollMailbox()
      │                       │                         │ SnapshotSparseArrays()
      │                       │                         │
      │   ◄────────────────────────── bNeedsGPUResources = true
      │                       │                         │
      │ FramePacer.BeginFrame()                        │
      │ AcquireCommandBuffer()                         │
      │ AcquireSwapchainTexture()                      │
      │                       │                         │
      │ ───► CmdBufferAtomic ────────────────────────►│
      │ ───► SwapchainAtomic ────────────────────────►│
      │                       │                         │
      │                       │                InterpolateToTransferBuffer()
      │                       │                BuildCommandBuffer()
      │                       │                         │
      │   ◄────────────────────────── bReadyToSubmit = true
      │                       │                         │
      │ TakeCommandBuffer() ◄──────────────────────────┤
      │ FramePacer.EndFrame(cmd)                       │
      │ Submit to GPU         │                         │
      │                       │                         │
      │ ───► bFrameSubmitted = true ──────────────────►│
      │                       │                         │
      │ WaitForTiming()       │                         │
      │                       │                         │
      └─► Loop                └─► Loop                  └─► Loop
```

---

**End of Master Plan v3.0**
