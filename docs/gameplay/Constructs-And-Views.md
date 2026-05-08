# Constructs & Views

> [Home](../Home.md) | [Game Flow →](Game-Flow.md)

---

## Two Object Types

The gameplay layer splits into two object types with fundamentally different access patterns:

| | Construct | Entity |
|---|---|---|
| Count | Singular (1 Player, 1 GameMode) | Horde (10K zombies, 50K bullets) |
| Logic | Bespoke (`ScalarUpdate`, `PrePhysics`) | None, or wide SIMD sweep |
| Dispatch | Type-erased `ConstructBatch` | Per-archetype SIMD job |
| API | `Construct<T>`, `Owned<T>`, Views | `TNX_REGISTER_ENTITY`, `EntityView` |
| Tick | Scalar, sequential | 8-wide AVX2, parallel |

**Rule of thumb:** "Is this a singular object with bespoke logic, or a horde member?"
- Singular → `Construct`
- Horde → `Entity`

---

## `Construct<T>` — CRTP Base

`Construct<T>` is the CRTP base for singular complex gameplay objects. It auto-registers tick hooks via `if constexpr` concept detection — implement the method and get the tick; don't implement it and pay nothing (no vtable slot, no virtual dispatch overhead).

```cpp
class Player : public Construct<Player>
{
    ConstructView<EPlayer> Body;
    JoltCharacter CharacterController;

public:
    void InitializeViews()
    {
        Body.Initialize(this);
        CharacterController.Initialize(...);
    }

    void PhysicsStep(SimFloat dt)
    {
        // Runs at physics rate (64Hz default, not 512Hz)
        CharacterController.Update(desiredVelocity, gravity, dt, ...);
        auto pos = CharacterController.GetPosition();
        Body.Transform.PosX = SimFloat::FromFloat(pos.GetX());
        Body.Transform.PosY = SimFloat::FromFloat(pos.GetY());
        Body.Transform.PosZ = SimFloat::FromFloat(pos.GetZ());
    }

    void ScalarUpdate(SimFloat dt)
    {
        // Camera, UI, post-logic (variable rate)
    }
};
```

### Four Tick Hooks

| Hook | When it runs | Use case |
|---|---|---|
| `PrePhysics(SimFloat dt)` | After wide entity PrePhysics sweep | Read input, set intentions |
| `PostPhysics(SimFloat dt)` | After wide entity PostPhysics sweep + pull | React to physics results |
| `PhysicsStep(SimFloat dt)` | During physics tick window (at physics rate) | Drive `JoltCharacter`, write kinematic targets |
| `ScalarUpdate(SimFloat dt)` | Outside fixed loop, variable rate | Camera, cosmetics, UI, AI thinking |

All hooks are optional. Concept detection at compile time (`if constexpr (HasPrePhysics<Derived>)`) ensures you pay nothing for hooks you don't implement.

### Creation

Constructs are created via `ConstructRegistry::Create<T>(world)`. The registry owns the object and manages its lifetime within the declared `ConstructLifetime` tier.

### Serialization Contract

Constructs do NOT serialize their own C++ member variables. Only View-owned ECS data is serialized through the existing ECS path. Designer-authored values (e.g., `TurretBase::MaxAmmo`) belong in cold components so they serialize automatically.

On load: `CreateConstruct<T>()` → spawns Views → hydrates from serialized ECS data → re-derives transient state in `PostInitialize`.

---

## `ConstructView<TEntity>` — Generic ECS Lens

`ConstructView<TEntity>` is a single generic template that:
1. Creates one backing ECS entity of any `EntityView` type
2. Hydrates FieldProxy cursors on initialization
3. Auto-rehydrates when the write frame advances or defrag relocates the entity
4. Auto-derives partition from the entity type's component `SystemGroup` tags

```cpp
ConstructView<EInstanced> Body;  // CTransform + CJoltBody + ... → DUAL partition
ConstructView<EPlayer> Body;     // CTransform + CVelocity + ... → DUAL partition
ConstructView<EPoint> Body;      // CTransform only              → PHYS partition
```

**Ownership chain:** `Construct → ConstructView → ECS Entity → Components → FieldArrays → FieldProxies`

The View provides direct access to component fields:
```cpp
// Writes go directly to the SoA write array for the current frame:
Body.Transform.PosX = SimFloat::FromMeters(10.0f);
Body.Color.R = 1.0f;
```

---

## `Owned<T>` — Composition

Complex Constructs compose via `Owned<T>` value members — compile-time ownership with deterministic init/destroy order and zero heap allocation.

```cpp
class Turret : public Construct<Turret>
{
    ConstructView<EInstanced> Body;    // Physics + render body
    Owned<BarrelAssembly>  Barrel;     // Has its own ConstructView, own tick
    Owned<TargetingSystem> Targeting;  // Pure logic, no View needed
    Owned<AmmoFeed>        Ammo;       // Data-only, no physics
};
```

