#pragma once
#include <cstdint>
#include <string>

#include "AssetRegistry.h"
#include "AssetTypes.h"
#include "AnimationAsset.h"
#include "TrinyxJobs.h"
#include "VulkanMemory.h"

// -----------------------------------------------------------------------
// GPU-side animation data structures — mirrors of CPU AnimBoneTrack and
// AnimKeyframe, with absolute global keyframe offsets for direct indexing
// in the skinning compute shader.
// -----------------------------------------------------------------------

struct GpuAnimBoneTrack
{
	uint32_t keyframeOffset; // absolute index into global GpuAnimKeyframe array
	uint32_t keyframeCount;
};

static_assert(sizeof(GpuAnimBoneTrack) == 8, "GpuAnimBoneTrack must be 8 bytes");

struct GpuAnimKeyframe
{
	float time;
	float tx, ty, tz;      // translation
	float rx, ry, rz, rw;  // rotation quaternion (normalized)
};

static_assert(sizeof(GpuAnimKeyframe) == 32, "GpuAnimKeyframe must be 32 bytes");

// -----------------------------------------------------------------------
// AnimationManager — flat keyframe storage for all loaded animations.
//
// All animation tracks and keyframes are packed into two mega-buffers
// (tracks + keyframes), each with a persistent-mapped GPU mirror for the
// skinning compute pass. CPU copies of AnimationAsset are retained for
// EvaluateBlendedBone lookups in ESkeletalEntity::PostPhysics.
//
// GpuAnimBoneTrack.keyframeOffset is an absolute global index, so the
// skinning shader can index directly without per-slot base adjustment.
//
// AssetRegistry is the authority for name/ID lookup.
// Slot 0 is reserved as the invalid/error sentinel.
// -----------------------------------------------------------------------

class AnimationManager
{
public:
	static constexpr uint32_t MAX_ANIM_SLOTS        = 2048;
	static constexpr uint32_t MAX_TOTAL_BONE_TRACKS = 131072; // avg 64 bones × 2048 anims
	static constexpr uint32_t MAX_TOTAL_KEYFRAMES   = 4194304; // ~4M keyframes

	struct AnimSlot
	{
		uint32_t trackOffset;    // first GpuAnimBoneTrack in TrackBuffer for this anim
		uint32_t boneCount;
		uint32_t keyframeOffset; // first GpuAnimKeyframe in KeyframeBuffer for this anim
		float    duration;
	};

	bool Initialize(VulkanMemory* vkMem);

	/// Free GPU buffers. Must be called before VulkanMemory::Shutdown().
	void Shutdown();

	/// Load an AnimationAsset — packs tracks + keyframes into the mega-buffers and
	/// mirrors them to the GPU. Records name/ID in AssetRegistry.
	/// Returns the slot ID, or UINT32_MAX on failure.
	uint32_t LoadAnimation(const AnimationAsset& asset, TnxName name, AssetID id = {});

	/// Resolve by AssetID from AssetRegistry, decode from disk, and load.
	uint32_t LoadAnimation(AssetID id);

	/// Resolve by TnxName from AssetRegistry, decode from disk, and load.
	uint32_t LoadAnimation(TnxName name);

	/// CPU animation for EvaluateBone() lookups.
	const AnimationAsset* GetAnimCPU(uint32_t slot) const;

	float GetDuration(uint32_t slot) const
	{
		return (slot > 0 && slot < AnimCount) ? Slots[slot].duration : 0.f;
	}

	uint64_t        GetTrackBufferAddr()    const { return TrackBuffer.DeviceAddr; }
	uint64_t        GetKeyframeBufferAddr() const { return KeyframeBuffer.DeviceAddr; }
	const AnimSlot& GetSlot(uint32_t slot)  const { return Slots[slot]; }
	uint32_t        GetAnimCount()          const { return AnimCount; }

	/// Block until all pending GPU track/keyframe upload jobs have completed.
	void FlushPendingUploads();

	/// Non-blocking poll — true when all GPU upload jobs have completed.
	bool IsUploadComplete() const { return GpuUploadCounter.Value.load(std::memory_order_acquire) == 0; }

	uint32_t FindSlotByTName(TnxName name) const
	{
		const AssetEntry* e = AssetRegistry::Get().FindByTName(name);
		if (!e || e->Type != AssetType::Animation) return UINT32_MAX;
		return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(e->Data));
	}

	uint32_t FindSlotByID(AssetID id) const
	{
		const AssetEntry* e = AssetRegistry::Get().Find(id);
		if (!e || e->Type != AssetType::Animation) return UINT32_MAX;
		return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(e->Data));
	}

	AssetID GetSlotID(uint32_t slot) const { return SlotIDs[slot]; }

	static AnimationManager& Get()
	{
		static AnimationManager instance;
		return instance;
	}

private:
	/// Pack asset data into flat buffers, mirror to GPU, update AssetRegistry.
	/// Does NOT call Register() — caller owns that.
	uint32_t CommitToSlot(const AnimationAsset& asset, AssetID id);

	VulkanBuffer TrackBuffer;    // GpuAnimBoneTrack[MAX_TOTAL_BONE_TRACKS], PersistentMapped + BDA
	VulkanBuffer KeyframeBuffer; // GpuAnimKeyframe[MAX_TOTAL_KEYFRAMES], PersistentMapped + BDA

	AnimSlot               Slots[MAX_ANIM_SLOTS]{};
	AssetID                SlotIDs[MAX_ANIM_SLOTS]{};
	AnimationAsset         CpuCopies[MAX_ANIM_SLOTS];
	TrinyxJobs::JobCounter GpuUploadCounter;
	uint32_t               NextTrack    = 0;
	uint32_t               NextKeyframe = 0;
	uint32_t               AnimCount    = 1; // slot 0 reserved as invalid sentinel
};
