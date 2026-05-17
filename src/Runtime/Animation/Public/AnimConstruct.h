#pragma once
#include "AnimationAsset.h"
#include "AnimTypes.h"
#include "AssetRegistry.h"
#include "AssetTypes.h"
#include "CAnimBase.h"
#include "CAnimLayer.h"
#include "CSkeletonRef.h"
#include "SimFloat.h"
#include "SkeletonAsset.h"

// -----------------------------------------------------------------------
// AnimConstruct — base for animation state machine Constructs (AnimBP equivalent).
//
// One AnimConstruct type per character type; entities of that type carry the
// node position (StateNodeID, blend fields) in their slab. The AnimConstruct
// is a Construct<T> companion owned by the character Construct via Owned<T>.
//
// Tick registration follows the standard Construct auto-registration pattern:
//   PrePhysics  — apply root motion delta from the previous frame into CTransform
//   PostPhysics — run state machine, write CAnimBase/CAnimLayer, handle loop wrap
//
// Derive from AnimConstruct and Construct<T>:
//
//   class PlayerAnimBP : public AnimConstruct, public Construct<PlayerAnimBP>
//   {
//       void PrePhysics(SimFloat dt)  { ApplyRootMotion(dt); }
//       void PostPhysics(SimFloat dt) { RunLocomotionStateMachine(dt); }
//   private:
//       ConstructView<EPlayerCharacter>* Body; // wired up by owning Construct
//       SimFloat RootMotionDeltaX{}, RootMotionDeltaY{}, RootMotionDeltaZ{};
//   };
//
// CPU socket queries are available via GetSocketTransform() using the BoneCache.
// Call RegisterSockets() once after the skeleton is bound (e.g., in Initialize()).
// -----------------------------------------------------------------------

class AnimConstruct
{
public:
    // -----------------------------------------------------------------------
    // CPU socket query — lazily evaluates the bone chain via BoneCache.
    // Call RegisterSockets() first to populate SocketTransforms from the skeleton.
    // Must only be called from scalar (PostPhysics) context.
    // -----------------------------------------------------------------------

    BoneTransform GetSocketTransform(SocketID socket,
                                     const CAnimBase<FieldWidth::Scalar>&   animBase,
                                     const CAnimLayer<FieldWidth::Scalar>&  animLayer,
                                     uint32_t skeletonID)
    {
        SocketEntry* entry = SocketTransforms.FindSocket(socket);
        assert(entry && "Socket not registered — call RegisterSockets() after binding the skeleton");
        if (entry->valid) return entry->worldTransform;

        auto ref = AssetRegistry::Get().GetAssetData<SkeletonAsset>(AssetType::Skeleton, skeletonID);
        const SkeletonAsset* skel = ref.Get();
        if (!skel) return BoneTransform::Identity();

        ChainWalkResult walk = BoneCache.FindNearestCachedAncestor(entry->boneIndex, *skel);

        BoneTransform worldT = walk.ancestorTransform;
        for (uint32_t i = 0; i < walk.remainingBones; ++i)
        {
            uint32_t boneIdx = walk.chain[i];
            BoneTransform localT = EvaluateBlendedBone(boneIdx, animBase, animLayer);
            worldT = BoneTransform::Compose(worldT, localT);
            BoneCache.Insert(boneIdx, worldT);
        }

        entry->worldTransform = BoneTransform::Compose(worldT, entry->localOffset);
        entry->valid          = true;
        return entry->worldTransform;
    }

    // Slot-based overload — used when only a slot index is available.
    void RegisterSockets(uint32_t skeletonSlot)
    {
        RegisterSockets(AssetRegistry::Get().GetAssetData<SkeletonAsset>(AssetType::Skeleton, skeletonSlot));
    }

    // Ref-based overload — preferred during init when you already have a ref.
    void RegisterSockets(const AssetDataRef<SkeletonAsset>& skelRef)
    {
        const SkeletonAsset* skel = skelRef.Get();
        if (!skel) return;
        for (uint32_t i = 0; i < skel->socketCount; ++i)
        {
            const SocketDef& def = skel->sockets[i];
            SocketTransforms.RegisterSocket(def.id, def.boneIndex, def.localOffset);
        }
    }

protected:
    BoneCacheLocal       BoneCache;
    SocketTransformLocal SocketTransforms;
    AnimNotifyState      NotifyState;

    SimFloat RootMotionDeltaX{};
    SimFloat RootMotionDeltaY{};
    SimFloat RootMotionDeltaZ{};

    void BeginAnimTick()
    {
        BoneCache.Clear();
        SocketTransforms.Clear();
    }