`Owned<T>` enforces:
- **Lifetime:** child is destroyed when parent is destroyed. No orphans.
- **Init order:** declaration order (Views hydrate first, then `Owned<T>` members, depth-first walk)
- **Tick order:** parent ticks before children, deterministic by declaration order
- **Compile-time contracts:** `static_assert(std::is_base_of_v<Construct<T>, T>)` enforced

### Initialization Order

1. Parent Views hydrate (base class init)
2. `Owned<T>` members construct in declaration order (language guarantee)
3. Each `Owned<T>` recursively runs the same sequence (depth-first walk)
4. `PostInitialize()` on parent — all children live, all Views hydrated

### Compile-Time Interface Contracts

C++20 concepts on `Owned<T>` enable zero-cost compile-time replacement for runtime gameplay tag queries:

```cpp
// "Targetable" concept — zero runtime cost, checked at compile time
Owned<BarrelAssembly, Concepts::Targetable> Barrel;
```

---

## `ConstructBatch` — Type-Erased Tick Dispatch

Constructs register into typed scalar batches on the Brain thread. Dispatch is type-erased and non-virtual:

```cpp
struct ConstructTickEntry
{
    void*       Object;
    void      (*Fn)(void*);      // [](void* o){ static_cast<T*>(o)->PrePhysics(dt); }
    TickGroup   Group;
    int16_t     OrderWithinGroup;
};

enum class TickGroup : uint8_t
{
    PreInput    = 0,  // Read input state before anything reacts
    Default     = 1,  // Standard gameplay logic (most Constructs)
    PostDefault = 2,  // Things that depend on Default having run
    Camera      = 3,  // Camera always resolves after gameplay
    Late        = 4,  // Final adjustments, IK, procedural
};
```

`ConstructBatch` sorts entries only when dirty (`stable_sort` preserves registration order as tiebreaker — deterministic without requiring explicit numbers from every Construct). Most Constructs register at `Default` with Order 0.

### Thread Safety

Registration follows the spawn handshake contract: the handshake window is the one safe place to mutate engine state. Registration outside the window defers to the next handshake. This is the same pattern used for spawning, despawning, and all state mutation.

---

## `ConstructLifetime` Tiers

Every Construct declares a static lifetime tier. `FlowManager` uses this to determine what survives each transition:

```cpp
enum class ConstructLifetime : uint8_t
{
    Level,      // Destroyed when the Level unloads
    World,      // Destroyed when the World resets
    Session,    // Survives World reset. Destroyed when the session ends.
    Persistent, // Survives everything. Destroyed only explicitly.
};
```

Constructs surviving a World reset receive `OnWorldTeardown()` (null Views and save state) and `OnWorldInitialized(World*)` (rebuild Views against new World) callbacks.

---

## `GameMode` — Server-Authoritative Rules

`GameMode` is an inheritable base class for match rules — NOT a Construct itself. Users opt into per-frame ticks via multiple inheritance when needed:

```cpp
// Event-driven mode (no tick overhead):
class ArenaMode : public GameMode
{
    void OnPlayerJoined(Soul& soul) override;  // spawn logic
    void OnPlayerLeft(Soul& soul) override;
};

// Mode with per-frame logic:
class ArenaMode : public GameMode, public Construct<ArenaMode>
{
    void OnPlayerJoined(Soul& soul) override;
    void ScalarUpdate(SimFloat dt) { /* win condition check */ }
};
```

`GameMode` hooks: `OnPlayerJoined(Soul&)`, `OnPlayerLeft(Soul&)`, `OnPlayerBeginRequest(Soul&, req)`.

---

## `JoltCharacter` — Character Controller

`JoltCharacter` wraps `JPH::CharacterVirtual` for Construct-driven character movement. It is completely independent of the `CJoltBody` component — no Jolt body is created in the ECS for the character. The Construct owns and drives the JoltCharacter directly.

```cpp
class Player : public Construct<Player>
{
    ConstructView<EPlayer> Body;
    JoltCharacter CharacterController;

    void InitializeViews()
    {
        Body.Initialize(this);
        CharacterController.Initialize(
            GetWorld()->GetPhysics()->GetPhysicsSystem(),
            JPH::RVec3(0, 5, 0),  // initial position
            0.3f,   // capsule radius
            0.7f);  // capsule half-height
    }

    void PhysicsStep(SimFloat dt)
    {
        // CharacterVirtual handles grounding, stair stepping, slope sliding
        CharacterController.Update(desiredVelocity, gravity, dt,
            *GetWorld()->GetPhysics()->GetTempAllocator());

        // Write resolved position back to slab via FieldProxy
        JPH::RVec3 pos = CharacterController.GetPosition();
        Body.Transform.PosX = SimFloat::FromFloat(pos.GetX());
        Body.Transform.PosY = SimFloat::FromFloat(pos.GetY());
        Body.Transform.PosZ = SimFloat::FromFloat(pos.GetZ());
    }
};
```

`JoltPhysics::GetTempAllocator()` exposes the Jolt temp allocator needed for `ExtendedUpdate`. `JoltLayers.h` provides the shared layer constants (Static, Dynamic) used by both `JoltPhysics` and `JoltCharacter`.
