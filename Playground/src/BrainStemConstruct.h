#pragma once
#include "AnimConstruct.h"
#include "AnimationAsset.h"
#include "AnimationManager.h"
#include "AssetRegistry.h"
#include "AssetTypes.h"
#include "Construct.h"
#include "ConstructView.h"
#include "EPlayerCharacter.h"
#include "Logger.h"
#include "MeshManager.h"
#include "SimFloat.h"
#include "SkeletonAsset.h"
#include "SkeletonManager.h"

// BrainStemConstruct — display-only skeletal character that loops its imported animation.
//
// Inherits AnimConstruct for per-instance bone cache and notify state.
// PostPhysics drives CAnimBase directly — no separate companion AnimBP Construct,
// so N instances produce N PostPhysics ticks but zero extra Construct registrations.
class BrainStemConstruct : public AnimConstruct, public Construct<BrainStemConstruct>
{
public:
    TNX_CONSTRUCT_WORLD

    SimFloat SpawnPosX = SimFloat(0.f);
    SimFloat SpawnPosY = SimFloat(0.f);
    SimFloat SpawnPosZ = SimFloat(-4.f);

    ConstructView<EPlayerCharacter> Body;

    // Eagerly load BrainStem skeleton + animation from disk.
    // Call before any BrainStemConstruct is spawned to avoid blocking the spawn handshake.
    static void PreloadAssets()
    {
        BrainStemAssets& assets = GetAssets();
        if (!assets.Loaded)
            LoadBrainStemAssets(AssetRegistry::Get().GetContentRoot(), assets);
    }

    void InitializeViews()
    {
        Body.Initialize(this);

        Vector3 spawnPos{ SpawnPosX, SpawnPosY, SpawnPosZ };
        Body.SetPosition(spawnPos);
        Body.Transform.Rotation.SetIdentity();
        Body.VisTransform.VisBlend = SimFloat(1.f);

        const BrainStemAssets& assets = GetAssets();
        if (!assets.Loaded)
        {
            LOG_WARN("[BrainStemConstruct] Assets not preloaded — call PreloadAssets() before spawning");
            return;
        }

        if (assets.SkelSlot != UINT32_MAX)
        {
            Body.SkeletonRef.SkeletonID = assets.SkelSlot;
            Body.SkeletonRef.SkinMeshID = assets.MeshSlot;
        }

        if (assets.SkelSlot != UINT32_MAX)
            RegisterSockets(assets.SkelSlot);
    }

    void PostPhysics(SimFloat dt)
    {
        const BrainStemAssets& assets = GetAssets();
        const uint32_t animID = assets.AnimSlot;
        if (animID == UINT32_MAX) return;
        BeginAnimTick();

        auto& animBase = Body.AnimBase;

        if (animBase.BaseAnimID.Value() == 0)
        {
            animBase.SetExplicitAnim(animID, SimFloat(0.f), true);
            return;
        }

        const float dur = AnimationManager::Get().GetDuration(animID);
        SimFloat t = animBase.GetBaseTimestamp();
        if (WrapTimestamp(t, dur, true))
        {
            NotifyState.ClearLoopedRecords(0, SimFloat(dur));
            animBase.SetBaseTimestamp(t);
        }

        DebugAnimState(assets, animID, t, dur);
    }

    void InitializeForReplication(WorldBase* world,
                                  [[maybe_unused]] EntityHandle* viewHandles,
                                  [[maybe_unused]] uint8_t viewCount)
    {
        Initialize(world);
    }

private:

    struct BrainStemAssets
    {
        uint32_t MeshSlot = 0;
        uint32_t SkelSlot = UINT32_MAX;
        uint32_t AnimSlot = UINT32_MAX;
        bool     Loaded   = false;
    };

    static BrainStemAssets& GetAssets()
    {
        static BrainStemAssets s;
        return s;
    }

