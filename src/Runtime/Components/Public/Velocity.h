#pragma once

// Velocity Component - Linear velocity for movement
// Aligned to 16 bytes for SIMD operations
struct alignas(16) Velocity
{
    float X = 0.0f;
    float Y = 0.0f;
    float Z = 0.0f;
    float _pad = 0.0f; // Padding for alignment
};

static_assert(sizeof(Velocity) == 16, "Velocity must be 16 bytes");
static_assert(alignof(Velocity) == 16, "Velocity must be 16-byte aligned");
