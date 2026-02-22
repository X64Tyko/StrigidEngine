#pragma once

#include "ComponentView.h"
#include "SchemaReflector.h"

// Transform Component - Position, Rotation, Scale
// Aligned to 16 bytes for SIMD/GPU upload
template <bool MASK = false>
struct Transform : public ComponentView<Transform<MASK>, MASK>
{
    STRIGID_HOT_COMPONENT()
    
    Transform::FloatProxy PositionX;
    Transform::FloatProxy PositionY;
    Transform::FloatProxy PositionZ;

    Transform::FloatProxy RotationX; // Euler angles for now
    Transform::FloatProxy RotationY;
    Transform::FloatProxy RotationZ;

    Transform::FloatProxy ScaleX;
    Transform::FloatProxy ScaleY;
    Transform::FloatProxy ScaleZ;

    STRIGID_REGISTER_FIELDS(Transform, PositionX, PositionY, PositionZ, RotationX, RotationY, RotationZ, ScaleX, ScaleY, ScaleZ)
};

STRIGID_REGISTER_COMPONENT(Transform)
