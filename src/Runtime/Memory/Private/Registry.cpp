#include "Registry.h"
#include "Profiler.h"
#include <cassert>

#include "SchemaReflector.h"

Registry::Registry()
    : NextEntityIndex(1) // Start at 1 (0 is reserved for Invalid)
{
    STRIGID_ZONE_N("Registry::Constructor");
    // Reserve space for entity index
    EntityIndex.reserve(1024);
    InitializeArchetypes();
}

Registry::Registry(const EngineConfig* Config)
    : Registry()
{
    HistorySlab.Initialize(Config);
    Registry();
}

Registry::~Registry()
{
    STRIGID_ZONE_N("Registry::Destructor");
    // Clean up all archetypes
    for (auto& Pair : Archetypes)
    {
        delete Pair.second;
    }
    Archetypes.clear();
}

Archetype* Registry::GetOrCreateArchetype(const Signature& Sig, const ClassID& ID)
{
    STRIGID_ZONE_C(STRIGID_COLOR_MEMORY);
    auto key = Archetype::ArchetypeKey(Sig, ID);

    // Check if archetype already exists
    auto It = Archetypes.find(key);
    if (It != Archetypes.end())
    {
        return It->second;
    }

    // Create new archetype
    auto NewArchetype = new Archetype(Sig, ID);

    // TODO: In Week 5, we'll build component layout from signature
    // For now, create empty archetype
    std::vector<ComponentMeta> Components;
    NewArchetype->BuildLayout(Components);

    Archetypes[key] = NewArchetype;
    return NewArchetype;
}

EntityID Registry::AllocateEntityID(uint16_t TypeID)
{
    STRIGID_ZONE_C(STRIGID_COLOR_MEMORY);

    EntityID Id;
    Id.Value = 0;

    // Try to reuse a free index
    if (!FreeIndices.empty())
    {
        uint32_t Index = FreeIndices.front();
        FreeIndices.pop();

        // Increment generation for recycled index
        uint16_t Generation = EntityIndex[Index].Generation + 1;
        if (Generation == 0) // Wrapped around
            Generation = 1; // Skip 0 (reserved for invalid)

        Id.Index = Index;
        Id.Generation = Generation;
        Id.TypeID = TypeID;
        Id.OwnerID = 0; // Server-owned by default
    }
    else
    {
        // Allocate new index
        Id.Index = NextEntityIndex++;
        Id.Generation = 1; // First generation
        Id.TypeID = TypeID;
        Id.OwnerID = 0;
    }

    return Id;
}

void Registry::FreeEntityID(EntityID Id)
{
    STRIGID_ZONE_C(STRIGID_COLOR_MEMORY);

    uint32_t Index = Id.GetIndex();
    if (Index >= EntityIndex.size())
        return;

    // Add to free list
    FreeIndices.push(Index);

    // Invalidate record
    EntityIndex[Index].Arch = nullptr;
    EntityIndex[Index].TargetChunk = nullptr;
}

void Registry::Destroy(EntityID Id)
{
    STRIGID_ZONE_C(STRIGID_COLOR_MEMORY);
    // Defer destruction until end of frame
    PendingDestructions.push_back(Id);
}

void Registry::ProcessDeferredDestructions()
{
    STRIGID_ZONE_C(STRIGID_COLOR_MEMORY);

    for (EntityID Id : PendingDestructions)
    {
        if (!Id.IsValid())
            continue;

        uint32_t Index = Id.GetIndex();
        if (Index >= EntityIndex.size())
            continue;

        EntityRecord& Record = EntityIndex[Index];

        // Validate generation
        if (Record.Generation != Id.GetGeneration())
            continue;

        if (!Record.IsValid())
            continue;

        // TODO: Week 12 - Mark entity as inactive in chunk's ActiveMask
        // For now, just invalidate the record

        // Remove from archetype (will be implemented with swap-and-pop in Week 12)
        // Record.Arch->RemoveEntity(chunkIndex, Record.Index);

        // Free the entity ID
        FreeEntityID(Id);
    }

    PendingDestructions.clear();

    STRIGID_PLOT("PendingDestructions", static_cast<double>(PendingDestructions.size()));
}

void Registry::InitializeArchetypes()
{
    MetaRegistry& MR = MetaRegistry::Get();
    // need to combine classes in the meta registry that have the same TypeID.

    for (auto Arch : MR.ClassToArchetype)
    {
        auto key = Archetype::ArchetypeKey(std::get<ComponentSignature>(Arch.second), Arch.first);
        Archetype*& NewArch = Archetypes[key];
        if (!NewArch)
        {
            NewArch = new Archetype(key);
            NewArch->BuildLayout(std::get<std::vector<ComponentMeta>>(Arch.second));
        }
    }
}

void Registry::ResetRegistry()
{
    for (auto& Entity : EntityIndex)
    {
        Entity.Arch = nullptr;
    }
    EntityIndex.clear();
    while (!FreeIndices.empty())
    {
        FreeIndices.pop();
    }
    PendingDestructions.clear();
    NextEntityIndex = 1;
}

uint32_t Registry::GetTotalChunkCount() const
{
    uint32_t totalChunks = 0;
    for (const auto& [sig, archetype] : Archetypes)
    {
        totalChunks += static_cast<uint32_t>(archetype->Chunks.size());
    }
    return totalChunks;
}

uint32_t Registry::GetTotalEntityCount() const
{
    uint32_t totalEntities = 0;
    for (const auto& [sig, archetype] : Archetypes)
    {
        totalEntities += archetype->TotalEntityCount;
    }
    return totalEntities;
}
