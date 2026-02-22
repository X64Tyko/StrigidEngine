#include "TemporalComponentCache.h"

#include "EngineConfig.h"
#include "FieldMeta.h"
#include "Schema.h"
#include "Types.h"

TemporalComponentCache::TemporalComponentCache()
{
}

TemporalComponentCache::~TemporalComponentCache()
{
    free(SlabPtr);
}

void TemporalComponentCache::Initialize(const EngineConfig* Config)
{
    ComponentFieldRegistry& CFR = ComponentFieldRegistry::Get();
    size_t MetaSize = 0;

    for (auto& meta : CFR.GetAllComponents())
    {
        if (meta.second.IsHot)
        {
            MetaSize += meta.second.Size;
        }
    }
    
    size_t HeaderSize = sizeof(HistorySectionHeader) * Config->HistoryBufferPages;
    size_t HistorySize = MetaSize * Config->MaxDynamicEntities * Config->HistoryBufferPages;
    size_t slabSize = HistorySize + HeaderSize;

    // malloc for now, will want something else later
    SlabPtr = static_cast<uint8_t*>(malloc(slabSize));

    LOG_INFO_F("Initialized TemporalComponentCache with %zi bytes", slabSize);
}
