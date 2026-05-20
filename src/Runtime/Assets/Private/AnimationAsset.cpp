#include "AnimationAsset.h"
#include "Logger.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>

static SimFloat QDot(SimFloat ax, SimFloat ay, SimFloat az, SimFloat aw,
                     SimFloat bx, SimFloat by, SimFloat bz, SimFloat bw)
{
	return ax * bx + ay * by + az * bz + aw * bw;
}

static void QNLerp(SimFloat ax, SimFloat ay, SimFloat az, SimFloat aw,
                   SimFloat bx, SimFloat by, SimFloat bz, SimFloat bw,
                   SimFloat t,
                   SimFloat& ox, SimFloat& oy, SimFloat& oz, SimFloat& ow)
{
	if (QDot(ax, ay, az, aw, bx, by, bz, bw) < SimFloat(0))
	{
		bx = -bx; by = -by; bz = -bz; bw = -bw;
	}
	SimFloat it = SimFloat(1) - t;
	ox = it * ax + t * bx;
	oy = it * ay + t * by;
	oz = it * az + t * bz;
	ow = it * aw + t * bw;
	SimFloat len = Sqrt(ox * ox + oy * oy + oz * oz + ow * ow);
	if (len > SimFloat(1e-6f)) { ox /= len; oy /= len; oz /= len; ow /= len; }
}

BoneTransform BoneTransform::Compose(const BoneTransform& parent, const BoneTransform& child)
{
	SimFloat qx = parent.rx, qy = parent.ry, qz = parent.rz, qw = parent.rw;
	SimFloat vx  = child.tx * parent.sx;
	SimFloat vy  = child.ty * parent.sy;
	SimFloat vz  = child.tz * parent.sz;

	SimFloat tx_ = 2 * (qy * vz - qz * vy);
	SimFloat ty_ = 2 * (qz * vx - qx * vz);
	SimFloat tz_ = 2 * (qx * vy - qy * vx);

	BoneTransform r;
	r.tx = parent.tx + vx + qw * tx_ + qy * tz_ - qz * ty_;
	r.ty = parent.ty + vy + qw * ty_ + qz * tx_ - qx * tz_;
	r.tz = parent.tz + vz + qw * tz_ + qx * ty_ - qy * tx_;

	// Multiply quaternions: parent.r * child.r
	// quatMul(parent.r, child.r)
	r.rx = qw * child.rx + qx * child.rw + qy * child.rz - qz * child.ry;
	r.ry = qw * child.ry - qx * child.rz + qy * child.rw + qz * child.rx;
	r.rz = qw * child.rz + qx * child.ry - qy * child.rx + qz * child.rw;
	r.rw = qw * child.rw - qx * child.rx - qy * child.ry - qz * child.rz;

	r.sx = parent.sx * child.sx;
	r.sy = parent.sy * child.sy;
	r.sz = parent.sz * child.sz;

	return r;
}

BoneTransform BoneTransform::NLerp(const BoneTransform& a, const BoneTransform& b, SimFloat t)
{
	BoneTransform r;
	SimFloat it = SimFloat(1) - t;
	r.tx = it * a.tx + t * b.tx;
	r.ty = it * a.ty + t * b.ty;
	r.tz = it * a.tz + t * b.tz;
	QNLerp(a.rx, a.ry, a.rz, a.rw, b.rx, b.ry, b.rz, b.rw, t, r.rx, r.ry, r.rz, r.rw);
	r.sx = it * a.sx + t * b.sx;
	r.sy = it * a.sy + t * b.sy;
	r.sz = it * a.sz + t * b.sz;
	return r;
}

BoneTransform BoneTransform::WeightedAdd(const BoneTransform& acc, const BoneTransform& sample, SimFloat w)
{
	BoneTransform r;
	r.tx = acc.tx + sample.tx * w;
	r.ty = acc.ty + sample.ty * w;
	r.tz = acc.tz + sample.tz * w;
	// Flip sign for shortest-arc before accumulating; Normalize() re-normalizes after all slots.
	SimFloat flip = QDot(acc.rx, acc.ry, acc.rz, acc.rw,
	                     sample.rx, sample.ry, sample.rz, sample.rw) < SimFloat(0) ? -w : w;
	r.rx = acc.rx + sample.rx * flip;
	r.ry = acc.ry + sample.ry * flip;
	r.rz = acc.rz + sample.rz * flip;
	r.rw = acc.rw + sample.rw * flip;
	r.sx = acc.sx + sample.sx * w;
	r.sy = acc.sy + sample.sy * w;
	r.sz = acc.sz + sample.sz * w;
	return r;
}

