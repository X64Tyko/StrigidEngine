#pragma once

#include "Transform.h"
#include "Velocity.h"
#include "ColorData.h"
#include "EntityView.h"
#include "Schema.h"
#include "SchemaReflector.h"

template <typename T>
class BaseCube : public EntityView<T>
{
public:
    Ref<Transform> transform;
    Ref<ColorData> color;

    // Lifecycle hooks
    void PrePhysics([[maybe_unused]] double dt) {
        constexpr float TWO_PI = 6.283185307179586f;

        transform->RotationX += (float)dt;
        if (transform->RotationX > TWO_PI) transform->RotationX -= TWO_PI;
        transform->RotationY += (float)dt * 0.7f;
        if (transform->RotationY > TWO_PI) transform->RotationY -= TWO_PI;
        transform->RotationZ += (float)dt * 0.5f;
        if (transform->RotationZ > TWO_PI) transform->RotationZ -= TWO_PI;
    }

    // Reflection - register components and lifecycle functions
    static constexpr auto DefineSchema() {
        return Schema::Create(
            &BaseCube::transform,
            &BaseCube::color
        );
    }
};

STRIGID_REGISTER_ENTITY(CubeEntity);
class CubeEntity : public BaseCube<CubeEntity>
{
public:
    static constexpr auto DefineSchema()
    {
        return BaseCube<CubeEntity>::DefineSchema();
    }
};

STRIGID_REGISTER_ENTITY(SuperCube);
class SuperCube : public BaseCube<SuperCube>
{
public:
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
    static constexpr auto DefineSchema() {
        return BaseCube<SuperCube>::DefineSchema();
    }
};
