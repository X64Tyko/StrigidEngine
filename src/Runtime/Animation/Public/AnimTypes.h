#pragma once
#include <cstdint>
#include <cstring>
#include "TnxName.h"
#include "SimFloat.h"

// SocketID and NotifyID are TnxName hash values (FNV1a).
// Construct via TNX_NAME("LeftFoot") or TnxName("LeftFoot").Value at runtime.
// Comparison is a single uint32_t compare — same as TnxName::operator==.
using SocketID = uint32_t;
using NotifyID = uint32_t;

constexpr SocketID InvalidSocketID = 0;
constexpr NotifyID InvalidNotifyID = 0;

// Helper: create a SocketID from a compile-time string literal.
// Usage: GetSocketTransform(TNX_SOCKET("LeftFoot"))
#define TNX_SOCKET(str) (TnxName::Fnv1a(str))
#define TNX_NOTIFY(str) (TnxName::Fnv1a(str))

// Value-type bone transform — not SoA, used for chain evaluation results.
// All fields are SimFloat so that evaluated world transforms are deterministic
// under TNX_DETERMINISM and can feed collision/socket queries bit-identically.
struct BoneTransform
{
	SimFloat tx{}, ty{}, tz{};
	SimFloat rx{}, ry{}, rz{};
	SimFloat rw = SimFloat(1);
	SimFloat sx = SimFloat(1), sy = SimFloat(1), sz = SimFloat(1);

	static BoneTransform Identity() { return {}; }

	// Compose: apply 'child' in the local space of 'parent'.
	static BoneTransform Compose(const BoneTransform& parent, const BoneTransform& child);

	// Normalized linear blend of two bone transforms by weight [0,1].
	static BoneTransform NLerp(const BoneTransform& a, const BoneTransform& b, SimFloat t);

	// Weighted accumulate for multi-slot blend.  Call Normalize() after all slots added.
	static BoneTransform WeightedAdd(const BoneTransform& acc, const BoneTransform& sample, SimFloat w);
	static BoneTransform Normalize(const BoneTransform& acc, SimFloat totalWeight);
};

// (SocketID and NotifyID defined at top of file)

// -----------------------------------------------------------------------
// BoneCacheLocal — PostPhysics-scoped bone transform cache.
// Allocated inline on ESkeletalEntity; not persistent across ticks.
// Each tick begins with Clear().  Subsequent socket queries accumulate
// evaluated ancestors so shared chain segments are not re-evaluated.
// -----------------------------------------------------------------------

struct BoneCacheEntry
{
	uint32_t    boneIndex = 0xFFFFFFFF;
	BoneTransform worldTransform;
};

struct ChainWalkResult
{
	BoneTransform ancestorTransform; // nearest cached ancestor (or identity for root)
	uint32_t      chain[64];         // bone indices from ancestor toward target (exclusive ancestor, inclusive target)
	uint32_t      remainingBones = 0;
};

struct SkeletonAsset; // forward — resolved via SkeletonManager

struct BoneCacheLocal
{
	static constexpr uint32_t MaxCachedBones = 16;

	BoneCacheEntry entries[MaxCachedBones];
	uint32_t       count = 0;

	void Clear() { count = 0; }

	const BoneTransform* Find(uint32_t boneIndex) const
	{
		for (uint32_t i = 0; i < count; ++i)
			if (entries[i].boneIndex == boneIndex) return &entries[i].worldTransform;
		return nullptr;
	}

	void Insert(uint32_t boneIndex, const BoneTransform& t)
	{
		if (count < MaxCachedBones)
		{
			entries[count].boneIndex      = boneIndex;
			entries[count].worldTransform = t;
			++count;
		}
	}

	// Walk up the skeleton hierarchy from targetBone to find the nearest cached ancestor.
	// Returns the cached ancestor's world transform and the chain of bone indices from
	// (exclusive) ancestor down to (inclusive) targetBone that still need evaluation.
	ChainWalkResult FindNearestCachedAncestor(uint32_t targetBoneIndex,
	                                          const SkeletonAsset& skeleton) const;
};

// -----------------------------------------------------------------------
// SocketTransformLocal — per-tick socket result cache.
// -----------------------------------------------------------------------

struct SocketEntry
{
	SocketID      id           = InvalidSocketID;
	uint32_t      boneIndex    = 0xFFFFFFFF;
	BoneTransform localOffset;   // socket offset in bone space (from SocketDef)
	BoneTransform worldTransform;
	bool          valid        = false;
};

struct SocketTransformLocal
{
	static constexpr uint32_t MaxSockets = 12;

	SocketEntry sockets[MaxSockets];
	uint32_t    count = 0;

	void Clear()
	{
		for (uint32_t i = 0; i < count; ++i) sockets[i].valid = false;
	}

	void RegisterSocket(SocketID id, uint32_t boneIndex, const BoneTransform& localOffset)
	{
		if (count >= MaxSockets) return;
		sockets[count].id          = id;
		sockets[count].boneIndex   = boneIndex;
		sockets[count].localOffset = localOffset;
		sockets[count].valid       = false;
		++count;
	}

	SocketEntry* FindSocket(SocketID id)
	{
		for (uint32_t i = 0; i < count; ++i)
			if (sockets[i].id == id) return &sockets[i];
		return nullptr;
	}
};

// -----------------------------------------------------------------------
// NotifyFireEvent — payload delivered to OnAnimNotify.
// -----------------------------------------------------------------------

struct NotifyFireEvent
{
	NotifyID  id;
	SimFloat  blendWeight;  // weight of the blend slot that fired this notify
	float     triggerTime;
};

// -----------------------------------------------------------------------
// AnimNotifyState — inert inline member on ESkeletalEntity.
// Tracks which notifies have fired this loop to prevent double-fire.
// Not persistent across ticks for M1; persistent storage is M3.
// -----------------------------------------------------------------------

struct NotifyFireRecord
{
	NotifyID id;
	uint32_t animSlot;
	SimFloat firedAtTime;
};

struct AnimNotifyState
{
	static constexpr uint32_t MaxFireRecords = 32;

	NotifyFireRecord fired[MaxFireRecords];
	uint32_t         firedCount = 0;

	void Clear() { firedCount = 0; }

	bool HasFiredThisLoop(NotifyID id, uint32_t slot, SimFloat loopStart) const
	{
		for (uint32_t i = 0; i < firedCount; ++i)
			if (fired[i].id == id && fired[i].animSlot == slot && fired[i].firedAtTime >= loopStart)
				return true;
		return false;
	}

	void RecordFire(NotifyID id, uint32_t slot, SimFloat time)
	{
		if (firedCount >= MaxFireRecords) return;
		fired[firedCount++] = {id, slot, time};
	}

	void ClearLoopedRecords(uint32_t slot, SimFloat newLoopStart)
	{
		uint32_t write = 0;
		for (uint32_t i = 0; i < firedCount; ++i)
			if (!(fired[i].animSlot == slot && fired[i].firedAtTime < newLoopStart))
				fired[write++] = fired[i];
		firedCount = write;
	}
};
