// skinning.slang — GPU vertex skinning compute pass (M2).
//
// One thread per skeletal entity. Evaluates FK bone chain from animation
// keyframes and writes SkinMatrix[compactIdx × MAX_BONES_PER_ENTITY + b]
// for each bone. The vertex shader reads these matrices for LBS.
//
// Dispatch: vkCmdDispatchIndirect(SkeletalDispatchArgsAddr), numthreads(1,1,1).
// x=skeletalCount is GPU-written by scatter; dispatch runs after sort_instances.

#include "common/GpuFrameData.slang"

struct GpuAnimBoneTrack { uint keyframeOffset; uint keyframeCount; };
struct GpuAnimKeyframe   { float time; float tx, ty, tz, rx, ry, rz, rw; };
struct GpuBoneData       { float inverseBindPose[16]; }; // column-major float4x4

[[vk::push_constant]]
cbuffer PushConstants { uint64_t gFrameDataAddr; }

// -----------------------------------------------------------------------
// Quaternion + matrix helpers
// -----------------------------------------------------------------------

float4x4 quatToMat(float4 q)
{
    float x = q.x, y = q.y, z = q.z, w = q.w;
    float x2 = x+x, y2 = y+y, z2 = z+z;
    float xx = x*x2, xy = x*y2, xz = x*z2;
    float yy = y*y2, yz = y*z2, zz = z*z2;
    float wx = w*x2, wy = w*y2, wz = w*z2;
    float4x4 m;
    m[0] = float4(1-(yy+zz), xy+wz,    xz-wy,    0);
    m[1] = float4(xy-wz,     1-(xx+zz), yz+wx,   0);
    m[2] = float4(xz+wy,     yz-wx,    1-(xx+yy), 0);
    m[3] = float4(0,         0,         0,         1);
    return m;
}

float4x4 makeTRS(float3 t, float4 q, float3 s)
{
    float4x4 r = quatToMat(normalize(q));
    float4x4 m;
    m[0] = float4(r[0].x * s.x, r[0].y * s.y, r[0].z * s.z, t.x);
    m[1] = float4(r[1].x * s.x, r[1].y * s.y, r[1].z * s.z, t.y);
    m[2] = float4(r[2].x * s.x, r[2].y * s.y, r[2].z * s.z, t.z);
    m[3] = float4(0, 0, 0, 1);
    return m;
}

float4x4 loadMat(float* data)
{
    // Column-major: M[row][col] = data[col*4 + row]
    float4x4 m;
    m[0] = float4(data[0], data[4], data[8],  data[12]);
    m[1] = float4(data[1], data[5], data[9],  data[13]);
    m[2] = float4(data[2], data[6], data[10], data[14]);
    m[3] = float4(data[3], data[7], data[11], data[15]);
    return m;
}

// -----------------------------------------------------------------------
// Keyframe evaluation — finds the two surrounding keyframes and lerps.
// -----------------------------------------------------------------------

void evalBoneAtTime(GpuAnimBoneTrack track, GpuAnimKeyframe* kfBuf, float t,
                    out float3 outT, out float4 outQ)
{
    if (track.keyframeCount == 0)
    {
        outT = float3(0, 0, 0);
        outQ = float4(0, 0, 0, 1);
        return;
    }

    uint first = track.keyframeOffset;
    uint last  = first + track.keyframeCount - 1u;

    // Clamp to range
    if (t <= kfBuf[first].time)
    {
        GpuAnimKeyframe k = kfBuf[first];
        outT = float3(k.tx, k.ty, k.tz);
        outQ = float4(k.rx, k.ry, k.rz, k.rw);
        return;
    }
    if (t >= kfBuf[last].time)
    {
        GpuAnimKeyframe k = kfBuf[last];
        outT = float3(k.tx, k.ty, k.tz);
        outQ = float4(k.rx, k.ry, k.rz, k.rw);
        return;
    }

    // Linear search — sufficient for M2; binary search in M3.
    uint lo = first;
    for (uint i = first; i < last; ++i)
    {
        if (kfBuf[i+1].time > t) { lo = i; break; }
    }

    GpuAnimKeyframe a = kfBuf[lo];
    GpuAnimKeyframe b = kfBuf[lo + 1u];
    float span = b.time - a.time;
    float alpha = (span > 0.0) ? (t - a.time) / span : 0.0;

    outT = lerp(float3(a.tx, a.ty, a.tz), float3(b.tx, b.ty, b.tz), alpha);

    float4 qa = float4(a.rx, a.ry, a.rz, a.rw);
    float4 qb = float4(b.rx, b.ry, b.rz, b.rw);
    if (dot(qa, qb) < 0.0) qb = -qb;
    outQ = normalize(lerp(qa, qb, alpha));
}

// -----------------------------------------------------------------------
// Main
// -----------------------------------------------------------------------

