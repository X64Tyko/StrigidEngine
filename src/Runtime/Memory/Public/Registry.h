#pragma once
#include "Types.h"
#include "Signature.h"
#include "Archetype.h"
#include "EntityRecord.h"
#include "SoARef.h"
#include <vector>
#include <unordered_map>
#include <queue>
#include <cassert>

#include "Schema.h"

// Registry - Central entity management system
// Handles entity creation, destruction, and component access
class Registry
{
public:
    Registry();
    ~Registry();

    // Entity creation, Reflection allows this to be extremely quick
    // Usage: EntityID player = Registry::Get().Create<PlayerController>();
    template <typename T>
    EntityID Create();

    // Destroy an entity (deferred until end of frame)
    void Destroy(EntityID Id);

    // Get component from entity
    template <typename T>
    T* GetComponent(EntityID Id);

    // Get SoA component reference from entity
    template <typename T>
    SoARef<T> GetComponentSoA(EntityID Id);

    // Check if entity has component
    template <typename T>
    bool HasComponent(EntityID Id);

    // Get or create archetype for a given signature
    Archetype* GetOrCreateArchetype(const Signature& Sig);

    // Apply all pending destructions (called at end of frame)
    void ProcessDeferredDestructions();

    template <typename... Components>
    std::vector<Archetype*> Query();

    // Invoke all lifecycle functions of a specific type
    void InvokeUpdate(double dt = 0.0);
    void InvokePrePhys(double dt = 0.0);
    void InvokePostPhys(double dt = 0.0);

    // Memory diagnostics
    uint32_t GetTotalChunkCount() const;
    uint32_t GetTotalEntityCount() const;

    // Singleton access
    static Registry& Get()
    {
        static Registry Instance;
        return Instance;
    }

private:
    // Initialize archetypes with data from MetaRegistry
    void InitializeArchetypes();

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
    template <typename... Components>
    Signature BuildSignature();
};

// Template implementations must be in header

template <typename T>
EntityID Registry::Create()
{
    // Static local caching - archetype is calculated once per type T
    static Archetype* CachedArchetype = nullptr;
    static bool Initialized = false;

    if (!Initialized)
    {
        ClassID classID = T::StaticClassID();
        auto& metaComponents = MetaRegistry::Get().ClassToArchetype;

#ifdef _DEBUG // || _WITH_EDITOR
        // Runtime guard: Check if entity type was registered with STRIGID_REGISTER_ENTITY
        if (metaComponents.find(classID) == metaComponents.end())
        {
            // FATAL: Entity type not registered
            const char* typeName = typeid(T).name();
            LOG_ERROR_F("FATAL: Entity type '%s' not registered! Did you forget STRIGID_REGISTER_ENTITY(%s)?",
                        typeName, typeName);

        // In debug builds, assert. In release, fail gracefully
#ifdef _DEBUG
        assert(false && "Entity type not registered - add STRIGID_REGISTER_ENTITY macro");
#endif

        // Return invalid entity ID
        return EntityID{};
        }
#endif

        Signature Sig = std::get<ComponentSignature>(metaComponents[classID]);

        CachedArchetype = GetOrCreateArchetype(Sig);
        Initialized = true;
    }

    // Allocate entity ID
    EntityID Id = AllocateEntityID(T::StaticClassID());

    // Allocate slot in archetype
    Archetype::EntitySlot Slot = CachedArchetype->PushEntity();

    // Update EntityIndex
    uint32_t Index = Id.GetIndex();
    if (Index >= EntityIndex.size())
    {
        EntityIndex.resize(Index * 2);
    }

    EntityRecord& Record = EntityIndex[Index];
    Record.Arch = CachedArchetype;
    Record.TargetChunk = Slot.TargetChunk;
    Record.Index = static_cast<uint16_t>(Slot.LocalIndex);
    Record.Generation = Id.GetGeneration();

    return Id;
}

template <typename T>
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

template <typename T>
SoARef<T> Registry::GetComponentSoA(EntityID Id)
{
    SoARef<T> ref;

    if (!Id.IsValid())
        return ref;

    uint32_t Index = Id.GetIndex();
    if (Index >= EntityIndex.size())
        return ref;

    EntityRecord& Record = EntityIndex[Index];

    // Validate generation (detect use-after-free)
    if (Record.Generation != Id.GetGeneration())
        return ref;

    if (!Record.IsValid())
        return ref;

    ComponentTypeID TypeID = GetComponentTypeID<T>();

    // Get field arrays from archetype
    std::vector<void*> fieldArrays = Record.Arch->GetFieldArrays(Record.TargetChunk, TypeID);
    if (fieldArrays.empty())
        return ref;

    // Bind SoARef to field arrays
    ref.Bind(fieldArrays.data(), Record.Index);
    return ref;
}

template <typename T>
bool Registry::HasComponent(EntityID Id)
{
    return GetComponent<T>(Id) != nullptr;
}

template <typename... Components>
Signature Registry::BuildSignature()
{
    Signature Sig;
    // Fold expression to set all component bits
    ((Sig.Set(GetComponentTypeID<Components>() - 1)), ...);
    return Sig;
}

template <typename... Components>
std::vector<Archetype*> Registry::Query()
{
    std::vector<Archetype*> Results;
    Signature Sig = BuildSignature<Components...>();
    for (auto Arch : Archetypes)
    {
        if (Arch.first.Contains(Sig))
        {
            Results.push_back(Arch.second);
        }
    }

    return Results;
}
