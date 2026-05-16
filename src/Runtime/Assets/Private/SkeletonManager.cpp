#include "SkeletonManager.h"
#include "AssetRegistry.h"
#include "SkeletonAsset.h"
#include "Logger.h"

#include <cstring>

// -----------------------------------------------------------------------
// Initialize
// -----------------------------------------------------------------------

bool SkeletonManager::Initialize(VulkanMemory* vkMem)
{
	BoneDataBuffer = vkMem->AllocateBuffer(
		MAX_TOTAL_BONES * sizeof(GpuBoneData),
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		GpuMemoryDomain::PersistentMapped,
		/*requestDeviceAddress=*/ true);

	if (!BoneDataBuffer.IsValid())
	{
		LOG_ENG_ERROR("[SkeletonManager] Bone data buffer allocation failed");
		return false;
	}

	std::memset(BoneDataBuffer.MappedPtr, 0, MAX_TOTAL_BONES * sizeof(GpuBoneData));

	LOG_ENG_INFO_F("[SkeletonManager] Initialized (max bones: %u, buffer: %.1f MB)",
				   MAX_TOTAL_BONES,
				   static_cast<float>(MAX_TOTAL_BONES * sizeof(GpuBoneData)) / (1024.f * 1024.f));
	return true;
}

// -----------------------------------------------------------------------
// Shutdown
// -----------------------------------------------------------------------

void SkeletonManager::Shutdown()
{
	BoneDataBuffer.Free();
}

// -----------------------------------------------------------------------
// CommitToSlot — internal; copies bone data into mega-buffer and CPU copy.
// Does NOT call Register() — caller owns that.
// -----------------------------------------------------------------------

uint32_t SkeletonManager::CommitToSlot(const SkeletonAsset& asset, AssetID id)
{
	if (SkeletonCount >= MAX_SKELETON_SLOTS)
	{
		LOG_ENG_ERROR("[SkeletonManager] Skeleton slot limit reached");
		return UINT32_MAX;
	}

	if (NextBoneOffset + asset.boneCount > MAX_TOTAL_BONES)
	{
		LOG_ENG_ERROR("[SkeletonManager] Bone mega-buffer overflow");
		return UINT32_MAX;
	}

	uint32_t slotID    = SkeletonCount++;
	SlotIDs[slotID]    = id;

	if (id.IsValid()) AssetRegistry::Get().RegisterSlot(AssetType::SkeletalMesh, slotID, id);

	Slots[slotID].boneOffset = NextBoneOffset;
	Slots[slotID].boneCount  = asset.boneCount;

	// Copy inverse bind poses to CPU BoneData shadow and GPU buffer.
	auto* gpuDst = static_cast<GpuBoneData*>(BoneDataBuffer.MappedPtr) + NextBoneOffset;
	for (uint32_t i = 0; i < asset.boneCount; ++i)
	{
		std::memcpy(BoneData[NextBoneOffset + i].inverseBindPose,
					asset.bones[i].inverseBindPose, sizeof(float) * 16);
		std::memcpy(gpuDst[i].inverseBindPose,
					asset.bones[i].inverseBindPose, sizeof(float) * 16);
	}

	CpuCopies[slotID] = asset; // full copy — bones + sockets retained for chain walks

	if (id.IsValid())
	{
		if (AssetEntry* entry = AssetRegistry::Get().FindMutable(id))
		{
			entry->Data  = reinterpret_cast<void*>(static_cast<uintptr_t>(slotID));
			entry->State = RuntimeFlags::Loaded;
			entry->OnLoaded(slotID);
			entry->OnLoaded.Reset();
		}
	}

	NextBoneOffset += asset.boneCount;

	LOG_ENG_INFO_F("[SkeletonManager] Loaded skeleton slot %u (%u bones, %u sockets)",
				   slotID, asset.boneCount, asset.socketCount);
	return slotID;
}

// -----------------------------------------------------------------------
// LoadSkeleton
// -----------------------------------------------------------------------

uint32_t SkeletonManager::LoadSkeleton(const SkeletonAsset& asset, TnxName name, AssetID id)
{
	if (id.IsValid()) AssetRegistry::Get().Register(id, name.GetStr(), {}, AssetType::SkeletalMesh);
	return CommitToSlot(asset, id);
}

uint32_t SkeletonManager::LoadSkeleton(AssetID id)
{
	uint32_t slot = FindSlotByID(id);
	if (slot != UINT32_MAX) return slot;

	const AssetEntry* entry = AssetRegistry::Get().Find(id);
	if (!entry || entry->Type != AssetType::SkeletalMesh)
	{
		LOG_ENG_ERROR("[SkeletonManager] LoadSkeleton: AssetID not in registry");
		return UINT32_MAX;
	}

	std::string path = AssetRegistry::Get().ResolvePath(id);
	if (path.empty())
	{
		LOG_ENG_ERROR("[SkeletonManager] LoadSkeleton: no resolvable path for AssetID");
		return UINT32_MAX;
	}

	SkeletonAsset asset;
	if (!LoadSkeletonAsset(asset, path))
	{
		LOG_ENG_ERROR_F("[SkeletonManager] LoadSkeleton: failed to decode '%s'", path.c_str());
		return UINT32_MAX;
	}

	return CommitToSlot(asset, id);
}

uint32_t SkeletonManager::LoadSkeleton(TnxName name)
{
	const AssetEntry* entry = AssetRegistry::Get().FindByTName(name);
	if (!entry || entry->Type != AssetType::SkeletalMesh)
	{
		LOG_ENG_ERROR_F("[SkeletonManager] LoadSkeleton: TnxName '%s' not in registry",
						name.GetStr());
		return UINT32_MAX;
	}
	return LoadSkeleton(entry->ID);
}

// -----------------------------------------------------------------------
// GetSkeletonCPU
// -----------------------------------------------------------------------

const SkeletonAsset* SkeletonManager::GetSkeletonCPU(uint32_t slot) const
{
	if (slot == 0 || slot >= SkeletonCount) return nullptr;
	return &CpuCopies[slot];
}
