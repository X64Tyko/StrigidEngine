#pragma once

#include "Transform.h"
#include "Velocity.h"
#include "ColorData.h"
#include "Schema.h"
#include "SchemaReflector.h"

// Simple test entity for Week 4 rendering
// Week 6 will add Ref<T> auto-wiring
struct CubeEntity
{
    Ref<Transform> transform;
    Ref<Velocity> velocity;
    Ref<ColorData> color;

    // Lifecycle hooks
    void Update(double dt) {
        transform->RotationX += (float)dt;
        transform->RotationY += (float)dt * 0.7f;
        transform->RotationZ += (float)dt * 0.5f;
    }

    // Reflection - register components and lifecycle functions
public:
    static constexpr auto DefineSchema() {
        return Schema::Create(
            &CubeEntity::transform,
            &CubeEntity::velocity,
            &CubeEntity::color,
            &CubeEntity::Update
        );
    }
};

class SuperCube : public CubeEntity
{
    // Logic
    void Update(double dt) {
        transform->RotationX += (float)dt;
        transform->RotationY += (float)dt * 0.7f;
        transform->RotationZ += (float)dt * 0.5f;
    }

    // Reflection
public:
    static constexpr auto DefineSchema() {
        return CubeEntity::DefineSchema().Replace(&CubeEntity::Update,&SuperCube::Update);
    }
};

STRIGID_REGISTER_ENTITY(CubeEntity);
STRIGID_REGISTER_ENTITY(SuperCube);
