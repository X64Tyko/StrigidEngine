#pragma once

// Transform Component - Position, Rotation, Scale
// Aligned to 16 bytes for SIMD/GPU upload
struct alignas(16) Transform
{
    float PositionX = 0.0f;
    float PositionY = 0.0f;
    float PositionZ = 0.0f;
    float _pad0 = 0.0f;  // Padding for alignment
    
    float RotationX = 0.0f;  // Euler angles for now
    float RotationY = 0.0f;
    float RotationZ = 0.0f;
    float _pad1 = 0.0f;
    
    float ScaleX = 1.0f;
    float ScaleY = 1.0f;
    float ScaleZ = 1.0f;
    float _pad2 = 0.0f;
};

static_assert(sizeof(Transform) == 48, "Transform must be 48 bytes");
static_assert(alignof(Transform) == 16, "Transform must be 16-byte aligned");
