# Schema Error Reference

> [← Known Issues](Known-Issues.md) | [Home](../Home.md)

---

## Overview

The schema reflection system enforces two hard constraints at compile time and one at runtime:

**Compile-time (fail fast):**
- Entities must not have virtual functions
- Components must be POD — no `std::string`, `std::vector`, non-trivial constructors/destructors
- Missing `TNX_REGISTER_SCHEMA` on an entity that uses `TNX_REGISTER_ENTITY`
- Missing `TNX_REGISTER_SUPER_SCHEMA` on CRTP base classes

**Runtime (debug assert / log error):**
- Missing `TNX_REGISTER_ENTITY` — detected when `Registry::Create<T>()` is called

---

## Error 1 — Forgetting `TNX_REGISTER_ENTITY`

```cpp
struct MyEntity : EntityView<MyEntity, WIDTH> {
    CTransform<WIDTH> Transform;
    TNX_REGISTER_SCHEMA(MyEntity, EntityView, Transform)
};
// MISSING: TNX_REGISTER_ENTITY(MyEntity);

EntityID id = Registry::Create<MyEntity>();  // Runtime error!
```

```
[ERROR] FATAL: Entity type 'MyEntity' not registered!
        Did you forget TNX_REGISTER_ENTITY(MyEntity)?
```

Debug build: assertion. Release build: returns invalid EntityID.

---

## Error 2 — Missing `TNX_REGISTER_SCHEMA`

```cpp
struct BadEntity : EntityView<BadEntity, WIDTH> {
    CTransform<WIDTH> Transform;
    // MISSING: TNX_REGISTER_SCHEMA
};

TNX_REGISTER_ENTITY(BadEntity);  // Compile error!
```

```
================================================================
ERROR: Entity missing schema registration!
================================================================
Add TNX_REGISTER_SCHEMA to your entity class:
    TNX_REGISTER_SCHEMA(YourEntity, EntityView, component1, component2, ...)
================================================================
```

---

## Error 3 — Virtual Functions in an Entity

```cpp
struct VirtualEntity : EntityView<VirtualEntity, WIDTH> {
    CTransform<WIDTH> Transform;
    virtual void PrePhysics(SimFloat dt) { }  // WRONG: virtual!
    TNX_REGISTER_SCHEMA(VirtualEntity, EntityView, Transform)
};
TNX_REGISTER_ENTITY(VirtualEntity);  // Compile error!
```

```
================================================================
ERROR: Entity must be standard layout!
================================================================
Entity types cannot have:
  - Virtual functions
  - Complex inheritance
Entities are lightweight data containers.
================================================================
```

**Why:** Virtual functions add a vtable pointer to the struct, which would occupy a slot in every SoA array column. This breaks SoA decomposition and wastes memory.

---

## Error 4 — Non-POD Component

```cpp
template <FieldWidth WIDTH = FieldWidth::Scalar>
struct BadComponent {
    std::string name;           // WRONG: not POD!
    FieldProxy<float, WIDTH> value;
    TNX_TEMPORAL_FIELDS(BadComponent, SystemGroup::None, value)
};
TNX_REGISTER_COMPONENT(BadComponent);  // Compile error!
```

```
================================================================
ERROR: Component must be POD (plain old data)!
================================================================
Components CANNOT have:
  - Virtual functions
  - Non-trivial constructors/destructors
  - std::string, std::vector, or complex types
  - Heap-allocated pointers

All component fields must be FieldProxy<T, WIDTH>:

    template <FieldWidth WIDTH = FieldWidth::Scalar>
    struct CMyTransform {
        FieldProxy<float, WIDTH> PosX, PosY, PosZ;
        TNX_TEMPORAL_FIELDS(CMyTransform, SystemGroup::None, PosX, PosY, PosZ)
    };
================================================================
```

**Why:** Components live in contiguous SoA arrays. Non-trivial constructors/destructors require individual object management, which is incompatible with bulk array operations and SIMD processing.

---

## Error 5 — Missing `TNX_REGISTER_SUPER_SCHEMA` on Base

```cpp
// WRONG: Using TNX_REGISTER_SCHEMA on an intermediate base:
struct BaseCube : EntityView<BaseCube, WIDTH> {
    CTransform<WIDTH> Transform;
    TNX_REGISTER_SCHEMA(BaseCube, EntityView, Transform)  // should be SUPER_SCHEMA
};

struct SuperCube : BaseCube<SuperCube, WIDTH> {
    CVelocity<WIDTH> Velocity;
    TNX_REGISTER_SCHEMA(SuperCube, BaseCube, Velocity)
};
TNX_REGISTER_ENTITY(SuperCube);
```

Using `TNX_REGISTER_SCHEMA` on a non-leaf base causes incorrect schema generation for derived types — derived entity's schema won't include the base's fields correctly.

**Fix:** Use `TNX_REGISTER_SUPER_SCHEMA` for non-leaf base classes in the CRTP hierarchy:

```cpp
struct BaseCube : EntityView<BaseCube, WIDTH> {
    CTransform<WIDTH> Transform;
    TNX_REGISTER_SUPER_SCHEMA(BaseCube, EntityView, Transform)  // correct
};
```

---

## Correct Patterns

### Entity

```cpp
template <FieldWidth WIDTH = FieldWidth::Scalar>
struct GoodEntity : EntityView<GoodEntity, WIDTH> {
    CTransform<WIDTH> Transform;
    CVelocity<WIDTH>  Velocity;
    CColor<WIDTH>     Color;

    FORCE_INLINE void PrePhysics(SimFloat dt) {
        Transform.PosX += Velocity.VelX * static_cast<float>(dt);
    }

    TNX_REGISTER_SCHEMA(GoodEntity, EntityView, Transform, Velocity, Color)
};
TNX_REGISTER_ENTITY(GoodEntity)
```

### Temporal Component

```cpp
template <FieldWidth WIDTH = FieldWidth::Scalar>
struct CMyComponent {
    FieldProxy<float, WIDTH>   Value1, Value2;
    FieldProxy<int32_t, WIDTH> Value3;

    TNX_TEMPORAL_FIELDS(CMyComponent, SystemGroup::None, Value1, Value2, Value3)
};
TNX_REGISTER_COMPONENT(CMyComponent)
```

### Cold Component

```cpp
struct CColdData {
    uint32_t ShapeType;
    float    Mass;
    float    Friction;

    TNX_REGISTER_FIELDS(CColdData, ShapeType, Mass, Friction)
};
TNX_REGISTER_COMPONENT(CColdData)
```

---

## Known Unchecked Gotcha

`Derived` entities that inherit from a `SUPER_SCHEMA` base and forget `Replace()` in the derived `PrePhysics` will produce silent wrong behavior — the base's `PrePhysics` runs instead of the derived version. There is no compile-time or runtime check for this. The static analyzer enhancement is planned.

---

## Summary

| Check | When |
|---|---|
| Missing `TNX_REGISTER_SCHEMA` | Compile error |
| Virtual functions in entity | Compile error |
| Non-POD component | Compile error |
| Missing `TNX_REGISTER_ENTITY` | Runtime assert (debug) / log error (release) |
| `TNX_REGISTER_SCHEMA` on base instead of `SUPER_SCHEMA` | Silent wrong behavior — no check |
