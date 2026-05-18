#pragma once
#include "ComponentView.h"
#include "SchemaReflector.h"
#include "NetDelta.h"
#include "SimFloat.h"

// CAnimLayer — overlay animation layers with bone masking.
// Temporal: rollback-capable.
//
// Provides two slots for masked overlays (replace or additive) applied on top of CAnimBase.
// Typical usage:
//   Slot 0 — upper body weapon animation (replace, LayerID = "UpperBody" index in skeleton)
//   Slot 1 — aim offset (additive, LayerID = "Spine" index)
//
// LayerConfig packing per slot:
//   bits 0-7  : skeleton named layer ID — indexes SkeletonAsset::layers[] for the bone mask.
//               Bone masks are authored in the skeleton asset, not stored here.
//   bit 8     : blend mode (0 = replace masked bones, 1 = additive)
//   bit 9     : loop
//
// LayerTimestamp for each slot is advanced unconditionally by the wide sweep.
// AnimConstruct handles loop wrapping in its scalar PostPhysics pass.

template <FieldWidth WIDTH = FieldWidth::Scalar>
struct CAnimLayer : ComponentView<CAnimLayer, WIDTH>
{
    TNX_TEMPORAL_FIELDS(CAnimLayer, Logic,
        LayerAnimID0,    LayerAnimID1,
        LayerTimestamp0, LayerTimestamp1,
        LayerAlpha0,     LayerAlpha1,
        LayerConfig0,    LayerConfig1)

    UIntProxy<WIDTH>  LayerAnimID0{},    LayerAnimID1{};
    FloatProxy<WIDTH> LayerTimestamp0{}, LayerTimestamp1{};
    FloatProxy<WIDTH> LayerAlpha0{},     LayerAlpha1{};
    UIntProxy<WIDTH>  LayerConfig0{},    LayerConfig1{};

    static constexpr uint32_t Slots = 2;

    // -----------------------------------------------------------------------
    // LayerConfig packing helpers
    // -----------------------------------------------------------------------

    static uint32_t MakeConfig(uint8_t layerID, bool additive, bool loops)
    {
        return uint32_t(layerID) | (additive ? (1u << 8) : 0u) | (loops ? (1u << 9) : 0u);
    }
    static uint8_t GetLayerID  (uint32_t cfg) { return uint8_t(cfg & 0xFF); }
    static bool    IsAdditive  (uint32_t cfg) { return (cfg >> 8) & 1u; }
    static bool    GetLoop     (uint32_t cfg) { return (cfg >> 9) & 1u; }

    // -----------------------------------------------------------------------
    // Scalar SimFloat accessors
    // -----------------------------------------------------------------------

    SimFloat GetTimestamp(uint32_t slot) const requires (WIDTH == FieldWidth::Scalar)
    {
        return SimFloat(slot == 0 ? LayerTimestamp0.Value() : LayerTimestamp1.Value());
    }

    SimFloat GetAlpha(uint32_t slot) const requires (WIDTH == FieldWidth::Scalar)
    {
        return SimFloat(slot == 0 ? LayerAlpha0.Value() : LayerAlpha1.Value());
    }

    uint32_t GetAnimID(uint32_t slot) const requires (WIDTH == FieldWidth::Scalar)
    {
        return slot == 0 ? LayerAnimID0.Value() : LayerAnimID1.Value();
    }

    uint32_t GetConfig(uint32_t slot) const requires (WIDTH == FieldWidth::Scalar)
    {
        return slot == 0 ? LayerConfig0.Value() : LayerConfig1.Value();
    }

    // -----------------------------------------------------------------------
    // Scalar write helpers
    // -----------------------------------------------------------------------

    void SetLayer(uint32_t slot, uint32_t animID, SimFloat startTime,
                  SimFloat alpha, uint8_t layerID, bool additive, bool loops)
        requires (WIDTH == FieldWidth::Scalar)
    {
        const uint32_t cfg = MakeConfig(layerID, additive, loops);
        if (slot == 0) {
            LayerAnimID0    = animID;       LayerTimestamp0 = startTime.ToFloat();
            LayerAlpha0     = alpha.ToFloat(); LayerConfig0  = cfg;
        } else {
            LayerAnimID1    = animID;       LayerTimestamp1 = startTime.ToFloat();
            LayerAlpha1     = alpha.ToFloat(); LayerConfig1  = cfg;
        }
    }

    void SetTimestamp(uint32_t slot, SimFloat t) requires (WIDTH == FieldWidth::Scalar)
    {
        if (slot == 0) LayerTimestamp0 = t;
        else           LayerTimestamp1 = t;
    }

    void SetAlpha(uint32_t slot, SimFloat alpha) requires (WIDTH == FieldWidth::Scalar)
    {
        if (slot == 0) LayerAlpha0 = alpha;
        else           LayerAlpha1 = alpha;
    }

    void ClearSlot(uint32_t slot) requires (WIDTH == FieldWidth::Scalar)
    {
        if (slot == 0) { LayerAnimID0 = 0u; LayerTimestamp0 = SimFloat(0.f); LayerAlpha0 = SimFloat(0.f); LayerConfig0 = 0u; }
        else           { LayerAnimID1 = 0u; LayerTimestamp1 = SimFloat(0.f); LayerAlpha1 = SimFloat(0.f); LayerConfig1 = 0u; }
    }

    void Clear() requires (WIDTH == FieldWidth::Scalar)
    {
        LayerAnimID0 = 0u; LayerTimestamp0 = SimFloat(0.f); LayerAlpha0 = SimFloat(0.f); LayerConfig0 = 0u;
        LayerAnimID1 = 0u; LayerTimestamp1 = SimFloat(0.f); LayerAlpha1 = SimFloat(0.f); LayerConfig1 = 0u;
    }
};

TNX_REGISTER_COMPONENT(CAnimLayer)
TNX_NET_TEMPORAL(CAnimLayer)