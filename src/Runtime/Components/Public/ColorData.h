#pragma once

#include <cstdint>

// ColorData Component - RGBA color for rendering
// Aligned to 16 bytes for GPU upload
struct alignas(16) ColorData
{
    float R = 1.0f;
    float G = 1.0f;
    float B = 1.0f;
    float A = 1.0f;
};

static_assert(sizeof(ColorData) == 16, "ColorData must be 16 bytes");
static_assert(alignof(ColorData) == 16, "ColorData must be 16-byte aligned");
