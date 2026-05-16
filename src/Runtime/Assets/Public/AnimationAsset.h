#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "AnimTypes.h"

// -----------------------------------------------------------------------
// .tnxanim binary format  (version 1 — linear keyframes)
//
// 64-byte header:
//   Offset  Size  Field
//   0       4     Magic: "ANIM" (0x4D494E41)
//   4       4     Version: 1  (M3: bump to 2 for CurveHandle compression)
//   8       4     BoneCount
//   12      4     KeyframeCount  (total across all bone tracks)
//   16      4     Duration (float seconds)
//   20      4     NotifyCount  (M2: populated; M1: 0)
//   24      1     HasRootMotion (M2: populated; M1: 0)
//   25      39    Reserved (pad to 64)
//   64      T     AnimBoneTrackDisk[BoneCount]  (8 bytes each)
//   64+T    K     AnimKeyframeDisk[KeyframeCount]  (44 bytes each)
//   64+T+K  N     AnimNotifyDisk[NotifyCount]  (M2+, 48 bytes each)
// -----------------------------------------------------------------------

constexpr uint32_t TnxAnimMagic   = 0x4D494E41; // "ANIM"
constexpr uint32_t TnxAnimVersion = 1;

struct TnxAnimHeader
{
	uint32_t Magic          = TnxAnimMagic;
	uint32_t Version        = TnxAnimVersion;
	uint32_t BoneCount      = 0;
	uint32_t KeyframeCount  = 0;
	float    Duration       = 0.f;
	uint32_t NotifyCount    = 0;
	uint8_t  HasRootMotion  = 0;
	uint8_t  Reserved[39]{};
};

static_assert(sizeof(TnxAnimHeader) == 64, "TnxAnimHeader must be exactly 64 bytes");

// On-disk bone track — indexes into the flat keyframe array.
struct AnimBoneTrackDisk
{
	uint32_t keyframeOffset; // first keyframe index for this bone
	uint32_t keyframeCount;
};

static_assert(sizeof(AnimBoneTrackDisk) == 8);

// On-disk keyframe — translation + rotation, scale omitted (M1).
struct AnimKeyframeDisk
{
	float time;
	float tx, ty, tz;
	float rx, ry, rz, rw;
};

static_assert(sizeof(AnimKeyframeDisk) == 32);

// On-disk notify — M2+.
struct AnimNotifyDisk
{
	uint32_t idHash;       // NotifyID (TnxName::Value)
	float    triggerTime;
	float    duration;     // 0 = instant, >0 = state notify
	uint32_t nameHash;     // TnxName::Value for editor display
	char     nameStr[32];  // editor-only string
};

static_assert(sizeof(AnimNotifyDisk) == 48);

// -----------------------------------------------------------------------
// In-memory animation clip.  Loaded from .tnxanim, kept by AnimationManager.
// -----------------------------------------------------------------------

struct AnimBoneTrack
{
	uint32_t keyframeOffset;
	uint32_t keyframeCount;
};

struct AnimKeyframe
{
	float time;
	float tx, ty, tz;
	float rx, ry, rz, rw;
};

struct AnimNotifyDef
{
	NotifyID id;          // TnxName hash
	float    triggerTime;
	float    duration;    // 0 = instant
	TnxName  name;
};

struct AnimationAsset
{
	float    duration  = 0.f;
	uint32_t boneCount = 0;

	std::vector<AnimBoneTrack> boneTracks; // one per bone
	std::vector<AnimKeyframe>  keyframes;  // flat array, all bones

	// M2+
	std::vector<AnimNotifyDef> notifies;
	bool hasRootMotion = false;
	std::vector<AnimKeyframe> rootMotionTrack; // bone-0 extracted separately for root motion

	// Evaluate bone transform at the given timestamp.
	// M1: linear interpolation between nearest keyframes.
	// M3: replaced by CurveHandle evaluation.
	BoneTransform EvaluateBone(uint32_t boneIndex, float timestamp) const;

	// M2+
	BoneTransform EvaluateRootMotionDelta(float fromTime, float toTime) const;
};

bool SaveAnimationAsset(const AnimationAsset& asset, const std::string& path);
bool LoadAnimationAsset(AnimationAsset& outAsset, const std::string& path);
