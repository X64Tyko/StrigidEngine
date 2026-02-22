#pragma once

#include "Transform.h"
#include "Velocity.h"
#include "ColorData.h"
#include "EntityView.h"
#include "Schema.h"
#include "SchemaReflector.h"

// Simple test struct (will be replaced with real components in Week 4)
template <bool MASK = false>
class TestEntity : public EntityView<TestEntity<MASK>, MASK>
{
using TestEntitySuper = EntityView<TestEntity<MASK>, MASK>;
    Transform<MASK> Transform;
    Velocity<MASK> Velocity;

    // Reflection - register components and lifecycle functions
public:
using MaskedType = TestEntity<true>;
    
    STRIGID_REGISTER_SCHEMA(TestEntity, TestEntitySuper, Transform, Velocity)

    void Update([[maybe_unused]] double dt)
    {
    }
};
STRIGID_REGISTER_ENTITY(TestEntity)
