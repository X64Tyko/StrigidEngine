#pragma once
#include <atomic>
#include <cstdint>

#include "Types.h"

struct EngineConfig;

struct alignas(16) HistorySectionHeader
{
    // Ownership tracking (atomic bitfield)
    std::atomic<uint8_t> OwnershipFlags;
    /*
        0x01 = LOGIC_WRITING
        0x02 = RENDER_READING
        0x04 = NETWORK_READING
        0x08 = DEFRAG_LOCKED
        Multiple readers can coexist (bitwise OR)
    */
    uint8_t _pad;

    // Frame identification
    uint32_t FrameNumber;

    // Camera/View data (replaces FramePacket)
    Matrix4 ViewMatrix;
    Matrix4 ProjectionMatrix;
    Vector3 CameraPosition;

    // Scene/Lighting data
    Vector3 SunDirection;
    Vector3 SunColor;
    float AmbientIntensity;

    // Entity metadata
    uint32_t ActiveEntityCount;
    uint32_t TotalAllocatedEntities;

    // Padding to cache line
    char _padding[ 64 - (sizeof(std::atomic<uint8_t>) + sizeof(uint8_t) + sizeof(uint32_t) * 3 + sizeof(Vector3) * 3 + sizeof(Matrix4) * 2 + sizeof(float)) % 64];
};

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
