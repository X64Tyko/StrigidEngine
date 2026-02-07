# Tracy Memory Profiling Guide - Understanding DoD Memory Layout at Scale

This guide shows you how to use Tracy to understand memory patterns in your Data-Oriented ECS beyond individual cache lines.

## Setup Complete ✓

Your engine now tracks:
- **Chunk allocations** per archetype (named pools)
- **Memory allocation patterns** over time
- **Component memory layouts** in Structure-of-Arrays format

## What You Can Learn

### 1. **Cache Line vs Page-Level Locality**
- Each chunk is 16KB (default CHUNK_SIZE)
- Components are laid out in SoA: `[Transform₀, Transform₁, ...][Velocity₀, Velocity₁, ...]`
- Tracy shows you:
  - How many chunks each archetype allocates
  - When chunks are allocated (frame-by-frame)
  - Memory fragmentation over time

### 2. **Memory Pool Segregation**
Your archetypes now have named memory pools:
- "CubeEntity" pool - Transform + Velocity + ColorData
- Different entity types get different pools
- Tracy's Memory view shows each pool separately

### 3. **TLB and Page Behavior**
- Chunks are 16KB (4 pages on most systems)
- Watch for TLB thrashing when iterating across many chunks
- Tracy timeline shows memory access patterns

### 4. **Allocation Patterns**
- **Burst allocation**: Creating 100k entities at startup
- **Steady-state**: Frame-to-frame allocations
- **Fragmentation**: Gaps in memory after entity destruction

## Using Tracy

### Step 1: Build with Tracy Enabled
Your project already has Tracy integrated. Build with:
```bash
# Build with Tracy profiling level 2 (includes memory zones)
cmake -DTRACY_ENABLE=ON -DTRACY_PROFILE_LEVEL=2 ..
cmake --build .
```

### Step 2: Run the Tracy Profiler
1. Download Tracy profiler GUI: https://github.com/wolfpld/tracy/releases
2. Run your engine (StrigidEngine.exe)
3. Open Tracy profiler
4. Click "Connect" - it will find your running application

### Step 3: Memory Analysis Views

#### **Memory View** (Main Tab)
- Click "Memory" button in toolbar
- See live allocations by pool:
  - "CubeEntity" - your main entity archetype
  - Other archetypes as you add them
- Watch memory grow as entities spawn
- See deallocations when entities are destroyed

#### **Memory Map**
- Visual representation of memory layout
- Each pixel = memory region
- Color = allocation pool
- **Key insight**: See fragmentation and locality patterns
  - Contiguous blocks = good cache behavior
  - Scattered allocations = cache misses

#### **Timeline View**
- Zoom into frame-by-frame allocations
- Correlate memory allocations with performance zones
- See when chunks are allocated during initialization vs runtime

### Step 4: Example Scenarios to Try

#### Scenario A: Sequential vs Random Access
```cpp
// Add to your Update loop to see different patterns:

// GOOD: Sequential access (cache-friendly)
void UpdateSequential() {
    for (Chunk* chunk : archetype->Chunks) {
        Transform* transforms = archetype->GetComponentArray<Transform>(chunk, transformTypeID);
        for (uint32_t i = 0; i < entitiesPerChunk; i++) {
            transforms[i].PositionX += 1.0f;  // Linear memory access
        }
    }
}

// BAD: Random access (cache-hostile)
void UpdateRandom() {
    std::vector<uint32_t> indices = GetRandomIndices();
    for (uint32_t idx : indices) {
        // Jumping between chunks - watch cache misses!
        Transform* t = GetTransformByGlobalIndex(idx);
        t->PositionX += 1.0f;
    }
}
```

**What to watch**: Tracy's memory timeline will show page faults and allocation patterns.

#### Scenario B: Memory Locality Experiment
```cpp
// Compact layout (CURRENT - SoA within chunks)
struct Chunk {
    Transform transforms[1024];  // 48KB contiguous
    Velocity velocities[1024];   // 24KB contiguous
};

// vs Scattered layout (for comparison)
struct BadChunk {
    struct { Transform t; Velocity v; } entities[1024];  // Interleaved
};
```

**What to watch**:
- SoA layout = fewer cache lines loaded per iteration
- Tracy shows bandwidth utilization
- Profile both and compare frame times

#### Scenario C: Chunk Size Experiments
Modify `CHUNK_SIZE` in Types.h and compare:
- 4KB chunks (1 page) - max TLB efficiency, more allocations
- 16KB chunks (current) - balanced
- 64KB chunks - fewer allocations, potential TLB misses

**What to watch**:
- Number of allocations (Memory view)
- Frame time variation (Timeline)
- Memory fragmentation (Memory Map)

## Understanding the Output

### Good Memory Patterns
- **Contiguous blocks** in Memory Map
- **Predictable allocation** in Timeline (startup burst, then stable)
- **Low fragmentation** after entity destruction
- **Cache-friendly iteration** visible in zone times

### Bad Memory Patterns
- **Scattered allocations** across many pages
- **Frame-to-frame allocation churn** (constant alloc/free)
- **Growing memory** without bound
- **Slow zones** with many cache misses

## Key Metrics to Track

Add these to your main loop:
```cpp
STRIGID_PLOT("Total Chunks", registry.GetTotalChunkCount());
STRIGID_PLOT("Active Entities", registry.GetEntityCount());
STRIGID_PLOT("Memory Used (MB)", registry.GetMemoryUsageMB());
```

Tracy will graph these over time - look for:
- Steady-state behavior
- Memory leaks (growing without bound)
- Allocation spikes

## Advanced: Multi-Archetype Patterns

When you have multiple entity types:
```cpp
// Fast entities (small, hot data)
struct FastEntity {
    Ref<Transform> transform;
    Ref<Velocity> velocity;
};

// Fat entities (large, cold data)
struct FatEntity {
    Ref<Transform> transform;
    Ref<Mesh> mesh;           // Large buffer
    Ref<Material> material;   // Texture references
    Ref<Animation> animation; // State machine
};
```

**What to watch**:
- Separate pools in Memory view
- Different chunk allocation patterns
- Cache behavior when iterating each type

## Reading Material

Now that you have hands-on profiling:

1. **Mike Acton - "Data-Oriented Design and C++"**
   - CppCon 2014 talk
   - Demonstrates these exact patterns at scale

2. **"What Every Programmer Should Know About Memory"**
   - Ulrich Drepper (2007)
   - Deep dive into TLB, cache hierarchies, NUMA

3. **"Game Engine Architecture" - Jason Gregory**
   - Chapter 5: Memory Management
   - Chapter 15: Runtime Gameplay Foundation (ECS)

4. **Tracy Manual**
   - `libs/tracy/manual/tracy.pdf`
   - Memory profiling section (Chapter 3.6)

## Questions to Answer with Tracy

1. **How many cache lines are touched per entity update?**
   - Hint: Profile Transform update loop at level 3

2. **What's the memory bandwidth utilization?**
   - Hint: Compare frame times with different chunk sizes

3. **How does fragmentation affect performance?**
   - Hint: Create/destroy entities in patterns, watch Memory Map

4. **What's the optimal EntitiesPerChunk?**
   - Hint: Experiment with component sizes and profile iteration times

5. **Are you TLB-bound or cache-bound?**
   - Hint: Compare small chunks (TLB-friendly) vs large chunks (cache-friendly)

## Next Steps

1. **Baseline**: Profile your current 100k entities
2. **Experiment**: Try different chunk sizes
3. **Compare**: SoA vs AoS layouts (requires code changes)
4. **Optimize**: Based on Tracy's insights

Happy profiling! 🚀
