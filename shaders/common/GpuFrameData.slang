// GpuFrameData.slang — shared GPU frame data struct.
// Included by all compute and graphics shaders via #include "common/GpuFrameData.slang".
//
// Layout mirrors C++ GpuFrameData in GpuFrameData.h exactly.
// Accessed via a single uint64_t push constant (buffer device address).

#pragma once

static const uint MaxGpuFields       = 128;
static const uint MaxMeshSlots       = 256;
static const uint GpuOutFieldCount   = 16; // Flags + PosXYZ + RotQxyzw + ScaleXYZ + ColorRGBA + MeshID
static const uint GpuAnimFieldCount  = 19; // SkeletonID + SkinMeshID + CAnimBase(10-StateNodeID) + CAnimLayer(8)
static const uint GpuTotalFieldCount = 35; // GpuOutFieldCount + GpuAnimFieldCount

// GpuFieldSemantic constants (match C++ GpuFrameData.h).
// Render output fields (sems 1-16): instance SoA slot = sem - 1.
// SemEntityCacheIdx (sem 17): always-on, written by scatter from entity index.
// Animation slab fields (sems 18-36): slab only, scatter skips (sem > GpuOutFieldCount).
//   Slab CurrFieldAddrs index = sem - 2  (anim sems start at 18, slab indices at 16).
static const uint SemGeneric = 0;
static const uint SemFlags   = 1;
static const uint SemPosX    = 2;  static const uint SemPosY    = 3;  static const uint SemPosZ    = 4;
static const uint SemRotQx   = 5;  static const uint SemRotQy   = 6;  static const uint SemRotQz   = 7;  static const uint SemRotQw = 8;
static const uint SemScaleX  = 9;  static const uint SemScaleY  = 10; static const uint SemScaleZ  = 11;
static const uint SemColorR  = 12; static const uint SemColorG  = 13; static const uint SemColorB  = 14; static const uint SemColorA = 15;
static const uint SemMeshID          = 16;
static const uint SemEntityCacheIdx  = 17; // always-on: scatter → instance slot 16
static const uint SemSkeletonID          = 18;
static const uint SemSkinMeshID          = 19;
static const uint SemAnimBaseAnimID      = 20;
static const uint SemAnimBlendspaceID    = 21;
static const uint SemAnimBaseTimestamp   = 22;
static const uint SemAnimBlendCoordX     = 23;
static const uint SemAnimBlendCoordY     = 24;
static const uint SemAnimFadeAnimID      = 25;
static const uint SemAnimFadeTimestamp   = 26;
static const uint SemAnimFadeAlpha       = 27;
static const uint SemAnimFlags           = 28;
static const uint SemAnimLayer0AnimID    = 29;
static const uint SemAnimLayer1AnimID    = 30;
static const uint SemAnimLayer0Timestamp = 31;
static const uint SemAnimLayer1Timestamp = 32;
static const uint SemAnimLayer0Alpha     = 33;
static const uint SemAnimLayer1Alpha     = 34;
static const uint SemAnimLayer0Config    = 35;
static const uint SemAnimLayer1Config    = 36;

// Slab slot indices for skinning compute: CurrFieldAddrs[SlabIdx_XXX].
// Formula: SlabIdx = sem - 2 for anim sems (18..36 → slab indices 16..34).
static const uint SlabIdx_SkeletonID      = 16;
static const uint SlabIdx_SkinMeshID      = 17;
static const uint SlabIdx_BaseAnimID      = 18;
static const uint SlabIdx_BlendspaceID    = 19;
static const uint SlabIdx_BaseTimestamp   = 20;
static const uint SlabIdx_BlendCoordX     = 21;
static const uint SlabIdx_BlendCoordY     = 22;
static const uint SlabIdx_FadeAnimID      = 23;
static const uint SlabIdx_FadeTimestamp   = 24;
static const uint SlabIdx_FadeAlpha       = 25;
static const uint SlabIdx_AnimFlags       = 26;
static const uint SlabIdx_Layer0AnimID    = 27;
static const uint SlabIdx_Layer1AnimID    = 28;
static const uint SlabIdx_Layer0Timestamp = 29;
static const uint SlabIdx_Layer1Timestamp = 30;
static const uint SlabIdx_Layer0Alpha     = 31;
static const uint SlabIdx_Layer1Alpha     = 32;
static const uint SlabIdx_Layer0Config    = 33;
static const uint SlabIdx_Layer1Config    = 34;

