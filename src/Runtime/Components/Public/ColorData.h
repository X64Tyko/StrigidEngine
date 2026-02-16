#pragma once
#include "SoAComponent.h"
#include <array>
#include <cstdint>

// ColorData Component - RGBA color for rendering
// Aligned to 16 bytes for GPU upload
// Inherits from SoAComponent for automatic field decomposition
struct alignas(16) ColorData : SoAComponent<ColorData>
{
    float R = 1.0f;
    float G = 1.0f;
    float B = 1.0f;
    float A = 1.0f;

    // Define all fields for automatic SoA decomposition
    static constexpr auto DefineFields()
    {
        return std::make_tuple(
            &ColorData::R, &ColorData::G, &ColorData::B, &ColorData::A
        );
    }

    // Field names for debugging
    static constexpr std::array<const char*, 4> FieldNames = {
        "R", "G", "B", "A"
    };
};

static_assert(sizeof(ColorData) == 16, "ColorData must be 16 bytes");
static_assert(alignof(ColorData) == 16, "ColorData must be 16-byte aligned");

// Auto-register fields during static initialization
STRIGID_REGISTER_COMPONENT_FIELDS(ColorData)
