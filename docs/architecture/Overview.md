# Architecture Overview

> [← Home](../Home.md) | [Threading →](Threading.md) | [ECS & Storage →](ECS-And-Storage.md)

---

TrinyxEngine is not a general-purpose engine. It is built to solve one specific and hard problem: **competitive multiplayer simulation where two clients running the same inputs must produce bit-identical state, and input latency is a design constraint at the substrate level.**

Every architectural decision traces back to three load-bearing constraints. Change any one of them and the rest needs to be redesigned.

---

## The Three Core Constraints

### 1. Fixed 512Hz Logic Update

The simulation runs at a stable, known tick rate — 1.95ms per frame. This is not a performance target that gets relaxed; it is a structural property the entire system relies on.

- **Rollback netcode** requires a known tick rate to align rollback frames and Jolt physics snapshots.
- **Input timestamps** are logic-frame-relative, not wall-clock-relative.
- **Clock synchronization** between Authority and Owner is expressed in frame counts.
- **Physics divisor** (512Hz logic / 64Hz physics) is an integer ratio that only makes sense given a fixed rate.

Variable timestep for the authoritative simulation is incompatible with deterministic replay and rollback netcode. The engine has a separate variable-rate **Scalar Update** tick for camera, cosmetics, and UI — things that only need to look smooth, not be deterministic. The fixed loop and the variable loop are two halves of the same design.

### 2. Tiered SoA Storage with Rollback as a First-Class Citizen

Entity component data lives in one of four storage tiers based on its update pattern and rollback requirements:

| Tier | Structure | Rollback | Use Case |
|---|---|---|---|
| Cold | Archetype chunks (AoS) | No | Rarely-updated config data |
| Static | Read-only SoA array | No | Geometry, never changes |
| Volatile | SoA triple-buffer | No | Particles, decals, cosmetics |
| Temporal | SoA N-frame ring | Yes | Networked, simulation-authoritative entities |

The tier is declared on the **component** (via registration macro), not on the entity. An entity's effective tier is the highest tier of any of its components. A single macro change at the component level promotes an entire class of entities to rollback-capable.

When `TNX_ENABLE_ROLLBACK` is off, Temporal components fall back to the Volatile 3-frame triple-buffer. Games that don't need rollback pay zero memory cost for the tier system.

### 3. OOP API Over a Data-Oriented Substrate

SIMD batch processing requires Structure-of-Arrays layout. SoA is hostile to component authors — writing `posXArray[i] += velXArray[i] * dt` for every field leaks the data layout into gameplay code.

`FieldProxy<T, WIDTH>` wraps raw SoA array pointers behind operator overloads:

```cpp
// Gameplay author writes:
transform.PosX += velocity.VelX * dt;

// Compiles to a direct SoA array access. No virtual dispatch, no map lookup.
```

The gameplay layer then splits into two object types:

- **Constructs** — Singular complex OOP objects (`Construct<Player>`, `Construct<GameMode>`). Own Views into ECS data, hold bespoke logic, auto-register ticks via C++20 concept detection.
- **Entities** — Raw ECS data for the horde (zombies, bullets, particles). No bespoke logic. Swept by the engine with 8-wide AVX2.

Forcing the horde into Constructs makes SIMD batch processing impossible. Forcing the thinkers into raw ECS makes per-object logic unnatural. The split gives each use case the correct tool.

---

## Architecture Layers

```
┌──────────────────────────────────────────────────────────┐
│  GAMEPLAY                                                 │
│  Construct<T> · Owned<T> · ConstructView<TEntity>        │
│  FlowManager · GameMode · Soul                           │
├──────────────────────────────────────────────────────────┤
│  ENGINE SYSTEMS                                          │
│  LogicThread<TNet,TRollback,TFrame>                      │
│  JoltPhysics · AudioManager · CameraManager              │
│  NetThread · ReplicationSystem                           │
├──────────────────────────────────────────────────────────┤
│  ECS CORE                                                │
│  Registry · Archetype · TemporalComponentCache           │
│  FieldProxy · EntityView · SchemaValidation              │
├──────────────────────────────────────────────────────────┤
│  THREADING                                               │
│  Sentinel (1000Hz) · Brain (512Hz) · Encoder (variable)  │
│  Lock-free MPMC Job System (4 queues)                    │
├──────────────────────────────────────────────────────────┤
│  PLATFORM                                                │
│  Raw Vulkan (volk + VMA) · SDL3 · Slang shaders          │
└──────────────────────────────────────────────────────────┘
```

---

## Mental Model: The Global Spreadsheet

TrinyxEngine's core storage is easiest to reason about as a **single global spreadsheet**:

- **Columns = Entities.** The column index is `EntityCacheIndex` — one integer that identifies an entity across all tiers.
- **Rows = Fields.** Each FieldProxy field (`Transform.PosX`, `Health.Value`) is one row: a contiguous SoA array.
- **Cells = Values.** The value for entity `i` in field `Transform.PosX` is at `(row=PosX array, column=i)`.

Volatile and Temporal are not separate "entity spaces." They are different row ranges in the same global model — different field sets and different history depths, but the same `EntityCacheIndex` column coordinate applies everywhere.

This has one critical implication: any operation that relocates an entity's column slot (defragmentation, chunk compaction) changes its identity and requires all Views to rehydrate their FieldProxy cursors.

---

## What This Engine Is Not

- Not a general-purpose engine competing on breadth
- Not designed for variable-timestep authoritative simulation
- Not designed with "multiplayer as an afterthought" — rollback, determinism, and input latency are load-bearing substrate properties

The correct question to ask before changing anything: *does this change break determinism, increase fixed-step latency, or prevent rollback?* If yes, it needs to go somewhere else in the architecture.

---

## Further Reading

| Topic | Page |
|---|---|
| Threading model in detail | [Threading](Threading.md) |
| Storage tiers and partition layout | [ECS & Storage](ECS-And-Storage.md) |
| FieldProxy and component system | [Component System](Component-System.md) |
| Entity spawn/despawn/handles | [Entity Lifecycle](Entity-Lifecycle.md) |
| Construct/View OOP layer | [Constructs & Views](../gameplay/Constructs-And-Views.md) |
