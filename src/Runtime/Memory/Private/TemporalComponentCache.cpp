#include "TemporalComponentCache.h"

#include "EngineConfig.h"
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
    MetaRegistry MR = MetaRegistry::Get();
    size_t MetaSize = 0;

    for (auto& meta : MR.ReflectedComponents)
    {
        if (meta.IsHot)
        {
            MetaSize += meta.Size;
        }
    }

    // malloc for now, will want something else later
    SlabPtr = static_cast<uint8_t*>(malloc(MetaSize * Config->MaxDynamicEntities * Config->HistoryBufferPages));

    LOG_INFO_F("Initialized TemporalComponentCache with %zi bytes", MetaSize * Config->MaxDynamicEntities * Config->HistoryBufferPages);
}
