#pragma once

#include "Transform.h"
#include "ColorData.h"
#include "EntityView.h"
#include "Schema.h"
#include "SchemaReflector.h"
#include "FieldProxy.h"

template <typename T, bool MASK = false>
class BaseCube : public EntityView<T, MASK>
{
    using BaseCubeSuper = EntityView<T, MASK>;
public:
    // We may still want a base class for these, just so it's less easy to use a non-SoA compliant component
    Transform<MASK> transform;
    //Velocity<MASK> velocity;
    ColorData<MASK> color;

    // Lifecycle hooks
    __forceinline void PrePhysics([[maybe_unused]] double dt)
    {
        constexpr float TWO_PI = 6.283185307179586f;

        // Now we emulate less than ideal assignment and operations in a fixed update.
        transform.PositionX += static_cast<float>(dt);// * velocity.vX;
        //transform.PositionY += static_cast<float>(dt) * velocity.vY;

        //velocity.vX *= pow(0.98f, dt);
        //velocity.vY *= pow(0.99f, dt);

        //float random = velocity.vX + velocity.vY; //static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
        transform.RotationY += static_cast<float>(dt) * 0.7f;
        //if (transform.RotationY > TWO_PI)[[unlikely]] transform.RotationY -= TWO_PI;

        //random = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
        transform.RotationZ += static_cast<float>(dt) * 0.6f;
        //if (transform.RotationZ > TWO_PI)[[unlikely]] transform.RotationZ -= TWO_PI;
    }

    STRIGID_REGISTER_SCHEMA(BaseCube, BaseCubeSuper, transform, color)
};

template<bool MASK = false>
class CubeEntity : public BaseCube<CubeEntity<MASK>, MASK>
{
    using CubeEntitySuper = BaseCube<CubeEntity<MASK>, MASK>;

public:
    using MaskedType = CubeEntity<true>;

    STRIGID_REGISTER_SCHEMA(CubeEntity, CubeEntitySuper)
};
STRIGID_REGISTER_ENTITY(CubeEntity)

template<bool MASK = false>
class SuperCube : public BaseCube<SuperCube<MASK>, MASK>
{
    using SuperCubeSuper = BaseCube<SuperCube<MASK>, MASK>;
public:
    using MaskedType = CubeEntity<true>;
    // Logic
    __forceinline void PrePhysics([[maybe_unused]] double dt)
    {
        constexpr float TWO_PI = 6.283185307179586f;

        this->transform.RotationX += static_cast<float>(dt);
        if (this->transform.RotationX > TWO_PI)[[unlikely]] this->transform.RotationX -= TWO_PI;

        this->transform.RotationY += static_cast<float>(dt) * 0.7f;
        if (this->transform.RotationY > TWO_PI)[[unlikely]] this->transform.RotationY -= TWO_PI;

        this->transform.RotationZ += static_cast<float>(dt) * 0.5f;
        if (this->transform.RotationZ > TWO_PI)[[unlikely]] this->transform.RotationZ -= TWO_PI;
    }

    STRIGID_REGISTER_SCHEMA(SuperCube, SuperCubeSuper)
};
STRIGID_REGISTER_ENTITY(SuperCube)
