# Component System

> [← ECS & Storage](ECS-And-Storage.md) | [Entity Lifecycle →](Entity-Lifecycle.md) | [Home](../Home.md)

---

## FieldProxy — SoA with OOP Syntax

`FieldProxy<T, FieldWidth>` is the core component abstraction. It wraps raw SoA array pointers behind operator overloads so gameplay code reads as OOP while compiling as direct array access.

```cpp
// Gameplay author writes:
transform.PosX += velocity.VelX * dt;   // OOP syntax

// Compiles to:
writeArrayPosX[entityIndex] += writeArrayVelX[entityIndex] * dt;   // direct SoA access
// No virtual dispatch, no map lookup.
```

Each proxy holds:
- `WriteArray` — pointer to the field's SoA array for the current write frame
- `index` — current entity offset within the array
- `mask` — AVX2 mask for `WideMask` partial-lane writes (zero-size in `Scalar` mode to save 32 bytes/field)

### Three Field Widths

| Width | Entities/iteration | Use case |
|---|---|---|
| `Scalar` | 1 | Construct tick access — safe, default |
| `Wide` | 8 (AVX2) | Full SIMD batch — count must be multiple of 8 |
| `WideMask` | 8 with tail mask | Handles non-multiple-of-8 tail chunks |

The same component type participates in both scalar Construct ticks and 8-wide entity sweeps without any code change at the component level. The width is a template parameter, not a runtime choice.

### FieldProxy as a Row in the Spreadsheet