static const uint MAX_BONES_PER_ENTITY   = 128;
static const uint MAX_SKELETAL_INSTANCES = 512;

// Per-skeleton slot table entry — skinning shader reads to locate bones in the mega-buffer.
struct GpuSkeletonSlotInfo { uint boneOffset; uint boneCount; };

// Per-animation slot table entry — skinning shader reads to locate tracks in the mega-buffer.
struct GpuAnimSlotInfo { uint trackOffset; uint boneCount; };

// GPU-side mesh slot — mirrors MeshManager::MeshSlot for GPU read.
struct GpuMeshInfo
{
    uint FirstIndex;
    uint IndexCount;
    int  VertexOffset;
    uint _pad;
};

// Persistently-mapped per-frame GPU struct.
// A single 8-byte push constant holds its device address.
struct GpuFrameData
{
    float Position[3];
    float OldPosition[3];
    float Rotation[4];
    float OldRotation[4];
    float FoV;
    float OldFoV;
    float    AspectRatio;              // viewport width / height
    float    _pad1;
    uint64_t InstancesAddr;            // sorted SoA — vertex shader reads from here
    uint64_t DrawArgsAddr;             // VkDrawIndexedIndirectCommand[MeshCount]
    uint64_t VerticesAddr;             // mesh vertex buffer (BDA)
    uint64_t CompactCounterAddr;       // single uint: global atomicAdd target
    uint64_t ScanAddr;                 // uint per entity: exclusive-scan output index
    float    Alpha;                    // render interpolation [0.0, 1.0]
    uint     EntityCount;
    uint     FieldCount;               // valid entries in Prev/CurrFieldAddrs
    uint     OutFieldStride;           // element stride between SoA output fields
    uint64_t UnsortedInstancesAddr;    // scatter writes here (pre-sort)
    uint64_t MeshHistogramAddr;        // uint[256] counting sort histogram
    uint64_t MeshWriteIdxAddr;         // uint[256] atomic write indices for sort
    uint64_t MeshTableAddr;            // GpuMeshInfo[MeshCount]
    uint     MeshCount;                // number of registered mesh types
    uint     _pad0;
    // Skinning / animation GPU addresses
    uint64_t SkinMatrixAddr;            // float4x4[MAX_SKELETAL_INSTANCES × MAX_BONES_PER_ENTITY]
    uint64_t SkeletalListAddr;          // uint[MAX_SKELETAL_INSTANCES] compact entity cache indices (GPU-written by scatter)
    uint64_t SkeletalIdxByEntityAddr;   // uint[MAX_CACHED_ENTITIES] cacheIdx → compact skeletal idx (GPU-written by scatter)
    uint64_t GpuBoneDataAddr;           // GpuBoneData[MAX_TOTAL_BONES]
    uint64_t GpuBoneParentAddr;         // uint32[MAX_TOTAL_BONES] parent bone indices
    uint64_t AnimTrackAddr;             // GpuAnimBoneTrack[MAX_TOTAL_BONE_TRACKS]
    uint64_t AnimKeyframeAddr;          // GpuAnimKeyframe[MAX_TOTAL_KEYFRAMES]
    uint64_t SkinWeightAddr;            // SkinWeights[MAX_TOTAL_SKIN_WEIGHTS]
    uint64_t SkinSlotTableAddr;         // GpuSkinSlotInfo[MAX_MESH_SLOTS]
    uint64_t SkeletalDispatchArgsAddr;  // VkDispatchIndirectCommand {x=skeletalCount,y=1,z=1}
    uint64_t GpuSkeletonSlotAddr;       // GpuSkeletonSlotInfo[MAX_SKELETON_SLOTS]
    uint64_t GpuAnimSlotAddr;           // GpuAnimSlotInfo[MAX_ANIM_SLOTS]
    uint64_t PrevFieldAddrs[MaxGpuFields];
    uint64_t CurrFieldAddrs[MaxGpuFields];
    uint     FieldSemantics[MaxGpuFields];
    uint     FieldElementSize[MaxGpuFields];
};