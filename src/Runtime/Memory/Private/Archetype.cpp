#include "../Public/Archetype.h"
#include "Profiler.h"
#include <cassert>
#include <FieldMeta.h>

Archetype::Archetype(const Signature& Sig, const char* DebugName)
    : ArchSignature(Sig)
      , DebugName(DebugName)
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

    size_t currentOffset = ReservedHeaderSpace;

    // Clear cached data
    CachedFieldArrayLayout.clear();
    FieldArrayTemplateCache.clear();
    TotalFieldArrayCount = 0;

    for (const auto& comp : Components)
    {
        ComponentTypeID typeID = comp.TypeID;

        // Check if component has pre-registered field decomposition
        const std::vector<FieldMeta>* fields =
            ComponentFieldRegistry::Get().GetFields(typeID);

        if (fields && !fields->empty())
        {
            // Component is decomposed - allocate separate field arrays
            LOG_INFO_F("Decomposing component %u into %zu field arrays",
                       typeID, fields->size());

            for (size_t fieldIdx = 0; fieldIdx < fields->size(); ++fieldIdx)
            {
                const FieldMeta& field = (*fields)[fieldIdx];

                // Align offset for this field array
                currentOffset = AlignOffset(currentOffset, field.Alignment);

                // Store offset for this field array
                FieldKey key{typeID, static_cast<uint32_t>(fieldIdx)};
                FieldOffsets[key] = currentOffset;

                // Add to cached layout
                CachedFieldArrayLayout.push_back({
                    typeID,
                    static_cast<uint32_t>(fieldIdx),
                    true
                });

                // Add to template cache
                FieldArrayTemplateCache.push_back({
                    currentOffset,
                    field.Name
                });

                LOG_TRACE_F("  Field %s[%zu]: offset=%zu, size=%zu",
                            field.Name, fieldIdx, currentOffset, field.Size);

                // Advance by EntitiesPerChunk * field size
                currentOffset += EntitiesPerChunk * field.Size;
            }

            TotalFieldArrayCount += fields->size();
        }
        else
        {
            // Non-decomposed component - store as single array
            LOG_INFO_F("Component %u stored as non-decomposed array", typeID);

            currentOffset = AlignOffset(currentOffset, comp.Alignment);

            ComponentLayout[typeID] = ComponentMeta{
                typeID,
                comp.Size,
                comp.Alignment,
                currentOffset
            };

            // Add to cached layout as single array
            CachedFieldArrayLayout.push_back({
                typeID,
                0,
                false
            });

            // Add to template cache
            FieldArrayTemplateCache.push_back({
                currentOffset,
                "non_decomposed"
            });

            currentOffset += EntitiesPerChunk * comp.Size;
            TotalFieldArrayCount += 1;
        }
    }

    TotalChunkDataSize = currentOffset;
    LOG_INFO_F("Archetype layout: %zu field arrays, %zu bytes, %u entities/chunk",
               TotalFieldArrayCount, TotalChunkDataSize, EntitiesPerChunk);

    // Validate cache consistency
    assert(CachedFieldArrayLayout.size() == TotalFieldArrayCount);
    assert(FieldArrayTemplateCache.size() == TotalFieldArrayCount);
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

std::vector<void*> Archetype::GetFieldArrays(Chunk* TargetChunk, ComponentTypeID TypeID)
{
    // Check if component is decomposed
    const std::vector<FieldMeta>* fields = ComponentFieldRegistry::Get().GetFields(TypeID);

    if (fields && !fields->empty())
    {
        // Decomposed component - return all field arrays
        std::vector<void*> fieldArrays;
        fieldArrays.reserve(fields->size());

        for (size_t fieldIdx = 0; fieldIdx < fields->size(); ++fieldIdx)
        {
            FieldKey key{TypeID, static_cast<uint32_t>(fieldIdx)};
            auto it = FieldOffsets.find(key);
            if (it != FieldOffsets.end())
            {
                fieldArrays.push_back(TargetChunk->GetBuffer(static_cast<uint32_t>(it->second)));
            }
        }

        return fieldArrays;
    }
    // Non-decomposed component - return single array
    auto It = ComponentLayout.find(TypeID);
    if (It == ComponentLayout.end())
        return {};

    const ComponentMeta& Meta = It->second;
    return {TargetChunk->GetBuffer(static_cast<uint32_t>(Meta.OffsetInChunk))};
}

Chunk* Archetype::AllocateChunk()
{
    STRIGID_ZONE_C(STRIGID_COLOR_MEMORY);
    auto NewChunk = new Chunk();

    // Tracy memory profiling: Track chunk allocation with pool name
    // This lets you see separate pools for Archetypes
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
        ptrdiff_t gap = (char*)NewChunk - static_cast<char*>(lastChunk);
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
    ptrdiff_t totalSpan = (char*)NewChunk - static_cast<char*>(firstChunk);
    STRIGID_PLOT("Total Span (MB)", totalSpan / (1024.0 * 1024.0));
    STRIGID_PLOT("Chunk Count", static_cast<int64_t>(chunkCount));
    STRIGID_PLOT("Efficiency %", (chunkCount * sizeof(Chunk) * 100.0) / (totalSpan > 0 ? totalSpan : 1));

    lastChunk = NewChunk;

    return NewChunk;
}
