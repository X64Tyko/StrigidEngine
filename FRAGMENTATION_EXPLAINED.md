# The 3MB Gap Mystery - Why Your Chunks Are So Far Apart

## Your Numbers
```
492 allocations
7,872 KB used     (actual memory in chunks)
1,497 MB spanned  (virtual address range)
```

## The Correct Math

**Used memory**: 492 chunks × 16KB = 7,872KB ✓

**Virtual span**: 1,497MB ÷ 492 chunks = **~3MB average distance between chunks**

**Efficiency**: 7,872KB ÷ 1,497MB = **0.5%** (only 0.5% of the virtual range is actually used!)

## What's in Those 3MB Gaps?

**Option 1: Nothing** (most likely)
- The system allocator (`new`) is just giving you addresses far apart
- Virtual memory is "free" - it's just numbers
- Physical pages are only allocated for the 16KB chunks you use

**Option 2: Other Allocations** (also likely)
- Your `std::vector`, `std::unordered_map`, and other STL containers
- SDL3 allocations (window, GPU buffers)
- Tracy profiler's own memory
- C++ runtime overhead

**Option 3: Allocator Strategy**
Windows HeapAlloc intentionally spreads allocations to:
- **ASLR**: Security feature randomizes addresses
- **Fragmentation prevention**: Leave room for variable-sized allocations
- **Thread safety**: Separate heap segments per thread
- **Growth room**: Space for realloc/resize operations

## Why This Happens

### The System Allocator is NOT a Bump Allocator

When you call `new Chunk()` repeatedly, you're not getting:
```
[Chunk][Chunk][Chunk][Chunk]... ✗
```

You're getting something like:
```
[Chunk]...[3MB of other stuff]...[Chunk]...[3MB]...[Chunk]
```

### What's Actually Happening (Hypothesis)

1. **You create 100,000 entities rapidly**
2. **Each entity creation involves**:
   - Registry allocations (EntityRecord, vectors growing)
   - Component Ref<T> creations
   - Archetype metadata updates
3. **Chunks are allocated mixed with other allocations**:
   ```
   new Chunk()           ← 16KB chunk
   vector.push_back()    ← STL allocation
   unordered_map insert  ← Hash table growth
   new Chunk()           ← Another 16KB chunk (now 3MB away!)
   ```

## Visual Representation

```
Virtual Address Space (1.5GB span):
0x00000000 [Chunk] [???] [???] [???] [Chunk] [???] [???] [Chunk] ...
           ↑ 16KB  ↑ ~3MB gap      ↑ 16KB  ↑ gap  ↑ 16KB

Physical Memory (~8MB used):
[Chunk 1][Chunk 2][Chunk 3]...[Chunk 492]
    ↑ These are contiguous in physical memory (cache-friendly!)
    ↑ But scattered in virtual memory (doesn't matter for performance!)
```

## Does This Matter for Performance?

**NO! Here's why:**

### What the CPU Actually Does

**Virtual Address Translation (Per Memory Access)**:
1. You access: `transforms[i].PositionX` at virtual address `0x7FF823450010`
2. CPU's MMU translates: Virtual `0x7FF823450010` → Physical `0x12340010`
3. CPU loads from physical address into cache

**The gaps don't exist in physical memory!**

### Cache Behavior (What Actually Matters)
```cpp
// Iterating through a chunk
for (uint32_t i = 0; i < 203; i++) {
    transforms[i].PositionX += 1.0f;  // Sequential in PHYSICAL memory
}
```

**Cache prefetcher sees**:
- Load from physical address `0x12340000`
- Next access at `0x12340030` (48 bytes later)
- Prefetcher: "Oh, sequential! Let me grab the next few cache lines!"

**Cache prefetcher does NOT see**:
- The 3MB gap in virtual address space
- Virtual addresses at all (MMU translates first)

### TLB Behavior
- TLB caches: Virtual Page → Physical Page mappings
- Only pages you TOUCH get TLB entries
- The 3MB gaps = unmapped virtual pages = no TLB entries needed
- Your 492 chunks = ~1,968 pages = fits in L2 TLB

## Proving It With Tracy

With the new diagnostics, you'll see:

**Plots Added:**
1. **"Chunk Gap (KB)"** - Shows gap between each allocation
   - Expect: highly variable (16KB to 10MB+)
   - Proves: allocator is not giving you contiguous addresses

2. **"Total Span (MB)"** - Running total of virtual span
   - Expect: grows to ~1500MB by the end
   - Watch: grows much faster than actual usage

3. **"Efficiency %"** - (Used / Spanned) × 100
   - Expect: drops to ~0.5%
   - Shows: how little of virtual space you're using

4. **Text Annotations** - Logs gaps > 100KB
   - You'll see: "Large gap detected: 3,456 KB between chunk 123 and 124"
   - Confirms: something else is allocating between your chunks

## What You Should See in Tracy

Run your build and look at the plots:

**Hypothesis 1: Uniform Gaps**
```
Chunk Gap: [3000] [3000] [3000] [3000] [3000]
→ Allocator is intentionally spacing chunks
```

**Hypothesis 2: Variable Gaps**
```
Chunk Gap: [20] [4500] [100] [8000] [30] [2000]
→ Other allocations are happening between chunks
```

**Hypothesis 3: Clusters with Gaps**
```
Chunk Gap: [16] [16] [16] [5000] [16] [16] [16] [5000]
→ Chunks allocated in batches, gaps between batches
```

## Solutions (If You Want to Fix It)

### Option 1: Custom Chunk Allocator
```cpp
class ChunkAllocator {
    static constexpr size_t ARENA_SIZE = 10 * 1024 * 1024;  // 10MB
    char* arena = new char[ARENA_SIZE];
    size_t offset = 0;

public:
    Chunk* Allocate() {
        if (offset + sizeof(Chunk) > ARENA_SIZE) {
            // Allocate new arena
            arena = new char[ARENA_SIZE];
            offset = 0;
        }
        Chunk* result = (Chunk*)(arena + offset);
        offset += sizeof(Chunk);
        return result;
    }
};
```

**Result**: Span drops to ~8MB (matches used)

### Option 2: Pre-allocate All Chunks
```cpp
// In Archetype constructor
Chunks.reserve(500);  // Reserve space for ~100k entities
for (size_t i = 0; i < 500; i++) {
    Chunks.push_back(AllocateChunk());
}
```

**Result**: All chunks allocated at once, more contiguous

### Option 3: Use mmap/VirtualAlloc Directly
```cpp
#ifdef _WIN32
void* ptr = VirtualAlloc(nullptr, numChunks * sizeof(Chunk),
                         MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#else
void* ptr = mmap(nullptr, numChunks * sizeof(Chunk),
                 PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#endif
```

**Result**: You control the virtual address layout

## Conclusion

**Your 3MB gaps are caused by**:
1. System allocator strategy (ASLR, fragmentation prevention)
2. Interleaved allocations (STL containers, other game data)
3. Heap segment boundaries

**This is completely normal and doesn't hurt performance** because:
- CPU only touches physical pages (which ARE contiguous in cache)
- Virtual address space is abundant (128TB available)
- TLB only tracks pages you use (not the gaps)

**The larger DoD lesson**:
- Virtual memory layout ≠ Physical memory layout
- Cache and TLB work on physical addresses
- What matters: sequential access within chunks (✓ you have this)
- What doesn't matter: virtual address gaps (✓ you have these, but it's fine)

Run with Tracy and you'll see exactly where those gaps come from! 🔍
