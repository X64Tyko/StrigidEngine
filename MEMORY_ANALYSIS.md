# Memory Analysis: Your 100k Entities

## Current Stats from Tracy
```
492 allocations
7,872 KB used
1,497.62 MB spanned
```

## What This Means

### The Math
- **492 chunks** × 16KB = **7,872KB** ✓
- **100,000 entities** across 492 chunks
- **~203 entities per chunk** (100,000 / 492)

### Component Layout (CubeEntity)
```cpp
struct CubeEntity {
    Ref<Transform>  transform;  // 48 bytes
    Ref<Velocity>   velocity;   // ~12 bytes (3 floats)
    Ref<ColorData>  color;      // ~16 bytes (4 floats)
}
```

**Per-entity cost**: ~76 bytes
**Chunk overhead**: 64 bytes (reserved header)
**Entities per 16KB chunk**: (16,384 - 64) / 76 = **~214 entities** (theoretical)

You're getting **203 entities/chunk** - close to optimal!

## The 1.5GB "Span" Mystery 🤔

This is the **key insight** about DoD at scale!

### Why So Much Virtual Address Space?

**1. Virtual vs Physical Memory**
- **Used**: 7.8MB = actual RAM/cache being touched
- **Spanned**: 1.5GB = virtual address range
- **Average gap**: 1,497MB ÷ 492 chunks = **~3MB between each chunk!**

That means there are **~187 "empty slots"** (3MB ÷ 16KB) between each allocated chunk!

**2. Possible Causes**

#### A. Memory Allocator Behavior (Most Likely)
C++ `new` operator uses the system allocator (MSVC uses HeapAlloc on Windows):
```
Chunk 1: 0x00007FF800000000 (16KB)
Chunk 2: 0x00007FF800100000 (16KB)  ← 1MB gap!
Chunk 3: 0x00007FF800200000 (16KB)  ← Another gap!
```

The allocator is **fragmenting virtual address space** to:
- Reserve room for growth
- Align to page boundaries
- Reduce TLB pressure (ironically)
- Allow for ASLR (Address Space Layout Randomization)

**This is normal and not a problem!** Virtual address space is cheap (you have 128TB on x64).

#### B. Component Interleaving
If you have multiple component arrays, they're allocated separately:
```
Transforms:  [Chunk1][Chunk2][Chunk3]... (492 chunks)
Velocities:  [Chunk1][Chunk2][Chunk3]... (492 more chunks)
Colors:      [Chunk1][Chunk2][Chunk3]... (492 more chunks)
```

Total: **1,476 chunks** if fully separated!
- 1,476 × 16KB = **23.6MB actual**
- But spread across ~1.5GB virtual range

#### C. STL Container Overhead
Your `std::vector<Chunk*>` in Archetype might be:
- Reserving more capacity than size
- Allocating with growth factor (1.5x or 2x)

### Is This Bad? NO! Here's Why:

**Good News:**
1. ✅ **Only 7.8MB in physical RAM/cache** - excellent!
2. ✅ **Virtual address space is free** (x64 has 128TB)
3. ✅ **TLB entries** only care about pages you actually touch
4. ✅ **Cache lines** only care about sequential access within chunks

**The CPU doesn't load the gaps!** Modern MMUs are smart:
- Virtual addresses are just numbers
- Physical pages are only mapped for used chunks
- Cache prefetcher works on physical addresses

## What Matters for Performance

### Cache Behavior (THIS is DoD)
When you iterate entities:
```cpp
for (Chunk* chunk : archetype->Chunks) {
    Transform* transforms = GetComponentArray<Transform>(chunk);
    for (uint32_t i = 0; i < entitiesPerChunk; i++) {
        transforms[i].PositionX += 1.0f;  // Sequential!
    }
}
```

**Cache lines touched**:
- Transform is 48 bytes = **1 cache line** per entity (64-byte lines)
- 203 entities/chunk = **203 cache lines** per 16KB chunk
- **Prefetcher loves this!** Linear access pattern

**Not touched**:
- The gaps between chunks in virtual memory (not mapped)
- Other component arrays (Velocity, Color) if you're not using them

### TLB Behavior
- **16KB chunks = 4 pages** (4KB pages on Windows)
- **492 chunks = 1,968 pages**
- Modern CPUs have **1,536-2,048 L2 TLB entries**

