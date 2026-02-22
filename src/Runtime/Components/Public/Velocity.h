#pragma once
#include <FieldProxy.h>
#include "ComponentView.h"
#include "SchemaReflector.h"

// Velocity Component - Linear velocity for movement
// Aligned to 16 bytes for SIMD operations
template<bool MASK = false>
struct Velocity : public ComponentView<Velocity<MASK>, MASK>
{
    Velocity::FloatProxy vX;
    Velocity::FloatProxy vY;
    Velocity::FloatProxy vZ;

    STRIGID_REGISTER_FIELDS(Velocity, vX, vY, vZ)
};
STRIGID_REGISTER_COMPONENT(Velocity)