[shader("compute")]
[numthreads(1, 1, 1)]
void main(uint3 tid : SV_DispatchThreadID)
{
    GpuFrameData* fd = (GpuFrameData*)gFrameDataAddr;

    uint compactIdx = tid.x;
    // Bounds are enforced by vkCmdDispatchIndirect — x = scatter-written skeletalCount.

    // Resolve entity cache index from the GPU-written compact list.
    uint cacheIdx = ((uint*)fd->SkeletalListAddr)[compactIdx];

    // Read SkeletonID and anim state from slab (CurrFieldAddrs[SlabIdx_XXX]).
    uint  skelID    = ((uint*)fd->CurrFieldAddrs[SlabIdx_SkeletonID])[cacheIdx];
    uint  animID    = ((uint*)fd->CurrFieldAddrs[SlabIdx_BaseAnimID])[cacheIdx];
    float timestamp = ((float*)fd->CurrFieldAddrs[SlabIdx_BaseTimestamp])[cacheIdx];

    if (skelID == 0 || animID == 0) return;

    // Skeleton metadata is a flat global array; skelID is a slot index.
    // AnimID is also a slot index into the global track/keyframe arrays.
    // We find per-skeleton boneOffset by reading the slab-adjacent SkeletonID
    // value as a slot index — SkeletonManager::GetSlot(skelID).boneOffset.
    // For M2, the boneOffset must be communicated via an additional slab field
    // or a dedicated GPU buffer. Since we don't have a per-skeleton metadata
    // GPU buffer yet, we read it from the SkeletonID assuming slot==boneOffset
    // is not true. Instead, we treat the bone tracks as absolute (they already
    // store absolute keyframeOffset from AnimationManager::CommitToSlot).
    // boneOffset: we embed it by convention — slot 0 = boneOffset 0, etc.
    // This works when MAX_SKELETON_SLOTS is small and slots pack sequentially.
    // A GpuSkeletonSlot buffer is the clean M3 fix.
    //
    // For now: boneCount and boneOffset are encoded from the anim track range.
    // The anim's boneCount tracks tell us how many bones to evaluate.

    GpuAnimBoneTrack* tracks = (GpuAnimBoneTrack*)fd->AnimTrackAddr;
    GpuAnimKeyframe*  kfs    = (GpuAnimKeyframe*)fd->AnimKeyframeAddr;
    GpuBoneData*      bones  = (GpuBoneData*)fd->GpuBoneDataAddr;
    uint*             parents = (uint*)fd->GpuBoneParentAddr;
    float4x4*         outMats = (float4x4*)fd->SkinMatrixAddr;

    // Read entity world TRS from slab for world-space skin matrices.
    float posX  = ((float*)fd->CurrFieldAddrs[SemPosX  - 1u])[cacheIdx];
    float posY  = ((float*)fd->CurrFieldAddrs[SemPosY  - 1u])[cacheIdx];
    float posZ  = ((float*)fd->CurrFieldAddrs[SemPosZ  - 1u])[cacheIdx];
    float rotQx = ((float*)fd->CurrFieldAddrs[SemRotQx - 1u])[cacheIdx];
    float rotQy = ((float*)fd->CurrFieldAddrs[SemRotQy - 1u])[cacheIdx];
    float rotQz = ((float*)fd->CurrFieldAddrs[SemRotQz - 1u])[cacheIdx];
    float rotQw = ((float*)fd->CurrFieldAddrs[SemRotQw - 1u])[cacheIdx];
    float scaX  = ((float*)fd->CurrFieldAddrs[SemScaleX - 1u])[cacheIdx];
    float scaY  = ((float*)fd->CurrFieldAddrs[SemScaleY - 1u])[cacheIdx];
    float scaZ  = ((float*)fd->CurrFieldAddrs[SemScaleZ - 1u])[cacheIdx];

    float4x4 entityWorld = makeTRS(float3(posX, posY, posZ),
                                   float4(rotQx, rotQy, rotQz, rotQw),
                                   float3(scaX, scaY, scaZ));

    // Determine boneCount from animID. animID-1 is the 0-based slot.
    // Tracks are contiguous: slot N has boneCount consecutive track entries
    // starting at Slots[N].trackOffset. Since we don't have a GPU slot table
    // for the animation yet, infer boneCount from the parent buffer:
    // walk until we find a bone whose parent wraps back or is UINT32_MAX.
    // For M2, use a reasonable cap (MAX_BONES_PER_ENTITY) and break early.
    uint boneOffset = 0; // M2: assume skeleton slot packs bones at offset 0 per skelID
    // TODO M3: add GpuSkeletonSlotBuffer {boneOffset, boneCount} indexed by skelID
    // and GpuAnimSlotBuffer {trackOffset, boneCount} indexed by animID.

    // Per-bone local world transforms (FK chain). Stored temporarily as float4x4.
    // We compute them in parent-before-child order (bones are sorted in the asset).
    float4x4 worldBone[MAX_BONES_PER_ENTITY];

    for (uint b = 0; b < MAX_BONES_PER_ENTITY; ++b)
    {
        uint globalBone = boneOffset + b;

        // Check parent — UINT32_MAX means root.
        uint parentIdx = parents[globalBone];

        // Evaluate this bone's local transform from the animation.
        GpuAnimBoneTrack track = tracks[(animID - 1u) * MAX_BONES_PER_ENTITY + b];
        float3 localT; float4 localQ;
        evalBoneAtTime(track, kfs, timestamp, localT, localQ);
        float4x4 localMat = makeTRS(localT, localQ, float3(1, 1, 1));

        if (parentIdx == 0xFFFFFFFFu || parentIdx >= MAX_BONES_PER_ENTITY)
        {
            // Root bone: local space is entity space; apply entity world transform.
            worldBone[b] = mul(entityWorld, localMat);
        }
        else
        {
            worldBone[b] = mul(worldBone[parentIdx], localMat);
        }

        // SkinMatrix = worldBone × inverseBindPose
        float4x4 ibp = loadMat(&bones[globalBone].inverseBindPose[0]);
        outMats[compactIdx * MAX_BONES_PER_ENTITY + b] = mul(worldBone[b], ibp);
    }
}