**You're just barely fitting in TLB!** This is actually near-optimal.

If you had:
- 4KB chunks → 1,968 / 4 = **492 TLB entries** ✓ (fits easily)
- 64KB chunks → 1,968 × 4 = **7,872 TLB entries** ✗ (TLB thrashing!)

**Your 16KB chunk size is perfect** for 100k entities.

## Experiments to Try

### Experiment 1: Measure Actual Fragmentation
Add this to your profiling:

```cpp
// In Archetype::AllocateChunk()
static void* lastChunk = nullptr;
if (lastChunk) {
    ptrdiff_t gap = (char*)NewChunk - (char*)lastChunk;
    STRIGID_PLOT("Chunk Gap (KB)", gap / 1024);
}
lastChunk = NewChunk;
```

**What to look for**: Are chunks adjacent or scattered?

### Experiment 2: Custom Allocator
Replace `new Chunk()` with a custom arena allocator:

```cpp
class ChunkArena {
    char* buffer = new char[100 * 1024 * 1024];  // 100MB contiguous
    size_t offset = 0;

public:
    Chunk* Allocate() {
        Chunk* result = (Chunk*)(buffer + offset);
        offset += sizeof(Chunk);
        return result;
    }
};
```

**Expected result**: "Spanned" drops to ~8MB (matches "used")

### Experiment 3: Smaller vs Larger Chunks
Modify `CHUNK_SIZE` and compare:

| Chunk Size | Chunks Needed | TLB Entries | Virtual Span | Prediction |
|------------|---------------|-------------|--------------|------------|
| 4KB        | 1,968         | 492         | ~400MB       | ✓ Best TLB |
| 16KB       | 492           | 1,968       | ~1.5GB       | ✓ Balanced |
| 64KB       | 123           | 7,872       | ~6GB         | ✗ TLB miss |

**What to watch**: Frame time variance (TLB misses show up as stutters)

## The Bigger Picture

### What You're Actually Learning

**Small Scale (Cache Lines)**:
- Transform fits in 1 cache line (48 bytes < 64 bytes)
- Sequential access = prefetcher wins
- SoA layout avoids loading unused components

**Medium Scale (Pages/TLB)**:
- 16KB chunks = 4 pages each
- ~2,000 pages total = fits in L2 TLB
- Page-aligned chunks reduce TLB pressure

**Large Scale (Virtual Memory)**:
- 1.5GB span is fragmentation artifact
- Only 7.8MB physically resident
- Virtual address space is "free" (not a resource)

### DoD Hierarchy
```
L1 Cache (32KB)  ← Hot loop fits here (1-2 chunks)
L2 Cache (256KB) ← Working set (16 chunks, ~3k entities)
L3 Cache (8MB)   ← Your entire dataset fits here! (7.8MB)
RAM (16GB)       ← Not even needed yet
Virtual (128TB)  ← 1.5GB is 0.001% of available space
```

**Your 100k entities fit entirely in L3 cache!** That's why you're hitting 130 FPS.

## Recommended Reading Now

With this hands-on data:

1. **"What Every Programmer Should Know About Memory" - Ulrich Drepper**
   - Section 3.5: "TLB" (you're seeing this!)
   - Section 4.1: "CPU Caches" (explains your 130 FPS)

2. **"Memory Mountain" - CMU 15-213 Lab**
   - Measures bandwidth at different strides
   - Your stride is perfect (sequential)

3. **"Virtual Memory in the IA-32" - Intel Manual Vol 3**
   - Chapter 3: Paging mechanism
   - Explains the 1.5GB span mystery

4. **Mike Acton's "Data Oriented Design" talk**
   - Re-watch with your Tracy data in hand
   - You're now seeing the patterns he describes!

## Conclusion

Your memory layout is **actually excellent**:
- ✅ 203 entities/chunk is near theoretical max (214)
- ✅ 7.8MB working set fits in L3 cache
- ✅ Linear memory access = prefetcher happy
- ✅ TLB footprint is manageable

The 1.5GB "span" is:
- ⚠️  Virtual address fragmentation (harmless)
- 🎯  Opportunity to learn about virtual vs physical memory
- 💡  Could be optimized with custom allocator (but unnecessary)

**Next**: Try the experiments above to understand the trade-offs!
