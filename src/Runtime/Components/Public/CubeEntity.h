#pragma once

#include "Transform.h"
#include "Velocity.h"
#include "ColorData.h"

// Simple test entity for Week 4 rendering
// Week 6 will add Ref<T> auto-wiring
struct CubeEntity
{
    Transform transform;
    Velocity velocity;
    ColorData color;
};
