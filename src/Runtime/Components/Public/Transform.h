#pragma once
#include <FieldProxy.h>
#include "SchemaReflector.h"

// Transform Component - Position, Rotation, Scale
// Aligned to 16 bytes for SIMD/GPU upload
struct alignas(16) Transform
{
    STRIGID_HOT_COMPONENT()
    FieldProxy<float> PositionX;
    FieldProxy<float> PositionY;
    FieldProxy<float> PositionZ;

    FieldProxy<float> RotationX; // Euler angles for now
    FieldProxy<float> RotationY;
    FieldProxy<float> RotationZ;

    FieldProxy<float> ScaleX;
    FieldProxy<float> ScaleY;
    FieldProxy<float> ScaleZ;

    // This closes out the struct, not sure I like that, makes it easy to mess up trying to add functions below.
    STRIGID_REGISTER_FIELDS(Transform, PositionX, PositionY, PositionZ, RotationX, RotationY, RotationZ, ScaleX, ScaleY, ScaleZ)
};

STRIGID_REGISTER_COMPONENT(Transform)
static_assert(alignof(Transform) == 16, "Transform must be 16-byte aligned");
