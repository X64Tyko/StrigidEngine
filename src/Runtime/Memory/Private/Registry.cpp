#include "../Public/Registry.h"
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

Archetype* Registry::GetOrCreateArchetype(const Signature& Sig)
{
    STRIGID_ZONE_C(STRIGID_COLOR_MEMORY);
    
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
    
    STRIGID_PLOT("PendingDestructions", (double)PendingDestructions.size());
}

void Registry::InitializeArchetypes()
{
    MetaRegistry& MR = MetaRegistry::Get();
    // need to combine classes in the meta registry that have the same TypeID.

    for (auto Arch : MR.MetaComponents)
    {
        Archetype* NewArch = new Archetype(std::get<ComponentSignature>(Arch.second));
        NewArch->BuildLayout(std::get<std::vector<ComponentMeta>>(Arch.second));

        // Copy lifecycle functions to the archetype
        NewArch->LifecycleFunctions = std::get<std::vector<LifecycleFunction>>(Arch.second);

        Archetypes[std::get<ComponentSignature>(Arch.second)] = NewArch;
    }
}

void Registry::InvokeAll(LifecycleType type, double dt)
{
    STRIGID_ZONE_C(STRIGID_COLOR_LOGIC);

    // Iterate through all archetypes
    for (auto& [sig, archetype] : Archetypes)
    {
        // Check if this archetype has any lifecycle functions of the requested type
        bool hasFunction = false;
        for (const LifecycleFunction& func : archetype->LifecycleFunctions)
        {
            if (func.Type == type)
            {
                hasFunction = true;
                break;
            }
        }

        if (!hasFunction)
            continue;

        // Iterate through all chunks in this archetype
        for (size_t chunkIdx = 0; chunkIdx < archetype->Chunks.size(); ++chunkIdx)
        {
            Chunk* chunk = archetype->Chunks[chunkIdx];
            uint32_t entityCount = archetype->GetChunkCount(chunkIdx);

            if (entityCount == 0)
                continue;

            // Build array of component array pointers for this chunk
            std::vector<void*> componentArrays;
            {
                STRIGID_ZONE_MEDIUM_N("Build_Component_Arrays"); // Level 2: Per-chunk profiling
                for (const auto& [compTypeID, compMeta] : archetype->ComponentLayout)
                {
                    void* arrayPtr = archetype->GetComponentArrayRaw(chunk, compTypeID);
                    componentArrays.push_back(arrayPtr);
                }
            }

            // Invoke all lifecycle functions of the requested type on each entity
            for (const LifecycleFunction& func : archetype->LifecycleFunctions)
            {
                if (func.Type != type)
                    continue;

                STRIGID_ZONE_MEDIUM_N("Invoke_Entity_Loop"); // Level 2: Per-chunk profiling
                // Call the invoker for each entity in the chunk
                for (uint32_t i = 0; i < entityCount; ++i)
                {
                    func.Invoker(func.FunctionPtr, func.CachedEntity, componentArrays.data(), i, dt);
                }
            }
        }
    }
}
