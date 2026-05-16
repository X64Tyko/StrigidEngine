// skinning.slang — GPU vertex skinning compute pass.
//
// For each skeletal entity, evaluates bone transforms from the blend state
// and writes skinned vertex positions into the CVisualTransform VisPosX/Y/Z
// SoA arrays so scatter reads them as-is.
//
// M1: pass-through placeholder — SkeletalEntityCount is always 0 until
// ESkeletalEntity entities are registered. Full per-vertex skinning with
// forward kinematics is M2/M3.
//
// Dispatch shape: X = ceil(maxVertsPerEntity / 64), Y = SkeletalEntityCount.
// Each thread processes one vertex for one entity.

#include "common/GpuFrameData.slang"

struct GpuAnimEntry
{
    uint  animID[4];
    float timestamp[4];
    float weight[4];
}; // 48 bytes

struct GpuAnimBoneTrack { uint keyframeOffset; uint keyframeCount; };
struct GpuAnimKeyframe   { float time; float tx; float ty; float tz; float rx; float ry; float rz; float rw; };
struct GpuBoneData       { float inverseBindPose[16]; };
struct SkinWeights       { uint packedIndices; uint packedWeights; }; // 2 × uint8×4

[[vk::push_constant]]
cbuffer PushConstants { uint64_t gFrameDataAddr; }

[shader("compute")]
[numthreads(64, 1, 1)]
void main(uint3 tid : SV_DispatchThreadID)
{
    GpuFrameData* fd = (GpuFrameData*)gFrameDataAddr;

    uint entityIndex = tid.y;
    if (entityIndex >= fd->SkeletalEntityCount) return;

    // M1 stub: no skeletal entities registered, so this thread is never reached.
    // Full implementation in M2: read GpuAnimEntry, evaluate bone transforms,
    // compute per-vertex skinned position, write to CurrFieldAddrs[SemPosX-1..SemPosZ-1].
}