BoneTransform BoneTransform::Normalize(const BoneTransform& acc, SimFloat totalWeight)
{
	if (totalWeight <= SimFloat(0)) return BoneTransform::Identity();
	SimFloat inv = SimFloat(1) / totalWeight;
	BoneTransform r;
	r.tx = acc.tx * inv;
	r.ty = acc.ty * inv;
	r.tz = acc.tz * inv;
	r.sx = acc.sx * inv;
	r.sy = acc.sy * inv;
	r.sz = acc.sz * inv;
	// Normalize quaternion
	SimFloat len = Sqrt(acc.rx * acc.rx + acc.ry * acc.ry + acc.rz * acc.rz + acc.rw * acc.rw);
	if (len > SimFloat(1e-6f)) { r.rx = acc.rx / len; r.ry = acc.ry / len; r.rz = acc.rz / len; r.rw = acc.rw / len; }
	else { r.rx = SimFloat{}; r.ry = SimFloat{}; r.rz = SimFloat{}; r.rw = SimFloat(1); }
	return r;
}

BoneTransform BoneTransform::ApplyAdditive(const BoneTransform& base, const BoneTransform& delta, SimFloat alpha)
{
	BoneTransform r = base;

	// Translation: offset added directly
	r.tx = base.tx + delta.tx * alpha;
	r.ty = base.ty + delta.ty * alpha;
	r.tz = base.tz + delta.tz * alpha;

	// Rotation: compose base * nlerp(identity, delta, alpha)
	SimFloat dx = delta.rx * alpha;
	SimFloat dy = delta.ry * alpha;
	SimFloat dz = delta.rz * alpha;
	SimFloat dw = SimFloat(1) + (delta.rw - SimFloat(1)) * alpha;
	// Ensure shortest-arc from identity
	if (dw < SimFloat(0)) { dx = -dx; dy = -dy; dz = -dz; dw = -dw; }
	SimFloat len = Sqrt(dx*dx + dy*dy + dz*dz + dw*dw);
	if (len > SimFloat(1e-6f)) { dx /= len; dy /= len; dz /= len; dw /= len; }
	else { dx = SimFloat{}; dy = SimFloat{}; dz = SimFloat{}; dw = SimFloat(1); }
	// quatMul(base.r, weighted_delta)
	SimFloat bx = base.rx, by = base.ry, bz = base.rz, bw = base.rw;
	r.rx = bw*dx + bx*dw + by*dz - bz*dy;
	r.ry = bw*dy - bx*dz + by*dw + bz*dx;
	r.rz = bw*dz + bx*dy - by*dx + bz*dw;
	r.rw = bw*dw - bx*dx - by*dy - bz*dz;
	SimFloat rlen = Sqrt(r.rx*r.rx + r.ry*r.ry + r.rz*r.rz + r.rw*r.rw);
	if (rlen > SimFloat(1e-6f)) { r.rx /= rlen; r.ry /= rlen; r.rz /= rlen; r.rw /= rlen; }

	// Scale: offset from identity (delta.s - 1) added
	r.sx = base.sx + (delta.sx - SimFloat(1)) * alpha;
	r.sy = base.sy + (delta.sy - SimFloat(1)) * alpha;
	r.sz = base.sz + (delta.sz - SimFloat(1)) * alpha;

	return r;
}


