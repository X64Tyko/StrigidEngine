#pragma once
#include <FieldProxy.h>
#include "SchemaReflector.h"

// ColorData Component - RGBA color for rendering
// Aligned to 16 bytes for GPU upload
struct alignas(16) ColorData
{
    FieldProxy<float> R;
    FieldProxy<float> G;
    FieldProxy<float> B;
    FieldProxy<float> A;

    // Register Proxy values and Component struct
    STRIGID_REGISTER_FIELDS(ColorData, R, G, B, A)
};
STRIGID_REGISTER_COMPONENT(ColorData)
static_assert(alignof(ColorData) == 16, "ColorData must be 16-byte aligned");
