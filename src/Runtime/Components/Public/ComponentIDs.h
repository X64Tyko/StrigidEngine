#pragma once

#include "Types.h"

// Manual component type ID registration
// Week 5 will replace this with automatic reflection
namespace ComponentIDs
{
    constexpr ComponentTypeID Transform = 0;
    constexpr ComponentTypeID Velocity = 1;
    constexpr ComponentTypeID ColorData = 2;
}
