#include "../Public/Archetype.h"
#include "Profiler.h"
#include <algorithm>
#include <cassert>

Archetype::Archetype(const Signature& Sig, const char* DebugName)
    : ArchSignature(Sig)
    , DebugName(DebugName)
    , EntitiesPerChunk(0)
    , TotalEntityCount(0)
{
}

Archetype::~Archetype()
{
    // Clean up all allocated chunks
    for (Chunk* ChunkPtr : Chunks)
    {
        // Tracy memory profiling: Track chunk deallocation with pool name
        STRIGID_FREE_N(ChunkPtr, DebugName);
        delete ChunkPtr;
    }
    Chunks.clear();
}

void Archetype::BuildLayout(const std::vector<ComponentMeta>& Components)
{
    STRIGID_ZONE_C(STRIGID_COLOR_MEMORY);
    if (Components.empty())
    {
        // Empty archetype - set a reasonable default capacity
        // (Useful for entities with only script component, no data components)
        constexpr size_t ReservedHeaderSpace = 64;
        size_t UsableSpace = Chunk::DATA_SIZE - ReservedHeaderSpace;
        EntitiesPerChunk = static_cast<uint32_t>(UsableSpace / 64); // Assume 64 bytes per entity minimum
        return;
    }

    // Calculate total stride (sum of all component sizes)
    size_t TotalStride = 0;

    for (const ComponentMeta& Meta : Components)
    {
        TotalStride += Meta.Size;
    }

    // Calculate how many entities fit in a chunk
    // Reserve some space for potential chunk header in future
    constexpr size_t ReservedHeaderSpace = 64;
    size_t UsableSpace = Chunk::DATA_SIZE - ReservedHeaderSpace;
    
    if (TotalStride > 0)
    {
        EntitiesPerChunk = static_cast<uint32_t>(UsableSpace / TotalStride);
    }
    else
    {
        EntitiesPerChunk = static_cast<uint32_t>(UsableSpace / 64); // Minimum 64 bytes per entity
    }

    // Build SoA layout: [CompA, CompA, CompA...][CompB, CompB, CompB...]
    size_t CurrentOffset = ReservedHeaderSpace;

    for (const ComponentMeta& Meta : Components)
    {
        ComponentMeta LayoutMeta = Meta;
        
        // Align offset to component's alignment requirement
        size_t Misalignment = CurrentOffset % Meta.Alignment;
        if (Misalignment != 0)
        {
            CurrentOffset += (Meta.Alignment - Misalignment);
        }

        LayoutMeta.OffsetInChunk = CurrentOffset;
        ComponentLayout[Meta.TypeID] = LayoutMeta;

        // Advance offset by (size * capacity)
        CurrentOffset += Meta.Size * EntitiesPerChunk;
    }

    // Verify we didn't overflow chunk
    assert(CurrentOffset <= Chunk::DATA_SIZE && "Component layout exceeds chunk size!");
}

uint32_t Archetype::GetChunkCount(size_t ChunkIndex) const
{
    if (Chunks.empty() || ChunkIndex >= Chunks.size() || EntitiesPerChunk == 0)
        return 0;

    // If it's the last chunk, calculate remainder
    if (ChunkIndex == Chunks.size() - 1)
    {
        uint32_t Remainder = TotalEntityCount % EntitiesPerChunk;
        // Handle case where last chunk is exactly full
        return (Remainder == 0 && TotalEntityCount > 0) ? EntitiesPerChunk : Remainder;
    }

    // All other chunks are guaranteed full (dense packing invariant)
    return EntitiesPerChunk;
}

Archetype::EntitySlot Archetype::PushEntity()
{
    STRIGID_ZONE_C(STRIGID_COLOR_MEMORY);
    // Safety check for empty archetypes
    if (EntitiesPerChunk == 0)
    {
        EntitiesPerChunk = 256; // Default fallback
    }

    // Check if we need a new chunk
    if (TotalEntityCount % EntitiesPerChunk == 0)
    {
        Chunk* NewChunk = AllocateChunk();
        Chunks.push_back(NewChunk);
    }

    // Calculate which chunk and local index
    uint32_t ChunkIndex = TotalEntityCount / EntitiesPerChunk;
    uint32_t LocalIndex = TotalEntityCount % EntitiesPerChunk;

    EntitySlot Slot;
    Slot.TargetChunk = Chunks[ChunkIndex];
    Slot.LocalIndex = LocalIndex;
    Slot.GlobalIndex = TotalEntityCount;

    TotalEntityCount++;

    return Slot;
}

void Archetype::RemoveEntity(size_t ChunkIndex, uint32_t LocalIndex)
{
    // This will be implemented with active mask in future
    // For now, just placeholder
    // TODO: Mark entity as inactive in chunk's ActiveMask
    // Actual swap-and-pop happens during compaction phase
    
    (void)ChunkIndex;
    (void)LocalIndex;
}

void* Archetype::GetComponentArrayRaw(Chunk* TargetChunk, ComponentTypeID TypeID)
{
    auto It = ComponentLayout.find(TypeID);
    if (It == ComponentLayout.end())
        return nullptr;

    const ComponentMeta& Meta = It->second;
    return TargetChunk->GetBuffer(static_cast<uint32_t>(Meta.OffsetInChunk));
}

Chunk* Archetype::AllocateChunk()
{
    STRIGID_ZONE_C(STRIGID_COLOR_MEMORY);
    Chunk* NewChunk = new Chunk();

    // Tracy memory profiling: Track chunk allocation with pool name
    // This lets you see separate pools for Transform, Velocity, etc.
    STRIGID_ALLOC_N(NewChunk, sizeof(Chunk), DebugName);

    // Debug: Track virtual memory fragmentation
    // This helps answer: "Why is 'spanned' so much larger than 'used'?"
    static void* lastChunk = nullptr;
    static void* firstChunk = nullptr;
    static uint32_t chunkCount = 0;

    if (firstChunk == nullptr)
    {
        firstChunk = NewChunk;
    }

    if (lastChunk != nullptr)
    {
        ptrdiff_t gap = (char*)NewChunk - (char*)lastChunk;
        STRIGID_PLOT("Chunk Gap (KB)", gap / 1024.0);

        // Log suspicious gaps (> 100KB means something's between chunks)
        if (gap > 100 * 1024)
        {
            char buffer[256];
            snprintf(buffer, sizeof(buffer), "Large gap detected: %lld KB between chunk %u and %u",
                     gap / 1024, chunkCount - 1, chunkCount);
            STRIGID_ZONE_TEXT(buffer, strlen(buffer));
        }
    }

    chunkCount++;

    // Track total span
    ptrdiff_t totalSpan = (char*)NewChunk - (char*)firstChunk;
    STRIGID_PLOT("Total Span (MB)", totalSpan / (1024.0 * 1024.0));
    STRIGID_PLOT("Chunk Count", (int64_t)chunkCount);
    STRIGID_PLOT("Efficiency %", (chunkCount * sizeof(Chunk) * 100.0) / (totalSpan > 0 ? totalSpan : 1));

    lastChunk = NewChunk;

    return NewChunk;
}
