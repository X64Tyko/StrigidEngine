#pragma once
#include "Types.h"
#include "Signature.h"
#include "Chunk.h"
#include <vector>
#include <unordered_map>
#include <unordered_set>

// Component metadata - describes how a component is laid out in memory
struct ComponentMeta
{
    ComponentTypeID TypeID;
    size_t Size;           // sizeof(Component)
    size_t Alignment;      // alignof(Component)
    size_t OffsetInChunk;  // Where this component's array starts in the chunk
};

// Archetype - manages storage for entities with a specific component signature
// Uses Structure-of-Arrays (SoA) layout within each chunk
class Archetype
{
public:
    Archetype(const Signature& Sig);
    ~Archetype();

    // Component signature
    Signature ComponentSignature;

    // Resident class types (multiple classes can share same archetype)
    std::unordered_set<uint16_t> ResidentClassIDs;

    // Entity capacity and tracking
    uint32_t EntitiesPerChunk = 0;  // How many entities fit in one chunk
    uint32_t TotalEntityCount = 0;  // Total entities across all chunks

    // Chunk storage
    std::vector<Chunk*> Chunks;

    // Component layout information
    std::unordered_map<ComponentTypeID, ComponentMeta> ComponentLayout;

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
    template<typename T>
    T* GetComponentArray(Chunk* TargetChunk, ComponentTypeID TypeID)
    {
        auto It = ComponentLayout.find(TypeID);
        if (It == ComponentLayout.end())
            return nullptr;

        const ComponentMeta& Meta = It->second;
        return reinterpret_cast<T*>(TargetChunk->GetBuffer(static_cast<uint32_t>(Meta.OffsetInChunk)));
    }

    // Get raw pointer to component array
    void* GetComponentArrayRaw(Chunk* TargetChunk, ComponentTypeID TypeID);

    // Build the internal SoA layout from component list
    void BuildLayout(const std::vector<ComponentMeta>& Components);

    // Edge graph for archetype transitions (future optimization)
    std::unordered_map<ComponentTypeID, Archetype*> AddEdges;    // Add component X -> go to archetype Y
    std::unordered_map<ComponentTypeID, Archetype*> RemoveEdges; // Remove component X -> go to archetype Y

private:
    // Allocate a new chunk
    Chunk* AllocateChunk();
};
