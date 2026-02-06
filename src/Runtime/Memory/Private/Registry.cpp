#include "../Public/Registry.h"
#include <cassert>

Registry::Registry()
    : NextEntityIndex(1) // Start at 1 (0 is reserved for Invalid)
{
    // Reserve space for entity index
    EntityIndex.reserve(1024);
}

Registry::~Registry()
{
    // Clean up all archetypes
    for (auto& Pair : Archetypes)
    {
        delete Pair.second;
    }
    Archetypes.clear();
}

Archetype* Registry::GetOrCreateArchetype(const Signature& Sig)
{
    // Check if archetype already exists
    auto It = Archetypes.find(Sig);
    if (It != Archetypes.end())
    {
        return It->second;
    }

    // Create new archetype
    Archetype* NewArchetype = new Archetype(Sig);
    
    // TODO: In Week 5, we'll build component layout from signature
    // For now, create empty archetype
    std::vector<ComponentMeta> Components;
    NewArchetype->BuildLayout(Components);

    Archetypes[Sig] = NewArchetype;
    return NewArchetype;
}

EntityID Registry::AllocateEntityID(uint16_t TypeID)
{
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
    // Defer destruction until end of frame
    PendingDestructions.push_back(Id);
}

void Registry::ProcessDeferredDestructions()
{
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
}
