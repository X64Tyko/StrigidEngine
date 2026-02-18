#pragma once
#include "SoAComponent.h"
#include <array>

// Transform Component - Position, Rotation, Scale
// Aligned to 16 bytes for SIMD/GPU upload
// Inherits from SoAComponent for automatic field decomposition
struct alignas(16) Transform : SoAComponent<Transform>
{
    STRIGID_HOT_COMPONENT()
    float PositionX = 0.0f;
    float PositionY = 0.0f;
    float PositionZ = 0.0f;
    float _pad0 = 0.0f; // Padding for alignment

    float RotationX = 0.0f; // Euler angles for now
    float RotationY = 0.0f;
    float RotationZ = 0.0f;
    float _pad1 = 0.0f;

    float ScaleX = 1.0f;
    float ScaleY = 1.0f;
    float ScaleZ = 1.0f;
    float _pad2 = 0.0f;

    // Define all fields for automatic SoA decomposition
    static constexpr auto DefineFields()
    {
        return std::make_tuple(
            &Transform::PositionX, &Transform::PositionY, &Transform::PositionZ, &Transform::_pad0,
            &Transform::RotationX, &Transform::RotationY, &Transform::RotationZ, &Transform::_pad1,
            &Transform::ScaleX, &Transform::ScaleY, &Transform::ScaleZ, &Transform::_pad2
        );
    }

    // Field names for debugging
    static constexpr std::array<const char*, 12> FieldNames = {
        "PositionX", "PositionY", "PositionZ", "_pad0",
        "RotationX", "RotationY", "RotationZ", "_pad1",
        "ScaleX", "ScaleY", "ScaleZ", "_pad2"
    };
};

static_assert(sizeof(Transform) == 48, "Transform must be 48 bytes");
static_assert(alignof(Transform) == 16, "Transform must be 16-byte aligned");

// Auto-register fields during static initialization
STRIGID_REGISTER_COMPONENT_FIELDS(Transform)
