# StrigidEngine Architecture

> **Navigation:** [← Back to README](../README.md) | [Performance Targets →](PERFORMANCE_TARGETS.md)

---

# Threading Model: The Strigid Trinity

## Overview

StrigidEngine uses a **three-thread architecture** with job-based parallelism:

1. **Sentinel (Main Thread):** 1000Hz input polling, GPU resource management, frame pacing
2. **Brain (Logic Thread):** 512Hz fixed timestep coordinator + job distribution
3. **Encoder (Render Thread):** Variable-rate render coordinator + job distribution

**Key Design:** Brain and Encoder are **coordinators**, not workers. They initialize frames, distribute work to the job system, and act as workers themselves while jobs are pending.

## Job System Architecture

### Core Count Distribution (8-core example)

- **Sentinel Thread:** 1 dedicated core (input + GPU resource management)
- **Encoder Thread:** 1 dedicated core (render coordination + render worker)
- **Brain Thread:** 1 dedicated core (logic coordination + logic worker)
- **Workers:** 5 cores (process tasks from all queues)

### Worker Pool

The job system maintains a shared pool of worker threads with queue affinity rules:

```cpp
// Simplified job system structure
struct JobSystem {
    static constexpr int WorkerCount = 6;  // Hardware threads - 2 (Sentinel + Encoder)

    // Separate queues for Logic and Render jobs
    MPMCQueue<Job> LogicQueue;
    MPMCQueue<Job> RenderQueue;

    std::thread Workers[WorkerCount];
    std::atomic<bool> IsRunning;
};
```

**Queue Affinity Rules:**
- **Brain Thread:** Only pulls from `LogicQueue` (when acting as worker)
- **Encoder Thread:** Only pulls from `RenderQueue` (when acting as worker)
- **Generic Workers (5 threads):** Pull from both `LogicQueue` and `RenderQueue` (work-stealing)

This design prevents the Brain from accidentally processing render jobs and vice versa, while allowing generic workers to balance load across both queues.

### Job Types

**Logic Jobs:**
- PrePhysics updates (per-archetype chunk)
- Physics simulation (per-chunk or per-rigid-body-group)
- PostPhysics updates (per-archetype chunk)

**Render Jobs:**
- Interpolation (per-entity-range)
- Frustum culling (per-chunk)
- Sort key generation (per-visible-entity)
- GPU command encoding (per-material/mesh batch)

### Performance Impact

**Example: 100k entities @ 128Hz on 8-core CPU**

Single-threaded Brain: ~1.7ms per frame
- PrePhysics: ~0.3ms
- Physics: ~1.0ms (projected)
- PostPhysics: ~0.4ms (projected)

Multi-threaded (6 workers @ 80% efficiency):
- PrePhysics: ~0.25ms (4x speedup)
- Physics: ~0.37ms (4x speedup)
- PostPhysics: ~0.13ms (4x speedup)
- **Total: ~0.75ms** (well within 1.95ms budget)

**Efficiency factors:**
- Job submission overhead (~5%)
- Cache coherency between cores (~10%)
- Load balancing variance (~5%)
- **Realistic efficiency: 75-85%**

---

# Memory Model: History Slab Architecture

## Overview: The History Slab

The History Slab is a custom arena allocator with integrated history buffering and defragmentation support. It replaces the previously planned "triple-buffered sparse sets" with a more flexible system that supports rendering, networking, rollback, prediction, lag compensation, and replay.

### Core Concept

**Single Contiguous Allocation** divided into X sections (user-configurable power-of-2, default 128 or 4):
- Each section represents one frame's worth of hot component data
- Sections are accessed via: `Section[FrameNumber % SectionCount]`
- All hot components (Transform, Velocity, etc.) live ONLY in the History Slab
- Cold components remain in Archetype chunks

**Memory Layout:**

