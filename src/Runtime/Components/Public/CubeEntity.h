#pragma once

#include "Transform.h"
#include "ColorData.h"
#include "EntityView.h"
#include "Schema.h"
#include "SchemaReflector.h"
#include "SoARef.h"

template <typename T>
class BaseCube : public EntityView<T>
{
public:
    // Use SoARef instead of Ref for field-decomposed components
    SoARef<Transform> transform;
    SoARef<ColorData> color;

    // Lifecycle hooks
    __forceinline void PrePhysics([[maybe_unused]] double dt)
    {
        constexpr float TWO_PI = 6.283185307179586f;

        // SoARef uses operator->() which returns accessor with FieldProxy
        // transform->RotationX returns FieldProxy<float>
        // Operator += is defined on FieldProxy, so this works naturally!
        transform->PositionX += static_cast<float>(dt);
        if (transform->RotationX > TWO_PI) transform->RotationX -= TWO_PI;

        transform->RotationY += static_cast<float>(dt) * 0.7f;
        if (transform->RotationY > TWO_PI) transform->RotationY -= TWO_PI;

        transform->RotationZ += static_cast<float>(dt) * 0.5f;
        if (transform->RotationZ > TWO_PI) transform->RotationZ -= TWO_PI;
    }

    // Reflection - register components and lifecycle functions
    static constexpr auto DefineSchema()
    {
        return EntityView<T>::DefineSchema().Extend(
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
    __forceinline void PrePhysics([[maybe_unused]] double dt)
    {
        constexpr float TWO_PI = 6.283185307179586f;

        transform->RotationX += static_cast<float>(dt);
        if (transform->RotationX > TWO_PI) transform->RotationX -= TWO_PI;

        transform->RotationY += static_cast<float>(dt) * 0.7f;
        if (transform->RotationY > TWO_PI) transform->RotationY -= TWO_PI;

        transform->RotationZ += static_cast<float>(dt) * 0.5f;
        if (transform->RotationZ > TWO_PI) transform->RotationZ -= TWO_PI;
    }

    // Reflection
    static constexpr auto DefineSchema()
    {
        return BaseCube<SuperCube>::DefineSchema();
    }
};
