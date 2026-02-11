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
    void Update([[maybe_unused]] double dt) {
        //transform->RotationX += (float)dt;
        //transform->RotationY += (float)dt * 0.7f;
        //transform->RotationZ += (float)dt * 0.5f;
        //color->R = color->B;
        //color->B = (color->B == 1.0f ? 0.0f : 1.0f);
        //LOG_ALWAYS_F("Color: R: %f, G: %f, B: %f, A: %f", color->R, color->G, color->B, color->A);
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
    void FixedUpdate([[maybe_unused]] double dt) {
        constexpr float TWO_PI = 6.283185307179586f;

        transform->RotationX += (float)dt;
        if (transform->RotationX > TWO_PI) transform->RotationX -= TWO_PI;
        transform->RotationY += (float)dt * 0.7f;
        if (transform->RotationY > TWO_PI) transform->RotationY -= TWO_PI;
        transform->RotationZ += (float)dt * 0.5f;
        if (transform->RotationZ > TWO_PI) transform->RotationZ -= TWO_PI;
        //color->R = color->B;
        //color->B = (color->B == 1.0f ? 0.0f : 1.0f);
    }

    // Reflection
public:
    static constexpr auto DefineSchema() {
        return CubeEntity::DefineSchema().Extend(&SuperCube::FixedUpdate);
    }
};

STRIGID_REGISTER_ENTITY(CubeEntity);
STRIGID_REGISTER_ENTITY(SuperCube);