Using the [global spreadsheet model](ECS-And-Storage.md#the-global-spreadsheet-model):
- The field (`Transform.PosX`) is the **row**
- `EntityCacheIndex` is the **column**
- `FieldProxy::Bind(writePtr, idx)` positions the cursor at cell (row=this field, column=entityIndex)
- `Advance(step)` moves +1 (Scalar) or +8 (Wide) columns

### FieldProxyMask

`FieldProxyMask<WIDTH>` is a zero-size base type for `Scalar` mode — saves 32 bytes per field vs always storing `__m256i mask`. Wide and WideMask access the mask via `this->mask`.

---

## Component Registration Macros

Three macros declare a component's storage tier and field list:

### `TNX_TEMPORAL_FIELDS(Name, SystemGroup, Field1, Field2, ...)`

Fields live in the **Temporal** SoA tier (N-frame rollback ring). Used for physics-authoritative and networked data.

```cpp
template <FieldWidth WIDTH = FieldWidth::Scalar>
struct CTransform
{
    FieldProxy<SimFloat, WIDTH> PosX, PosY, PosZ;
    FieldProxy<SimFloat, WIDTH> RotQx, RotQy, RotQz, RotQw;

    TNX_TEMPORAL_FIELDS(CTransform, SystemGroup::None,
        PosX, PosY, PosZ, RotQx, RotQy, RotQz, RotQw)
};
TNX_REGISTER_COMPONENT(CTransform)
```

`SystemGroup::None` means this component alone doesn't determine partition placement (other components on the same entity do).

### `TNX_VOLATILE_FIELDS(Name, SystemGroup, Field1, Field2, ...)`

Fields live in the **Volatile** SoA tier (3-frame triple-buffer). Used for cosmetic data that doesn't need rollback.

```cpp
template <FieldWidth WIDTH = FieldWidth::Scalar>
struct CColor
{
    FieldProxy<float, WIDTH> R, G, B, A;

    TNX_VOLATILE_FIELDS(CColor, SystemGroup::Render, R, G, B, A)
};
TNX_REGISTER_COMPONENT(CColor)
```

`SystemGroup::Render` means entities with this component go into the RENDER or DUAL partition.

### `TNX_REGISTER_FIELDS(Name, Field1, Field2, ...)`

Fields live in **archetype chunk memory** (Cold tier). No slab storage, no rollback, no tier cost. Used for config data read during logic but never iterated in hot paths.

```cpp
template <FieldWidth WIDTH = FieldWidth::Scalar>
struct CJoltBody
{
    FieldProxy<float, WIDTH> ShapeHalfExtentX, ShapeHalfExtentY, ShapeHalfExtentZ;
    FieldProxy<float, WIDTH> Mass;

    TNX_REGISTER_FIELDS(CJoltBody, ShapeHalfExtentX, ShapeHalfExtentY, ShapeHalfExtentZ, Mass)
};
TNX_REGISTER_COMPONENT(CJoltBody)
```

Cold components contribute no slab storage and do not affect the entity's tier classification.

---

## Entity Definition

Entities are CRTP structs templated on `FieldWidth`. The partition group is auto-derived from component `SystemGroup` tags — no manual annotation on the entity.

```cpp
template <FieldWidth WIDTH = FieldWidth::Scalar>
struct EInstanced : EntityView<EInstanced, WIDTH>
{
    CTransform<WIDTH> Transform;  // SystemGroup::None
    CJoltBody<WIDTH>  Jolt;       // SystemGroup::Phys (Cold, but tagged)
    CScale<WIDTH>     Scale;      // SystemGroup::Render (Volatile)
    CColor<WIDTH>     Color;      // SystemGroup::Render (Volatile)
    CMeshRef<WIDTH>   Mesh;       // SystemGroup::Render (Volatile)

    // Derived partition: Phys + Render → DUAL

    void PrePhysics(SimFloat dt)
    {
        // User logic here
    }

    TNX_REGISTER_SCHEMA(EInstanced, EntityView, Transform, Jolt, Scale, Color, Mesh)
};
```

### Dynamic Chunk Sizing

The target entity count per chunk can be specified at the class level:

```cpp
template <FieldWidth WIDTH = FieldWidth::Scalar>
class EProjectile : public EntityView<EProjectile, WIDTH>
{
    static constexpr uint32_t EntitiesPerChunk = 4096;  // High count for SIMD throughput
    // ...
};
```

Default is 256 entities per chunk. Data-heavy entities (projectiles, particles) benefit from larger chunks. Low-count entities (players) can use smaller chunks to reduce padding waste.

### Inheritance Pattern

```cpp
// Base entity (defines shared fields and PrePhysics logic)
template <typename Derived, FieldWidth WIDTH = FieldWidth::Scalar>
struct EBaseCube : EntityView<Derived, WIDTH>
{
    CTransform<WIDTH> Transform;
    CColor<WIDTH>     Color;

    void PrePhysics(SimFloat dt) { Transform.RotQy += SimFloat(0.7f) * dt; }

    TNX_REGISTER_SUPER_SCHEMA(EBaseCube, EntityView, Transform, Color)
};

// Derived entity (adds physics body, overrides PrePhysics)
template <FieldWidth WIDTH = FieldWidth::Scalar>
struct ESuperCube : EBaseCube<ESuperCube, WIDTH>
{
    CRigidBody<WIDTH> Body;  // SystemGroup::Phys → promotes to Temporal

    void PrePhysics(SimFloat dt)
    {
        EBaseCube<ESuperCube, WIDTH>::PrePhysics(dt);
        Transform.PosX += Body.VelX * dt;
        Transform.PosY += Body.VelY * dt;
        Transform.PosZ += Body.VelZ * dt;
    }

    TNX_REGISTER_SCHEMA(ESuperCube, EBaseCube, Body)
};
```

---

## Schema Validation

`SchemaValidation.h` enforces two hard constraints at compile time:

1. **No virtual functions.** A vtable pointer in a component struct pollutes every SoA array slot and breaks the schema. `VALIDATE_COMPONENT_IS_POD` catches this.

2. **All `DefineFields()`-registered fields must be `FieldProxy` types.** A raw `float` added to a component struct but not registered in `DefineFields()` is inert — invisible to the SoA system, can't corrupt layout. The schema is the enforcement boundary; the `Bind`/`Advance` requirement catches mismatches.

Schema validation errors produce clear, actionable compile-time messages. See [Schema Error Reference](../reference/Schema-Error-Reference.md).

---

## Built-In Components

The engine provides a set of ready-to-use components. Gameplay authors can build entirely without defining custom components.

| Component | Tier | SystemGroup | Purpose |
|---|---|---|---|
| `CTransform` | Temporal | None | Position (Fixed32) + rotation (quaternion) |
| `CTranslation` | Temporal | None | Position-only (no rotation) |
| `CRotation` | Temporal | None | Rotation-only |
| `CVelocity` | Temporal | Phys | Linear velocity |
| `CRigidBody` | Temporal | Phys | Full rigid body (vel + angular vel) |
| `CJoltBody` | Volatile | Phys | Jolt shape/motion/mass data (Cold in chunks) |
| `CScale` | Volatile | Render | Non-uniform scale |
| `CColor` | Volatile | Render | RGBA color |
| `CMeshRef` | Volatile | Render | Mesh slot reference |
| `CVisualTransform` | Volatile | Render | Interpolated visual position (EInterpEntity) |
| `CCameraLayer` | Cold | None | Camera layer config |

---

## SimFloat — Determinism Alias

`SimFloat` is the canonical numeric type for all physics-authoritative simulation data:

```cpp
#if TNX_DETERMINISTIC
    using SimFloat = Fixed32;  // Integer determinism for rollback
#else
    using SimFloat = float;    // Float for non-deterministic builds
#endif

template <FieldWidth WIDTH = FieldWidth::Scalar>
using FloatProxy = FieldProxy<SimFloat, WIDTH>;
```

Entity and component authors always write `FloatProxy<WIDTH>` — the backing type is decided at compile time by `TNX_DETERMINISM`. Code written against `SimFloat` works in both modes unchanged.

See [Fixed-Point Math](../math-and-determinism/Fixed-Point.md) for the full fixed-point system.