```
┌───────────────────────────────────────────────────────────────────────┐
│                    HISTORY SLAB (Arena Allocator)                     │
│                   Total Size: SectionSize × SectionCount              │
├───────────────────────────────────────────────────────────────────────┤
│  Section 0 (Frame % 128 == 0)                                         │
│  ┌─────────────────────────────────────────────────────────────────┐ │
│  │ HistorySectionHeader                                            │ │
│  │   - OwnershipFlags (atomic bitfield)                            │ │
│  │   - FrameNumber                                                 │ │
│  │   - ViewMatrix, ProjectionMatrix, CameraPosition                │ │
│  │   - SunDirection, SunColor                                      │ │
│  │   - ActiveEntityCount                                           │ │
│  ├─────────────────────────────────────────────────────────────────┤ │
│  │ Transform Block A [entities 0..255]   ← Archetype requested     │ │
│  │   - PositionX[256], PositionY[256], PositionZ[256]              │ │
│  │   - RotationX[256], RotationY[256], RotationZ[256]              │ │
│  │   - ScaleX[256], ScaleY[256], ScaleZ[256]                       │ │
│  ├─────────────────────────────────────────────────────────────────┤ │
│  │ Transform Block B [entities 256..511]                           │ │
│  │   - (Same field layout as Block A)                              │ │
│  ├─────────────────────────────────────────────────────────────────┤ │
│  │ Velocity Block A [entities 0..255]                              │ │
│  │   - VelocityX[256], VelocityY[256], VelocityZ[256]              │ │
│  ├─────────────────────────────────────────────────────────────────┤ │
│  │ [Inactive entity markers / gap metadata]                        │ │
│  └─────────────────────────────────────────────────────────────────┘ │
├───────────────────────────────────────────────────────────────────────┤
│  Section 1 (Frame % 128 == 1)                                         │
│  ┌─────────────────────────────────────────────────────────────────┐ │
│  │ HistorySectionHeader { ... }                                    │ │
│  ├─────────────────────────────────────────────────────────────────┤ │
│  │ Transform Block A [entities 0..255]  ← SAME OFFSET as Section 0 │ │
│  │ Transform Block B [entities 256..511]                           │ │
│  │ Velocity Block A [entities 0..255]                              │ │
│  │ [Gaps...]                                                       │ │
│  └─────────────────────────────────────────────────────────────────┘ │
├───────────────────────────────────────────────────────────────────────┤
│  Section 2, 3, 4... (up to 127 or configured max)                    │
│    - All sections maintain identical layout/offsets                   │
│    - Defragmentation must keep all sections in sync                   │
└───────────────────────────────────────────────────────────────────────┘
```

---

## Allocation Strategy

### Archetype Requests Space

When an Archetype is created with hot components:

```cpp
// Archetype determines it needs Transform (9 fields × 256 entities = 2304 floats)
size_t bytesNeeded = 9 * 256 * sizeof(float);  // ~9 KB

// Request allocation from History Slab
void* basePtr = HistorySlab.AllocateForArchetype(bytesNeeded);
// Returns pointer to offset X in Section 0

// ALL future sections maintain this offset
// Section 1 has Transform Block A at (Section1BasePtr + X)
// Section 2 has Transform Block A at (Section2BasePtr + X)
// etc.
```

**Key Properties:**
- Allocation happens once per Archetype (not per frame)
- Same offset across all sections ensures consistent pointer arithmetic
- Jumping to next frame is trivial: `ptr += sectionSize`

### Field Array Table Construction

The Archetype knows where hot components live in the History Slab:

```cpp
void Archetype::BuildFieldArrayTable(uint32_t frameNum, void** fieldArrayTable)
{
    void* sectionBase = HistorySlab.GetSection(frameNum % SectionCount);
    void* transformBlock = (char*)sectionBase + transformOffset;

    // Build field array pointers into the History Slab
    fieldArrayTable[0] = (char*)transformBlock + offsetof(TransformSoA, PositionX);
    fieldArrayTable[1] = (char*)transformBlock + offsetof(TransformSoA, PositionY);
    fieldArrayTable[2] = (char*)transformBlock + offsetof(TransformSoA, PositionZ);
    // ... etc for all 9 Transform fields

    // Cold components still come from Archetype chunk memory
    fieldArrayTable[9] = chunk->GetComponentArray<HealthComponent>();
}
```

