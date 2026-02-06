#pragma once
#include "Types.h"
#include "Signature.h"
#include "Archetype.h"
#include "EntityRecord.h"
#include <vector>
#include <unordered_map>
#include <queue>

// Registry - Central entity management system
// Handles entity creation, destruction, and component access
class Registry
{
public:
    Registry();
    ~Registry();

    // Entity creation with lazy archetype caching
    // Usage: EntityID player = Registry::Get().Create<PlayerController>();
    template<typename T>
    EntityID Create();

    // Destroy an entity (deferred until end of frame)
    void Destroy(EntityID Id);

    // Get component from entity
    template<typename T>
    T* GetComponent(EntityID Id);

    // Check if entity has component
    template<typename T>
    bool HasComponent(EntityID Id);

    // Get or create archetype for a given signature
    Archetype* GetOrCreateArchetype(const Signature& Sig);

    // Apply all pending destructions (called at end of frame)
    void ProcessDeferredDestructions();

    // Singleton access
    static Registry& Get()
    {
        static Registry Instance;
        return Instance;
    }

private:
    // Global entity lookup table (indexed by EntityID.GetIndex())
    std::vector<EntityRecord> EntityIndex;

    // Free list for recycled entity indices
    std::queue<uint32_t> FreeIndices;

    // Next entity index to allocate (if free list is empty)
    uint32_t NextEntityIndex = 0;

    // Archetype storage (signature → archetype)
    std::unordered_map<Signature, Archetype*> Archetypes;

    // Pending destructions (processed at end of frame)
    std::vector<EntityID> PendingDestructions;

    // Allocate a new EntityID
    EntityID AllocateEntityID(uint16_t TypeID);

    // Free an EntityID (returns index to free list)
    void FreeEntityID(EntityID Id);

    // Helper: Build signature from component list
    template<typename... Components>
    Signature BuildSignature();

    // Helper: Get component type ID (will be defined via reflection later)
    template<typename T>
    static ComponentTypeID GetComponentTypeID();
};

// Template implementations must be in header

template<typename T>
EntityID Registry::Create()
{
    // Static local caching - archetype is calculated once per type T
    static Archetype* CachedArchetype = nullptr;
    static bool Initialized = false;

    if (!Initialized)
    {
        // TODO: In Week 5 (Reflection), we'll scan T for Ref<Component> members
        // For now, create empty archetype as placeholder
        Signature Sig;
        // Sig.Set(GetComponentTypeID<Transform>()); // Example
        
        CachedArchetype = GetOrCreateArchetype(Sig);
        Initialized = true;
    }

    // Allocate entity ID
    // TODO: TypeID will come from reflection system (Week 5)
    constexpr uint16_t TypeID = 0; // Placeholder
    EntityID Id = AllocateEntityID(TypeID);

    // Allocate slot in archetype
    Archetype::EntitySlot Slot = CachedArchetype->PushEntity();

    // Update EntityIndex
    uint32_t Index = Id.GetIndex();
    if (Index >= EntityIndex.size())
    {
        EntityIndex.resize(Index + 1);
    }

    EntityRecord& Record = EntityIndex[Index];
    Record.Arch = CachedArchetype;
    Record.TargetChunk = Slot.TargetChunk;
    Record.Index = static_cast<uint16_t>(Slot.LocalIndex);
    Record.Generation = Id.GetGeneration();

    // TODO: Placement new for T instance (Week 6)
    // TODO: Auto-wire Ref<Component> members (Week 6)

    return Id;
}

template<typename T>
T* Registry::GetComponent(EntityID Id)
{
    if (!Id.IsValid())
        return nullptr;

    uint32_t Index = Id.GetIndex();
    if (Index >= EntityIndex.size())
        return nullptr;

    EntityRecord& Record = EntityIndex[Index];

    // Validate generation (detect use-after-free)
    if (Record.Generation != Id.GetGeneration())
        return nullptr;

    if (!Record.IsValid())
        return nullptr;

    // TODO: Get ComponentTypeID from reflection (Week 5)
    ComponentTypeID TypeID = GetComponentTypeID<T>();

    // Get component array from archetype
    T* ComponentArray = Record.Arch->GetComponentArray<T>(Record.TargetChunk, TypeID);
    if (!ComponentArray)
        return nullptr;

    // Return pointer to this entity's component
    return &ComponentArray[Record.Index];
}

template<typename T>
bool Registry::HasComponent(EntityID Id)
{
    return GetComponent<T>(Id) != nullptr;
}

template<typename T>
ComponentTypeID Registry::GetComponentTypeID()
{
    // TODO: This will be implemented in Week 5 (Reflection)
    // For now, use compile-time hash of type name
    static ComponentTypeID CachedID = static_cast<ComponentTypeID>(typeid(T).hash_code() & 0xFF);
    return CachedID;
}

template<typename... Components>
Signature Registry::BuildSignature()
{
    Signature Sig;
    // Fold expression to set all component bits
    ((Sig.Set(GetComponentTypeID<Components>())), ...);
    return Sig;
}
