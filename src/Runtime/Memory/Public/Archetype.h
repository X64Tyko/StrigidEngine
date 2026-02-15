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

    // Get raw pointer to component array
    void* GetComponentArrayRaw(Chunk* TargetChunk, ComponentTypeID TypeID);

    // Get field arrays for decomposed components (SoA)
    std::vector<void*> GetFieldArrays(Chunk* TargetChunk, ComponentTypeID TypeID);

    // Build the internal SoA layout from component list
    void BuildLayout(const std::vector<ComponentMeta>& Components);

    // Edge graph for archetype transitions (future optimization)
    std::unordered_map<ComponentTypeID, Archetype*> AddEdges; // Add component X -> go to archetype Y
    std::unordered_map<ComponentTypeID, Archetype*> RemoveEdges; // Remove component X -> go to archetype Y

private:
    // Allocate a new chunk
    Chunk* AllocateChunk();
};