static BoneTransform EvaluateTrack(const std::vector<AnimKeyframe>& keyframes,
                                   const AnimBoneTrack& track, float timestamp)
{
	if (track.keyframeCount == 0) return BoneTransform::Identity();

	const AnimKeyframe* keys = keyframes.data() + track.keyframeOffset;

	if (track.keyframeCount == 1 || timestamp <= keys[0].time)
	{
		const AnimKeyframe& k = keys[0];
		BoneTransform r;
		r.tx = k.tx; r.ty = k.ty; r.tz = k.tz;
		r.rx = k.rx; r.ry = k.ry; r.rz = k.rz; r.rw = k.rw;
		return r;
	}

	const uint32_t last = track.keyframeCount - 1;
	if (timestamp >= keys[last].time)
	{
		const AnimKeyframe& k = keys[last];
		BoneTransform r;
		r.tx = k.tx; r.ty = k.ty; r.tz = k.tz;
		r.rx = k.rx; r.ry = k.ry; r.rz = k.rz; r.rw = k.rw;
		return r;
	}

	// Binary search for the segment
	uint32_t lo = 0, hi = last;
	while (lo + 1 < hi)
	{
		uint32_t mid = (lo + hi) / 2;
		if (keys[mid].time <= timestamp) lo = mid;
		else hi = mid;
	}

	const AnimKeyframe& a = keys[lo];
	const AnimKeyframe& b = keys[hi];
	float span = b.time - a.time;
	float t    = (span > 1e-6f) ? (timestamp - a.time) / span : 0.f;

	BoneTransform r;
	r.tx = a.tx + (b.tx - a.tx) * t;
	r.ty = a.ty + (b.ty - a.ty) * t;
	r.tz = a.tz + (b.tz - a.tz) * t;
	QNLerp(SimFloat(a.rx), SimFloat(a.ry), SimFloat(a.rz), SimFloat(a.rw),
	       SimFloat(b.rx), SimFloat(b.ry), SimFloat(b.rz), SimFloat(b.rw),
	       SimFloat(t), r.rx, r.ry, r.rz, r.rw);
	r.sx = 1.f; r.sy = 1.f; r.sz = 1.f; // scale identity in M1
	return r;
}

BoneTransform AnimationAsset::EvaluateBone(uint32_t boneIndex, float timestamp) const
{
	if (boneIndex >= boneCount) return BoneTransform::Identity();
	return EvaluateTrack(keyframes, boneTracks[boneIndex], timestamp);
}

BoneTransform AnimationAsset::EvaluateRootMotionDelta(float fromTime, float toTime) const
{
	// M2: returns the delta transform between two timestamps on the root motion track.
	if (rootMotionTrack.empty()) return BoneTransform::Identity();
	BoneTransform from = EvaluateTrack(rootMotionTrack, {0, static_cast<uint32_t>(rootMotionTrack.size())}, fromTime);
	BoneTransform to   = EvaluateTrack(rootMotionTrack, {0, static_cast<uint32_t>(rootMotionTrack.size())}, toTime);
	// Delta: to.pos - from.pos in world space (simplified — ignores rotation delta for M2 stub)
	BoneTransform delta = BoneTransform::Identity();
	delta.tx = to.tx - from.tx;
	delta.ty = to.ty - from.ty;
	delta.tz = to.tz - from.tz;
	return delta;
}

// Defined here to resolve the forward declaration of SkeletonAsset in AnimTypes.h.

#include "SkeletonAsset.h"

ChainWalkResult BoneCacheLocal::FindNearestCachedAncestor(uint32_t targetBoneIndex,
                                                           const SkeletonAsset& skeleton) const
{
	// Build chain from root to targetBone
	uint32_t fullChain[256];
	uint32_t chainLen = 0;
	skeleton.GetChainFromRoot(targetBoneIndex, fullChain, chainLen);

	// Walk from targetBone upward to find the nearest cached ancestor
	// fullChain[0] == root, fullChain[chainLen-1] == targetBone
	ChainWalkResult result;
	result.ancestorTransform = BoneTransform::Identity();
	result.remainingBones    = 0;

	int32_t firstUncachedIdx = 0; // index into fullChain of first bone we still need to evaluate

	for (int32_t i = static_cast<int32_t>(chainLen) - 1; i >= 0; --i)
	{
		const BoneTransform* cached = Find(fullChain[i]);
		if (cached)
		{
			result.ancestorTransform = *cached;
			firstUncachedIdx         = i + 1;
			break;
		}
	}

	// Remaining = bones from firstUncachedIdx to targetBone (inclusive)
	result.remainingBones = chainLen - static_cast<uint32_t>(firstUncachedIdx);
	for (uint32_t i = 0; i < result.remainingBones; ++i)
		result.chain[i] = fullChain[firstUncachedIdx + i];

	return result;
}