    void StoreRootMotionDelta(uint32_t animID, SimFloat fromT, SimFloat toT)
    {
        RootMotionDeltaX = SimFloat{};
        RootMotionDeltaY = SimFloat{};
        RootMotionDeltaZ = SimFloat{};

        auto ref = AssetRegistry::Get().GetAssetData<AnimationAsset>(AssetType::Animation, animID);
        const AnimationAsset* anim = ref.Get();
        if (!anim || !anim->hasRootMotion) return;

        BoneTransform delta = anim->EvaluateRootMotionDelta(fromT.ToFloat(), toT.ToFloat());
        RootMotionDeltaX   = delta.tx;
        RootMotionDeltaY   = delta.ty;
        RootMotionDeltaZ   = delta.tz;
    }

    void ApplyRootMotion(FieldProxy<SimFloat, FieldWidth::Scalar>& posX,
                         FieldProxy<SimFloat, FieldWidth::Scalar>& posY,
                         FieldProxy<SimFloat, FieldWidth::Scalar>& posZ)
    {
        posX += RootMotionDeltaX;
        posY += RootMotionDeltaY;
        posZ += RootMotionDeltaZ;
    }

    static bool WrapTimestamp(SimFloat& inOutT, float duration, bool loops)
    {
        if (duration <= 0.f) return false;
        const SimFloat dur = SimFloat(duration);
        if (loops && inOutT >= dur)
        {
            inOutT = Fmod(inOutT, dur);
            return true;
        }
        if (!loops && inOutT > dur)
            inOutT = dur;
        return false;
    }

    template <typename Derived>
    void FireNotifies(Derived* self, uint32_t animID, uint32_t slot,
                      SimFloat fromT, SimFloat toT, SimFloat weight)
    {
        auto ref = AssetRegistry::Get().GetAssetData<AnimationAsset>(AssetType::Animation, animID);
        const AnimationAsset* anim = ref.Get();
        if (!anim) return;
        for (const AnimNotifyDef& n : anim->notifies)
        {
            if (SimFloat(n.triggerTime) < fromT || SimFloat(n.triggerTime) >= toT) continue;
            if (NotifyState.HasFiredThisLoop(n.id, slot, fromT)) continue;
            self->OnAnimNotify({n.id, weight, n.triggerTime});
            NotifyState.RecordFire(n.id, slot, SimFloat(n.triggerTime));
        }
    }

    static BoneTransform EvaluateBlendedBone(uint32_t boneIndex,
                                              const CAnimBase<FieldWidth::Scalar>&  animBase,
                                              const CAnimLayer<FieldWidth::Scalar>& animLayer)
    {
        BoneTransform acc{};
        SimFloat      totalW{};
        bool          any = false;

        auto Accumulate = [&](uint32_t animID, SimFloat t, SimFloat w)
        {
            if (animID == 0 || w <= SimFloat(0)) return;
            auto ref = AssetRegistry::Get().GetAssetData<AnimationAsset>(AssetType::Animation, animID);
            const AnimationAsset* anim = ref.Get();
            if (!anim) return;
            BoneTransform sample = anim->EvaluateBone(boneIndex, t.ToFloat());
            if (!any)
            {
                acc  = sample;
                acc.tx *= w; acc.ty *= w; acc.tz *= w;
                acc.rx *= w; acc.ry *= w; acc.rz *= w; acc.rw *= w;
                acc.sx *= w; acc.sy *= w; acc.sz *= w;
                totalW = w; any = true;
            }
            else
            {
                acc    = BoneTransform::WeightedAdd(acc, sample, w);
                totalW = totalW + w;
            }
        };

        const SimFloat fadeAlpha = animBase.GetFadeAlpha();
        const SimFloat baseW     = SimFloat(1) - fadeAlpha;

        if (!animBase.IsBlendspaceMode())
            Accumulate(animBase.BaseAnimID.Value(), animBase.GetBaseTimestamp(), baseW);

        if (animBase.HasFade())
            Accumulate(animBase.FadeAnimID.Value(), animBase.GetFadeTimestamp(), fadeAlpha);

        for (uint32_t s = 0; s < CAnimLayer<FieldWidth::Scalar>::Slots; ++s)
        {
            const uint32_t id = animLayer.GetAnimID(s);
            if (id == 0) continue;
            const uint32_t cfg = animLayer.GetConfig(s);
            if (!CAnimLayer<FieldWidth::Scalar>::IsAdditive(cfg))
                Accumulate(id, animLayer.GetTimestamp(s), animLayer.GetAlpha(s));
        }

        if (!any) return BoneTransform::Identity();
        return BoneTransform::Normalize(acc, totalW);
    }
};
