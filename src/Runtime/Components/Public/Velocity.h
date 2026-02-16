#pragma once
#include "SoAComponent.h"
#include <array>

// Velocity Component - Linear velocity for movement
// Aligned to 16 bytes for SIMD operations
// Inherits from SoAComponent for automatic field decomposition
struct alignas(16) Velocity : SoAComponent<Velocity>
{
    float vX = 0.0f;
    float vY = 0.0f;
    float vZ = 0.0f;
    float _pad = 0.0f; // Padding for alignment

    // Define all fields for automatic SoA decomposition
    static constexpr auto DefineFields()
    {
        return std::make_tuple(
            &Velocity::vX, &Velocity::vY, &Velocity::vZ, &Velocity::_pad
        );
    }

    // Field names for debugging
    static constexpr std::array<const char*, 4> FieldNames = {
        "X", "Y", "Z", "_pad"
    };
};

static_assert(sizeof(Velocity) == 16, "Velocity must be 16 bytes");
static_assert(alignof(Velocity) == 16, "Velocity must be 16-byte aligned");

// Auto-register fields during static initialization
STRIGID_REGISTER_COMPONENT_FIELDS(Velocity)
