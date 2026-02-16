#pragma once
#include "Types.h"
#include "Signature.h"
#include "Chunk.h"
#include <vector>
#include <unordered_map>
#include <unordered_set>

#include "Schema.h"

// Archetype - manages storage for entities with a specific component signature
// Uses Structure-of-Arrays (SoA) layout within each chunk
class Archetype
{
public:
    Archetype(const Signature& Sig, const char* DebugName = "Archetype");
    ~Archetype();

    // Component signature
    Signature ArchSignature;

    // Debug name for profiling
    const char* DebugName;

    // Resident class types (multiple classes can share same archetype)
    std::unordered_set<uint16_t> ResidentClassIDs;

    // Entity capacity and tracking
    uint32_t EntitiesPerChunk = 0; // How many entities fit in one chunk
    uint32_t TotalEntityCount = 0; // Total entities across all chunks

    // Chunk storage
    std::vector<Chunk*> Chunks;

    // Component layout information
    std::unordered_map<ComponentTypeID, ComponentMeta> ComponentLayout;

    // Cached component iteration data (built once in BuildLayout)
    struct ComponentCacheEntry
    {
        ComponentTypeID TypeID;
        bool IsFieldDecomposed;
        size_t ChunkOffset;
    };

    std::vector<ComponentCacheEntry> ComponentIterationCache;

    // Get the number of entities in a specific chunk (handles tail chunk)
    uint32_t GetChunkCount(size_t ChunkIndex) const;

    // Allocate a new entity slot (returns chunk and local index)
    struct EntitySlot
    {
        Chunk* TargetChunk;
        uint32_t LocalIndex;
        uint32_t GlobalIndex; // Index across all chunks
    };

    EntitySlot PushEntity();

    // Remove an entity (swap-and-pop, deferred via active mask)
    void RemoveEntity(size_t ChunkIndex, uint32_t LocalIndex);

    // Get typed array pointer for a component in a specific chunk
    template <typename T>
    T* GetComponentArray(Chunk* TargetChunk, ComponentTypeID TypeID)
    {
        auto It = ComponentLayout.find(TypeID);
        if (It == ComponentLayout.end())
            return nullptr;

        const ComponentMeta& Meta = It->second;
        return reinterpret_cast<T*>(TargetChunk->GetBuffer(static_cast<uint32_t>(Meta.OffsetInChunk)));
    }

    template <typename T>
    T* GetComponent(Chunk* TargetChunk, ComponentTypeID TypeID, uint32_t Index)
    {
        // TODO: need to verify this Index is valid
        return GetComponentArray<T>(TargetChunk, TypeID)[Index];
    }

    // Get field arrays for decomposed components (SoA)
    std::vector<void*> GetFieldArrays(Chunk* TargetChunk, ComponentTypeID TypeID);

    // Build the internal SoA layout from component list
    void BuildLayout(const std::vector<ComponentMeta>& Components);

    // Edge graph for archetype transitions (future optimization)
    std::unordered_map<ComponentTypeID, Archetype*> AddEdges; // Add component X -> go to archetype Y
    std::unordered_map<ComponentTypeID, Archetype*> RemoveEdges; // Remove component X -> go to archetype Y

    // Field array lookup key
    struct FieldKey
    {
        ComponentTypeID componentID;
        uint32_t fieldIndex;

        bool operator==(const FieldKey& other) const
        {
            return componentID == other.componentID && fieldIndex == other.fieldIndex;
        }
    };

    struct FieldKeyHash
    {
        size_t operator()(const FieldKey& key) const
        {
            return (static_cast<size_t>(key.componentID) << 32) | key.fieldIndex;
        }
    };

    // Storage for field array offsets
    std::unordered_map<FieldKey, size_t, FieldKeyHash> FieldOffsets;

    // CACHED FIELD ARRAY TABLE - computed once after BuildLayout()
    // This stores the field array count and layout order
    struct FieldArrayDescriptor
    {
        ComponentTypeID componentID;
        uint32_t fieldIndex;
        bool isDecomposed;
    };

    std::vector<FieldArrayDescriptor> CachedFieldArrayLayout;
    size_t TotalFieldArrayCount = 0;

    // Pre-compute field array offsets (chunk-independent)
    // Call this once after BuildLayout() to cache offsets
    struct FieldArrayTemplate
    {
        size_t offsetInChunk;
        const char* debugName; // For debugging
    };

    std::vector<FieldArrayTemplate> FieldArrayTemplateCache;

    size_t TotalChunkDataSize = 0;

    // Get pointer to a specific field array within a chunk
    void* GetFieldArray(Chunk* chunk, ComponentTypeID typeID, uint32_t fieldIndex)
    {
        FieldKey key{typeID, fieldIndex};
        auto it = FieldOffsets.find(key);
        if (it == FieldOffsets.end()) return nullptr;

        return chunk->GetBuffer(static_cast<uint32_t>(it->second));
    }

    // Build field array table using pre-computed template
    void BuildFieldArrayTable(Chunk* chunk, void** outFieldArrayTable)
    {
        auto chunkBase = chunk->Data;

        // Simple loop with cached offsets - compiler can unroll this!
        for (size_t i = 0; i < FieldArrayTemplateCache.size(); ++i)
        {
            outFieldArrayTable[i] = chunkBase + FieldArrayTemplateCache[i].offsetInChunk;
        }
    }

    // LEGACY: Build field array table with map lookups (slower, for debugging)
    void BuildFieldArrayTableSlow(Chunk* chunk, void** outFieldArrayTable)
    {
        for (size_t i = 0; i < CachedFieldArrayLayout.size(); ++i)
        {
            const auto& desc = CachedFieldArrayLayout[i];

            if (desc.isDecomposed)
            {
                // Decomposed component - get specific field array
                outFieldArrayTable[i] = GetFieldArray(chunk, desc.componentID, desc.fieldIndex);
            }
            else
            {
                // Non-decomposed component - get whole component array
                outFieldArrayTable[i] = GetComponentArrayRaw(chunk, desc.componentID);
            }
        }
    }

    // Get total field array count (for allocating table)
    size_t GetFieldArrayCount() const
    {
        return TotalFieldArrayCount;
    }

    // Validate that cached layout matches current state
    bool ValidateCache() const
    {
        return CachedFieldArrayLayout.size() == TotalFieldArrayCount &&
            FieldArrayTemplateCache.size() == TotalFieldArrayCount;
    }

    // Get component type at specific table index (for debug/validation)
    ComponentTypeID GetComponentTypeAtTableIndex(size_t tableIndex) const
    {
        if (tableIndex >= CachedFieldArrayLayout.size())
            return 0;

        return CachedFieldArrayLayout[tableIndex].componentID;
    }

    // Get field name at specific table index (for debugging)
    const char* GetFieldNameAtTableIndex(size_t tableIndex) const
    {
        if (tableIndex >= FieldArrayTemplateCache.size())
            return "invalid";

        return FieldArrayTemplateCache[tableIndex].debugName;
    }

    // Helper: Align offset to alignment requirement
    static size_t AlignOffset(size_t offset, size_t alignment)
    {
        return (offset + alignment - 1) & ~(alignment - 1);
    }

    // Legacy: Get component array for non-decomposed components
    void* GetComponentArrayRaw(Chunk* chunk, ComponentTypeID typeID)
    {
        auto it = ComponentLayout.find(typeID);
        if (it == ComponentLayout.end()) return nullptr;

        return chunk->GetBuffer(static_cast<uint32_t>(it->second.OffsetInChunk));
    }

private:
    // Allocate a new chunk
    Chunk* AllocateChunk();
};
