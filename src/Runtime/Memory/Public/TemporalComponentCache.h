#pragma once

struct EngineConfig;

class TemporalComponentCache
{
public:
    TemporalComponentCache();
    ~TemporalComponentCache();

    void Initialize(const EngineConfig* Config);

private:
    // for Hot components we're allocating one large slab for the history of components.
    void* SlabPtr = nullptr;
};