    static void LoadBrainStemAssets(const std::string& root, BrainStemAssets& out)
    {
        {
            const AssetEntry* e = AssetRegistry::Get().FindByTName(TnxName("BrainStem_mesh"));
            if (e && e->Type == AssetType::StaticMesh && e->Data)
                out.MeshSlot = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(e->Data));
            else
                LOG_WARN("[BrainStemConstruct] BrainStem mesh slot not found — has it been imported?");
        }

        out.SkelSlot = SkeletonManager::Get().FindSlotByTName(TnxName("BrainStem_skel"));
        if (out.SkelSlot == UINT32_MAX)
        {
            SkeletonAsset skelAsset;
            if (LoadSkeletonAsset(skelAsset, root + "/BrainStem.tnxskel"))
                out.SkelSlot = SkeletonManager::Get().LoadSkeleton(skelAsset, TnxName("BrainStem_skel"));
            else
                LOG_WARN("[BrainStemConstruct] Failed to load BrainStem.tnxskel");
        }

        out.AnimSlot = AnimationManager::Get().FindSlotByTName(TnxName("BrainStem_anim"));
        if (out.AnimSlot == UINT32_MAX)
        {
            AnimationAsset animAsset;
            if (LoadAnimationAsset(animAsset, root + "/BrainStem_anim.tnxanim"))
                out.AnimSlot = AnimationManager::Get().LoadAnimation(animAsset, TnxName("BrainStem_anim"));
            else
                LOG_WARN("[BrainStemConstruct] Failed to load BrainStem_anim.tnxanim");
        }

        if (out.SkelSlot != UINT32_MAX && out.AnimSlot != UINT32_MAX)
            LOG_INFO("[BrainStemConstruct] BrainStem assets ready");

        out.Loaded = true;
    }
	static constexpr uint32_t kDebugLogInterval = 512; // 512Hz → ~1 log/sec

	uint32_t DebugTickCount = 0;
	bool     SkeletonDumped = false;

	void DebugAnimState(const BrainStemAssets& assets, uint32_t animID, SimFloat t, float dur)
	{
		++DebugTickCount;

		if (!SkeletonDumped && assets.SkelSlot != UINT32_MAX)
		{
			const SkeletonAsset* skel = SkeletonManager::Get().GetSkeletonCPU(assets.SkelSlot);
			if (skel)
			{
				LOG_INFO_F("[BrainStem] Skeleton slot=%u  boneCount=%u", assets.SkelSlot, skel->boneCount);
				for (uint32_t i = 0; i < skel->boneCount; ++i)
					LOG_INFO_F("[BrainStem]   bone[%2u] %-32s  parent=%u",
							   i, skel->bones[i].name.GetStr(), skel->bones[i].parentIndex);
				SkeletonDumped = true;
			}
		}

		if (DebugTickCount % kDebugLogInterval != 0) return;

		const float tF = t.ToFloat();
		LOG_INFO_F("[BrainStem] animSlot=%u  t=%.3f / %.3f s  (%.1f%%)",
				   animID, tF, dur, dur > 0.f ? 100.f * tF / dur : 0.f);

		// Update kTrackedBone after reading the skeleton dump to pick an animated bone.
		static constexpr uint32_t kTrackedBone = 1;
		const AnimationAsset* anim = AnimationManager::Get().GetAnimCPU(animID);
		if (anim && kTrackedBone < anim->boneCount)
		{
			BoneTransform b = anim->EvaluateBone(kTrackedBone, tF);
			LOG_INFO_F("[BrainStem]   bone[%u] tx=%.4f ty=%.4f tz=%.4f  rx=%.4f ry=%.4f rz=%.4f rw=%.4f",
					   kTrackedBone,
					   b.tx.ToFloat(), b.ty.ToFloat(), b.tz.ToFloat(),
					   b.rx.ToFloat(), b.ry.ToFloat(), b.rz.ToFloat(), b.rw.ToFloat());
		}
		else if (anim)
		{
			LOG_WARN_F("[BrainStem] kTrackedBone=%u out of range (boneCount=%u)", kTrackedBone, anim->boneCount);
		}
	}
};