---

## Thread Access Pattern

### Logic Thread (Brain) - Job Coordinator

The Brain thread acts as a **job coordinator** rather than doing all work itself. On an 8-core system:
- **Sentinel Thread:** Input polling and GPU resource management (1 core)
- **Encoder Thread:** Render job coordination (1 core)
- **Brain Thread + Workers:** Logic coordination + work distribution (6 cores)

The Brain thread initializes the frame, distributes work to the job system, and acts as a worker itself while jobs are pending.

```cpp
void LogicThread::FixedUpdate()
{
    uint32_t currentFrame = FrameNumber;
    uint32_t writeFrame = (currentFrame + 1) % SectionCount;
    uint32_t readFrame = currentFrame % SectionCount;

    // Check if writeFrame is available
    HistorySection* writeSection = slab.GetSection(writeFrame);
    if (writeSection->IsLockedByRender() && Config.DeterministicFrames) {
        // Wait for render thread to release
        while (writeSection->IsLockedByRender()) { /* spin */ }
    }
    // else: Non-deterministic mode, skip to next available section

    // Mark as writing
    writeSection->SetOwnership(LOGIC_WRITING);

    // Build field array tables for all archetypes
    void* readFieldArrays[64];
    void* writeFieldArrays[64];
    archetype->BuildFieldArrayTable(readFrame, readFieldArrays);
    archetype->BuildFieldArrayTable(writeFrame, writeFieldArrays);

    // Distribute work across job system (Brain becomes a worker during this phase)
    JobSystem::SubmitLogicJobs(
        LogicJobType::PrePhysics,
        archetypes,
        writeFieldArrays,
        dt
    );

    // Brain thread acts as worker while jobs pending
    JobSystem::ProcessLogicQueueAsWorker();

    // Wait for all PrePhysics jobs to complete
    JobSystem::WaitForLogicJobs();

    // Write frame-specific data to section header
    writeSection->Header.FrameNumber = currentFrame + 1;
    writeSection->Header.ViewMatrix = camera.GetViewMatrix();
    writeSection->Header.ProjectionMatrix = camera.GetProjection();
    // ... etc

    // Publish to renderer
    LastFrameWritten.store(currentFrame + 1, std::memory_order_release);

    FrameNumber++;
}
```

**Performance Impact (8-core example):**
- Single-threaded Logic: ~1.7ms per frame
- Multi-threaded (6 workers @ 80% efficiency): ~0.43ms per frame (projected)
- **Scalability:** 4x speedup with 6 worker threads

**Ownership Rules:**
- Logic reads from `Section[Frame % N]`
- Logic writes to `Section[(Frame+1) % N]`
- Logic NEVER touches `Section[(Frame-1) % N]` (owned by Render)

### Render Thread (Encoder) - Job Coordinator

The Encoder thread acts as a **job coordinator** for rendering work:
- Initializes the frame and reads History Slab sections
- Distributes interpolation, culling, and sorting work to the job system
- Acts as a worker while jobs are pending
- On an 8-core system, Encoder + workers can parallelize sorting, culling, and GPU command encoding

```cpp
void RenderThread::ProcessFrame()
{
    // Poll for new frame
    uint32_t latestFrame = Logic->LastFrameWritten.load(std::memory_order_acquire);

    // Interpolation needs T-1 and T
    uint32_t frameCurr = latestFrame % SectionCount;
    uint32_t framePrev = (latestFrame - 1) % SectionCount;

    HistorySection* currSection = slab.GetSection(frameCurr);
    HistorySection* prevSection = slab.GetSection(framePrev);

    // Mark as reading (atomic bitfield)
    currSection->SetOwnership(RENDER_READING);
    prevSection->SetOwnership(RENDER_READING);

    // Calculate interpolation alpha
    float alpha = CalculateAlpha();

    // Distribute work to job system (interpolation, culling, sort key generation)
    JobSystem::SubmitRenderJobs(
        RenderJobType::InterpolateAndCull,
        currSection,
        prevSection,
        alpha,
        camera
    );

    // Encoder thread acts as worker while jobs pending
    JobSystem::ProcessRenderQueueAsWorker();

    // Wait for all jobs to complete
    JobSystem::WaitForRenderJobs();

    // Release ownership
    currSection->ClearOwnership(RENDER_READING);
    prevSection->ClearOwnership(RENDER_READING);

    // Submit final GPU commands
    SubmitGPUCommands();

    lastRenderedFrame = latestFrame;
}
```