bool SaveAnimationAsset(const AnimationAsset& asset, const std::string& path)
{
	std::ofstream file(path, std::ios::binary);
	if (!file.is_open())
	{
		LOG_ENG_ERROR_F("[AnimationAsset] Failed to open '%s' for writing", path.c_str());
		return false;
	}

	TnxAnimHeader header;
	header.BoneCount     = asset.boneCount;
	header.KeyframeCount = static_cast<uint32_t>(asset.keyframes.size());
	header.Duration      = asset.duration;
	header.NotifyCount   = static_cast<uint32_t>(asset.notifies.size());
	header.HasRootMotion = asset.hasRootMotion ? 1 : 0;
	file.write(reinterpret_cast<const char*>(&header), sizeof(header));

	// Bone tracks
	for (const AnimBoneTrack& t : asset.boneTracks)
	{
		AnimBoneTrackDisk d{t.keyframeOffset, t.keyframeCount};
		file.write(reinterpret_cast<const char*>(&d), sizeof(d));
	}

	// Keyframes
	for (const AnimKeyframe& k : asset.keyframes)
	{
		AnimKeyframeDisk d{k.time, k.tx, k.ty, k.tz, k.rx, k.ry, k.rz, k.rw};
		file.write(reinterpret_cast<const char*>(&d), sizeof(d));
	}

	// Notifies (M2+)
	for (const AnimNotifyDef& n : asset.notifies)
	{
		AnimNotifyDisk d;
		d.idHash      = n.id;
		d.triggerTime = n.triggerTime;
		d.duration    = n.duration;
		d.nameHash    = n.name.Value;
		std::memset(d.nameStr, 0, sizeof(d.nameStr));
#ifndef TNX_STRIP_NAMES
		std::strncpy(d.nameStr, n.name.GetStr(), sizeof(d.nameStr) - 1);
#endif
		file.write(reinterpret_cast<const char*>(&d), sizeof(d));
	}

	if (!file.good())
	{
		LOG_ENG_ERROR_F("[AnimationAsset] Write error for '%s'", path.c_str());
		return false;
	}

	LOG_ENG_INFO_F("[AnimationAsset] Saved '%s' (%.2fs, %u bones, %u keyframes)",
	               path.c_str(), asset.duration, asset.boneCount,
	               static_cast<uint32_t>(asset.keyframes.size()));
	return true;
}

bool LoadAnimationAsset(AnimationAsset& outAsset, const std::string& path)
{
	std::ifstream file(path, std::ios::binary);
	if (!file.is_open())
	{
		LOG_ENG_ERROR_F("[AnimationAsset] Failed to open '%s' for reading", path.c_str());
		return false;
	}

	TnxAnimHeader header;
	file.read(reinterpret_cast<char*>(&header), sizeof(header));

	if (header.Magic != TnxAnimMagic)
	{
		LOG_ENG_ERROR_F("[AnimationAsset] Invalid magic in '%s'", path.c_str());
		return false;
	}
	if (header.Version != TnxAnimVersion)
	{
		LOG_ENG_ERROR_F("[AnimationAsset] Unsupported version %u in '%s'", header.Version, path.c_str());
		return false;
	}

	outAsset.duration      = header.Duration;
	outAsset.boneCount     = header.BoneCount;
	outAsset.hasRootMotion = header.HasRootMotion != 0;
	outAsset.boneTracks.resize(header.BoneCount);
	outAsset.keyframes.resize(header.KeyframeCount);
	outAsset.notifies.resize(header.NotifyCount);

	for (AnimBoneTrack& t : outAsset.boneTracks)
	{
		AnimBoneTrackDisk d;
		file.read(reinterpret_cast<char*>(&d), sizeof(d));
		t = {d.keyframeOffset, d.keyframeCount};
	}

	for (AnimKeyframe& k : outAsset.keyframes)
	{
		AnimKeyframeDisk d;
		file.read(reinterpret_cast<char*>(&d), sizeof(d));
		k = {d.time, d.tx, d.ty, d.tz, d.rx, d.ry, d.rz, d.rw};
	}

	for (AnimNotifyDef& n : outAsset.notifies)
	{
		AnimNotifyDisk d;
		file.read(reinterpret_cast<char*>(&d), sizeof(d));
		n.id          = d.idHash;
		n.triggerTime = d.triggerTime;
		n.duration    = d.duration;
		n.name        = TnxName(d.nameHash, d.nameStr);
	}

	if (!file.good())
	{
		LOG_ENG_ERROR_F("[AnimationAsset] Read error for '%s'", path.c_str());
		outAsset = {};
		return false;
	}

	LOG_ENG_INFO_F("[AnimationAsset] Loaded '%s' (%.2fs, %u bones, %u keyframes, %u notifies)",
	               path.c_str(), header.Duration, header.BoneCount,
	               header.KeyframeCount, header.NotifyCount);
	return true;
}
