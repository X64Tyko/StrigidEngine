#pragma once
#include <cstdint>
#include "VulkanMemory.h"
#include "VulkanInclude.h"

// -----------------------------------------------------------------------
// GpuAnimEntry — per-entity animation state uploaded to GPU each frame.
// One entry per entity in the skeletal partition.
//
// Base layer: explicit cross-fade or blendspace.
//   baseAnimID  > 0 → explicit clip at baseTimestamp
//   baseAnimID == 0 → blendspace (blendspaceID) evaluated at (blendCoordX, blendCoordY)
//   fadeAnimID  > 0 → outgoing clip at fadeTimestamp, lerped by fadeAlpha
//
// Overlay layers: up to 2 masked replace/additive slots.
//   layerConfig bits 0-7 = skeleton named layer ID (bone mask index in SkeletonAsset)
//   layerConfig bit  8   = 1 → additive, 0 → replace
// -----------------------------------------------------------------------
struct GpuAnimEntry
{
    // Base layer
    uint32_t baseAnimID;
    uint32_t blendspaceID;
    float    baseTimestamp;
    float    blendCoordX;
    float    blendCoordY;
    uint32_t fadeAnimID;
    float    fadeTimestamp;
    float    fadeAlpha;

    // Overlay layer 0
    uint32_t layer0AnimID;
    float    layer0Timestamp;
    float    layer0Alpha;
    uint32_t layer0Config;

    // Overlay layer 1
    uint32_t layer1AnimID;
    float    layer1Timestamp;
    float    layer1Alpha;
    uint32_t layer1Config;
}; // 16 fields × 4 bytes = 64 bytes

static_assert(sizeof(GpuAnimEntry) == 64, "GpuAnimEntry must be 64 bytes");

// -----------------------------------------------------------------------
// SkinningPass — GPU vertex skinning infrastructure.
//
// Owns the per-frame GpuAnimBuffer (CAnimBase + CAnimLayer SoA packed per entity).
// Dispatched before the predicate pass; outputs skinned vertex positions consumed
// by the scatter pass.
//
// M1: Dispatch is a no-op when SkeletalEntityCount == 0.
// M2/M3: full per-vertex skinning with forward kinematics + blendspace eval.
// -----------------------------------------------------------------------

class SkinningPass
{
public:
    bool Initialize(VkDevice device, VkPipelineLayout pipelineLayout,
                    VulkanMemory* vkMem, uint32_t maxEntities);

    void Destroy(VkDevice device);

    // Upload CAnimBase + CAnimLayer state for all active skeletal entities.
    // All float params are raw floats — caller converts SimFloat fields via .ToFloat().
    void UploadAnimData(const uint32_t* skeletalEntities, uint32_t count,
                        // Base layer
                        const uint32_t* baseAnimID,   const uint32_t* blendspaceID,
                        const float*    baseTimestamp, const float*    blendCoordX,
                        const float*    blendCoordY,
                        const uint32_t* fadeAnimID,   const float*    fadeTimestamp,
                        const float*    fadeAlpha,
                        // Overlay layers
                        const uint32_t* layer0AnimID, const float*    layer0Timestamp,
                        const float*    layer0Alpha,  const uint32_t* layer0Config,
                        const uint32_t* layer1AnimID, const float*    layer1Timestamp,
                        const float*    layer1Alpha,  const uint32_t* layer1Config);

    void Dispatch(VkCommandBuffer cmd, uint64_t fdAddr, uint32_t skeletalEntityCount);

    uint64_t GetAnimBlendAddr() const { return AnimBlendBuffer.DeviceAddr; }
    bool     IsValid()          const { return Pipeline != VK_NULL_HANDLE; }

private:
    VulkanBuffer  AnimBlendBuffer;
    VkPipeline    Pipeline    = VK_NULL_HANDLE;
    VkPipelineLayout PipeLayout = VK_NULL_HANDLE;
};