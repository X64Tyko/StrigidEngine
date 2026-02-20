#pragma once
#include <FieldProxy.h>
#include "SchemaReflector.h"

// Velocity Component - Linear velocity for movement
// Aligned to 16 bytes for SIMD operations
struct alignas(16) Velocity
{
    FieldProxy<float> vX;
    FieldProxy<float> vY;
    FieldProxy<float> vZ;

    STRIGID_REGISTER_FIELDS(Velocity, vX, vY, vZ)
};
STRIGID_REGISTER_COMPONENT(Velocity)
static_assert(alignof(Velocity) == 16, "Velocity must be 16-byte aligned");
