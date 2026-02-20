#pragma once

#include "Transform.h"
#include "Velocity.h"
#include "ColorData.h"
#include "EntityView.h"
#include "Schema.h"
#include "SchemaReflector.h"

// Simple test struct (will be replaced with real components in Week 4)
STRIGID_REGISTER_ENTITY(TestEntity);

class TestEntity : public EntityView<TestEntity>
{
    Transform Transform;
    Velocity Velocity;

    // Reflection - register components and lifecycle functions
public:
    static constexpr auto DefineSchema()
    {
        return Schema::Create(
            &TestEntity::Transform,
            &TestEntity::Velocity
        );
    }

    void Update([[maybe_unused]] double dt)
    {
    }
};
