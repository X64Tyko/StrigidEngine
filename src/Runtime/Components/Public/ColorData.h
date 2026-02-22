#pragma once
#include <FieldProxy.h>
#include "ComponentView.h"
#include "SchemaReflector.h"

// ColorData Component - RGBA color for rendering
// Aligned to 32 bytes for GPU upload
template<bool MASK = false>
struct ColorData : public ComponentView<ColorData<MASK>, MASK>
{
    ColorData::FloatProxy R;
    ColorData::FloatProxy G;
    ColorData::FloatProxy B;
    ColorData::FloatProxy A;

    // Register Proxy values and Component struct
    STRIGID_REGISTER_FIELDS(ColorData, R, G, B, A)
};
STRIGID_REGISTER_COMPONENT(ColorData)