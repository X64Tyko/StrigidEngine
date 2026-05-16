#pragma once
#include "AnimConstruct.h"
#include "AnimationManager.h"
#include "Construct.h"
#include "ConstructView.h"
#include "EPlayerCharacter.h"

// BrainStemAnimBP — animation state machine for the BrainStem skeletal mesh.
//
// Owned by BrainStemConstruct. Loops BrainStem_anim indefinitely.
// Body is Attached (not owned) to the same entity as BrainStemConstruct::Body,
// so HydrateAllViews() in PostPhysBase keeps its cursors current each tick.
class BrainStemAnimBP : public AnimConstruct, public Construct<BrainStemAnimBP>
{
public:
    TNX_CONSTRUCT_WORLD

    ConstructView<EPlayerCharacter> Body;
    uint32_t AnimID = 0;

    void PostPhysics(SimFloat dt)
    {
        if (!Body.IsInitialized() || AnimID == 0) return;
        BeginAnimTick();

        auto& animBase = Body.AnimBase;

        if (animBase.BaseAnimID.Value() == 0)
        {
            animBase.SetExplicitAnim(AnimID, SimFloat(0.f), true);
            return;
        }

        const float dur = AnimationManager::Get().GetDuration(AnimID);
        SimFloat t = animBase.GetBaseTimestamp() + dt;
        if (WrapTimestamp(t, dur, true))
            NotifyState.ClearLoopedRecords(0, SimFloat(dur));
        animBase.SetBaseTimestamp(t);

        LOG_INFO_F("Anim ticking! time: %f", t.ToFloat());
    }
};
