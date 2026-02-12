#include "../Public/Registry.h"
#include "Profiler.h"
#include <cassert>

#include "EntityView.h"
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

    for (auto Arch : MR.ClassToArchetype)
    {
        Archetype*& NewArch = Archetypes[std::get<ComponentSignature>(Arch.second)];
        if (!NewArch)
        {
            NewArch = new Archetype(std::get<ComponentSignature>(Arch.second));
            NewArch->BuildLayout(std::get<std::vector<ComponentMeta>>(Arch.second));
        }
        NewArch->ResidentClassIDs.insert(Arch.first);
    }
}

void Registry::InvokeUpdate(double dt)
{
    STRIGID_ZONE_C(STRIGID_COLOR_LOGIC);

    // Iterate through all archetypes
    for (auto& [sig, archetype] : Archetypes)
    {
        MetaRegistry& MR = MetaRegistry::Get();
        
        auto meta = MR.EntityGetters[*archetype->ResidentClassIDs.begin()];
        if (!meta.Update)
            continue;
        
        // Iterate through all chunks in this archetype
        for (size_t chunkIdx = 0; chunkIdx < archetype->Chunks.size(); ++chunkIdx)
        {
            Chunk* chunk = archetype->Chunks[chunkIdx];
            uint32_t entityCount = archetype->GetChunkCount(chunkIdx);

            if (entityCount == 0)
                continue;
            
            // Build array of component array pointers for this chunk
            void* componentArrays[MAX_COMPONENTS];
            {
                STRIGID_ZONE_MEDIUM_N("Build_Component_Arrays"); // Level 2: Per-chunk profiling
                for (const auto& [compTypeID, compMeta] : archetype->ComponentLayout)
                {
                    void* arrayPtr = archetype->GetComponentArrayRaw(chunk, compTypeID);
                    componentArrays[compTypeID] = arrayPtr;
                }
            }

            alignas(16) char View[64];
            STRIGID_ZONE_MEDIUM_N("Invoke_Entity_Loop");
            for (uint32_t CompIndex = 0; CompIndex < entityCount; CompIndex++)
            {
                meta.Hydrate(componentArrays, CompIndex, View);
                meta.Update(dt, View);
            }
        }
    }
}

void Registry::InvokePrePhys(double dt)
{
    STRIGID_ZONE_C(STRIGID_COLOR_LOGIC);

    // Iterate through all archetypes
    for (auto& [sig, archetype] : Archetypes)
    {
        MetaRegistry& MR = MetaRegistry::Get();
        
        auto meta = MR.EntityGetters[*archetype->ResidentClassIDs.begin()];
        if (!meta.PrePhys)
            continue;
        
        // Iterate through all chunks in this archetype
        for (size_t chunkIdx = 0; chunkIdx < archetype->Chunks.size(); ++chunkIdx)
        {
            Chunk* chunk = archetype->Chunks[chunkIdx];
            uint32_t entityCount = archetype->GetChunkCount(chunkIdx);

            if (entityCount == 0)
                continue;
            
            // Build array of component array pointers for this chunk
            void* componentArrays[MAX_COMPONENTS];
            {
                STRIGID_ZONE_MEDIUM_N("Build_Component_Arrays"); // Level 2: Per-chunk profiling
                for (const auto& [compTypeID, compMeta] : archetype->ComponentLayout)
                {
                    void* arrayPtr = archetype->GetComponentArrayRaw(chunk, compTypeID);
                    componentArrays[compTypeID] = arrayPtr;
                }
            }

            alignas(16) char View[64];
            STRIGID_ZONE_MEDIUM_N("Invoke_Entity_Loop");
            for (uint32_t CompIndex = 0; CompIndex < entityCount; CompIndex++)
            {
                meta.Hydrate(componentArrays, CompIndex, View);
                meta.PrePhys(dt, View);
            }
        }
    }
}

void Registry::InvokePostPhys(double dt)
{
    STRIGID_ZONE_C(STRIGID_COLOR_LOGIC);

    // Iterate through all archetypes
    for (auto& [sig, archetype] : Archetypes)
    {
        MetaRegistry& MR = MetaRegistry::Get();
        
        auto meta = MR.EntityGetters[*archetype->ResidentClassIDs.begin()];
        if (!meta.PostPhys)
            continue;
        
        // Iterate through all chunks in this archetype
        for (size_t chunkIdx = 0; chunkIdx < archetype->Chunks.size(); ++chunkIdx)
        {
            Chunk* chunk = archetype->Chunks[chunkIdx];
            uint32_t entityCount = archetype->GetChunkCount(chunkIdx);

            if (entityCount == 0)
                continue;
            
            // Build array of component array pointers for this chunk
            void* componentArrays[MAX_COMPONENTS];
            {
                STRIGID_ZONE_MEDIUM_N("Build_Component_Arrays"); // Level 2: Per-chunk profiling
                for (const auto& [compTypeID, compMeta] : archetype->ComponentLayout)
                {
                    void* arrayPtr = archetype->GetComponentArrayRaw(chunk, compTypeID);
                    componentArrays[compTypeID] = arrayPtr;
                }
            }

            alignas(16) char View[64];
            STRIGID_ZONE_MEDIUM_N("Invoke_Entity_Loop");
            for (uint32_t CompIndex = 0; CompIndex < entityCount; CompIndex++)
            {
                meta.Hydrate(componentArrays, CompIndex, View);
                meta.PostPhys(dt, View);
            }
        }
    }
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