**Job System Benefits:**
- **Parallel Interpolation:** Split entity ranges across workers
- **Parallel Culling:** Frustum culling per chunk
- **Parallel Sort:** Sort visible entities by 64-bit keys in parallel
- **Scalability:** Multi-core systems see significant speedup

**Ownership Rules:**
- Render reads from `Section[(Frame-1) % N]` and `Section[Frame % N]`
- Render NEVER writes to History Slab
- Both Logic and Render can read from `Section[Frame % N]` simultaneously (no contention)

---

## HistorySectionHeader

Each section begins with metadata:

```cpp
struct HistorySectionHeader
{
    // Ownership tracking (atomic bitfield)
    std::atomic<uint8_t> OwnershipFlags;
    /*
        0x01 = LOGIC_WRITING
        0x02 = RENDER_READING
        0x04 = NETWORK_READING
        0x08 = DEFRAG_LOCKED
        Multiple readers can coexist (bitwise OR)
    */

    // Frame identification
    uint32_t FrameNumber;

    // Camera/View data (replaces FramePacket)
    Matrix4 ViewMatrix;
    Matrix4 ProjectionMatrix;
    Vector3 CameraPosition;

    // Scene/Lighting data
    Vector3 SunDirection;
    Vector3 SunColor;
    float AmbientIntensity;

    // Entity metadata
    uint32_t ActiveEntityCount;
    uint32_t TotalAllocatedEntities;

    // Padding to cache line
    char _padding[/* calculated */];
};
```

**Benefits:**
- Replaces `FramePacket` entirely
- Camera data travels with the frame in the History Slab
- Enables client-side replay by reading old sections
- Network can read historical frames for lag compensation

---

## Defragmentation

### Slab-Level Defragmentation

**Problem:** Over time, Archetype allocations may become fragmented (gaps between blocks).

**Solution:** Slab defrag moves entire Archetype blocks to compact memory.

**Challenge:** Must keep ALL sections in sync.

```cpp
void HistorySlab::DefragArchetypeBlock(ArchetypeID id)
{
    // 1. Lock ALL sections
    for (int i = 0; i < SectionCount; ++i) {
        sections[i].SetOwnership(DEFRAG_LOCKED);
    }

    // 2. Find new contiguous location
    size_t newOffset = FindContiguousSpace(archetypeSize);

    // 3. Move block in EVERY section
    for (int i = 0; i < SectionCount; ++i) {
        void* oldPtr = (char*)sections[i].base + oldOffset;
        void* newPtr = (char*)sections[i].base + newOffset;
        memcpy(newPtr, oldPtr, archetypeSize);
    }

    // 4. Update Archetype's offset metadata
    archetype->SetHistorySlabOffset(newOffset);

    // 5. Release locks
    for (int i = 0; i < SectionCount; ++i) {
        sections[i].ClearOwnership(DEFRAG_LOCKED);
    }
}
```

**Status:** Ideation phase, not yet implemented.

### Entity-Level Defragmentation

**Problem:** Within an Archetype's allocation, entities may have gaps (destroyed entities).

**Solution:** Archetype defrag compacts entities within its block.

**Challenge:** Must update all 128 sections to maintain history consistency.

**Status:** Design work needed. Low priority.

---

## Configuration Options

### Section Count

From `EngineConfig.h`:

```cpp
int HistoryBufferPages = 128;  // Must be power of 2, min 8
```

**Memory Impact:**

Assumes 100k entities with 90/10 split between simple and complex entities:

**Hot Component Sizes (Physics-Enabled):**
- **Simple Entity:** Transform (36B) + Velocity (24B) + Forces (24B) + Collider (32B) = **116 bytes**
- **Complex Entity:** Transform (36B) + Velocity (24B) + Forces (24B) + Collider (32B) + BoneArray/Extra (200B) = **316 bytes**

**Per-Frame Calculation (100k entities):**
- Simple (90k): 116 bytes × 90,000 = 10.44 MB
- Complex (10k): 316 bytes × 10,000 = 3.16 MB
- **Base per-frame:** 13.6 MB

| Config | @ 128Hz | History Duration | Memory (100k Mixed) | Simple Only (100k) | Complex Only (100k) |
|--------|---------|------------------|---------------------|--------------------|--------------------|
| 4      | 128Hz   | 0.03 seconds     | 54.4 MB             | 46.4 MB            | 126.4 MB           |
| 16     | 128Hz   | 0.125 seconds    | 217.6 MB            | 185.6 MB           | 505.6 MB           |
| 64     | 128Hz   | 0.5 seconds      | 870.4 MB            | 742.4 MB           | 2.02 GB            |
| 128    | 128Hz   | 1.0 second       | 1.74 GB             | 1.48 GB            | 4.04 GB            |
| 128    | 512Hz   | 0.25 seconds     | 1.74 GB             | 1.48 GB            | 4.04 GB            |

**Use Cases:**
- **4 sections:** Rendering only (T-2, T-1, T, T+1) - Minimum viable for interpolation
- **16-32 sections:** Client prediction + rollback - Good balance for networked games
- **64 sections:** Extended lag compensation, replay recording
- **128 sections:** GGPO-style rollback, full replay system, 1s @ 128Hz or 0.25s @ 512Hz

**Note:** These numbers reflect ONLY hot components (stored in History Slab with full history). Cold components (AI state, inventory, health, etc.) live in archetype chunks without history, adding ~20-50 MB regardless of page count. Total memory footprint = History Slab + Cold Components + ECS Metadata (~5-10%).

### Deterministic vs Non-Deterministic

```cpp
bool DeterministicFrames = false;  // Default: skip frames if render falls behind
```

**Deterministic Mode (true):**
- Logic waits if it loops around and Render still holds a section
- Guarantees perfect history consistency
- May stall Logic thread if Render is slow

**Non-Deterministic Mode (false):**
- Logic grabs next available section and continues
- If Render is slow, Logic may "lap" it (overwrite old data)
- Render adapts by tracking `LastFrameWritten` and jumping to newest frame
- Better for real-time performance, acceptable for single-player

---

## Benefits of History Slab

1. **Zero-Copy Rendering:** Render reads directly from committed sections (no memcpy)
2. **Integrated Interpolation:** T-1 and T are adjacent in memory layout
3. **Replay Support:** Read old sections to reconstruct past frames
4. **Rollback/Prediction:** Network can rewind to frame N, resimulate forward
5. **Lag Compensation:** Server reads client's historical position from History Slab
6. **GGPO Integration:** Multiple frames of history enable rollback netcode
7. **Frame-Specific Metadata:** Camera, lighting, entity counts travel with the frame
8. **Flexible History Depth:** Users choose 4-128+ sections based on needs

---

## Migration Status

**Current (Week 3):**
- ✅ FieldProxy system operational
- ✅ Components decompose into SoA field arrays
- ✅ Archetype chunk iteration works
- ❌ History Slab not yet implemented
- ❌ Components still live in Archetype chunks
- ❌ Render still uses FramePacket + snapshot copies

**Next Steps:**
1. Implement `HistorySlab` allocator with section management
2. Implement `HistorySectionHeader` with ownership atomics
3. Migrate Transform to History Slab (first hot component)
4. Update `Archetype::BuildFieldArrayTable` to point into slab
5. Update Logic thread to write to `Section[(Frame+1) % N]`
6. Update Render thread to read from `Section[Frame % N]` and `Section[(Frame-1) % N]`
7. Remove FramePacket mailbox, use `LastFrameWritten` atomic instead
8. Benchmark memory usage and performance vs current snapshot system

---
