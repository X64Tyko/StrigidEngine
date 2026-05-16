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

    void InitializeViews()
    {
        Body.Initialize(this);

        Vector3 spawnPos{ SpawnPosX, SpawnPosY, SpawnPosZ };
        Body.SetPosition(spawnPos);
        Body.Transform.Rotation.SetIdentity();
        Body.VisTransform.VisBlend = SimFloat(1.f);

        const BrainStemAssets& assets = GetAssets();
        if (!assets.Loaded)
            LoadBrainStemAssets(AssetRegistry::Get().GetContentRoot(),
                                const_cast<BrainStemAssets&>(assets));

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
        const uint32_t animID = GetAssets().AnimSlot;
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
        LOG_INFO_F("Anim ticking! time: %f", t.ToFloat());
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
};
