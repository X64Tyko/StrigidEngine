# ECS & Storage

> [← Threading](Threading.md) | [Component System →](Component-System.md) | [Home](../Home.md)

---

## Overview: Four Storage Tiers

Entity component data lives in one of four tiers based on access pattern and rollback requirements. The tier is declared on the **component** via registration macro — not on the entity.

| Tier | Structure | Frames | Rollback | Use Case |
|---|---|---|---|---|
| **Cold** | Archetype chunks (AoS) | 1 | No | Rarely-updated config data (health, ammo caps) |
| **Static** | Separate read-only SoA | 1 | No | Geometry/mesh data, never changes |
| **Volatile** | SoA triple-buffer | 3 | No | Cosmetic entities, particles, decals |
| **Temporal** | SoA N-frame ring | max(8, X) | Yes | Networked, simulation-authoritative entities |

**An entity's effective tier is the highest tier of any of its components.** An entity carrying a `CJoltBody` (Temporal) is a Temporal entity; an entity with only `CColor` (Volatile) is Volatile. Cold components (`TNX_REGISTER_FIELDS`) contribute no slab storage and do not affect the tier classification.

**If `TNX_ENABLE_ROLLBACK` is disabled,** Temporal components are treated as Volatile (3-frame triple-buffer). Games that don't need rollback pay zero memory cost for the tier system.

### Why Four Tiers

A single SoA ring buffer for everything would waste memory on entities that never roll back and bandwidth on entities that never render:

| Tier | Ring depth | Why |
|---|---|---|
| Cold | 1 | Rarely updated config. AoS in chunks. No iteration cost. |
| Static | 1 | Read-only geometry. Never touched by update loops. |
| Volatile | 3 | Triple-buffer for Logic↔Render handoff. No rollback needed. |
| Temporal | N | Rollback history. Only networked entities pay the memory cost. |

### Volatile = 3 Frames

Originally 5 frames were used. After moving to GPU-driven rendering with a persistent previous-frame InstanceBuffer on the GPU side, the render thread only needs to supply frame T — the GPU interpolates T-1 from its own buffer. CPU slab needs 1 frame for logic and 1 for render simultaneously, so 3 (triple-buffer) is the correct minimum.

---

## The Global Spreadsheet Model

TrinyxEngine's SoA data is easiest to reason about as a **single global spreadsheet**:

- **Columns = Entities.** The column index is `EntityCacheIndex`.
- **Rows = Fields.** Each FieldProxy field (`Transform.PosX`, `Health.Value`) is one contiguous SoA array.
- **Cells = Values.** The value for entity `i` in field `Transform.PosX` is the cell at (row=PosX, column=i).

Volatile and Temporal are not separate "entity spaces" — they are different **row ranges** in the same global model. Cold chunk storage is additional rows outside the main slab ranges, but they still use `EntityCacheIndex` as the column coordinate.

Chunks claim **contiguous column ranges** sized by `EntitiesPerChunk`. If one chunk claims `[0..255]`, the next chunk claims the next range. This makes indexing uniform: every field array in every tier uses the same `EntityCacheIndex`.

**Consequence:** any operation that relocates a chunk's column range (defragmentation) changes `EntityCacheIndex` for all entities in that chunk and requires all Views to rehydrate their FieldProxy cursors. See [Entity Lifecycle](Entity-Lifecycle.md) for defrag details.

---

## Partition Layout: Dual-Ended Arenas

The slab is divided into two arenas by `EngineConfig`. Within each arena, two buckets grow inward from opposite ends.

```
Arena 1: Renderable  [0 .. MAX_RENDERABLE_ENTITIES)
  RENDER (→) starts at 0              — render-only (particles, decals, ambient props)
  DUAL   (←) starts at MAX_RENDERABLE — physics + render (players, AI, physics props)

Arena 2: Cached  [MAX_RENDERABLE_ENTITIES .. MAX_CACHED_ENTITIES)
  PHYS  (→) starts at MAX_RENDERABLE — physics-only (triggers, invisible movers)
  LOGIC (←) starts at MAX_CACHED     — logic/rollback-only entities
```

Config constraints (validated at startup):
```
MaxRenderEntities + MaxDualEntities <= MAX_RENDERABLE_ENTITIES
MAX_RENDERABLE_ENTITIES + MaxPhysEntities <= MAX_CACHED_ENTITIES
```

### Why Dual-Ended Arenas

Physics must iterate players + AI + physics props densely. Rendering must iterate particles + decals + players densely. These sets overlap but are not identical.

The dual-ended layout solves this with zero padding overhead:
- **Physics** iterates DUAL + PHYS contiguously at the arena boundary — a dense wall with no gap. 100% of its relevant entities, no render-only or logic-only data anywhere in its access range.
- **Render** iterates RENDER + DUAL with one gap in Arena 1. The GPU predicate pass handles this at negligible cost (the gap is all-zero in the Active bitplane, so the predicate writes 0 for every slot in the gap).

No gap-skipping branches are needed in the physics loop. The render gap is handled by the GPU, not the CPU.

---

## Entity Group Auto-Derivation

The partition group (Dual/Phys/Render/Logic) is computed automatically at `TNX_REGISTER_ENTITY` time from the `SystemGroup` tags on each component. There is no manual annotation on the entity — that would be a footgun that silently puts entities in the wrong partition.

```cpp
// Component annotations drive automatic entity placement:
TNX_TEMPORAL_FIELDS(CJoltBody,  SystemGroup::Phys,   ...)  // contributes Phys membership
TNX_VOLATILE_FIELDS(CMeshRef,   SystemGroup::Render, ...)  // contributes Render membership
TNX_TEMPORAL_FIELDS(CTransform, SystemGroup::None,   ...)  // partition-agnostic

// Derivation rule:
// Has Phys AND Render → Dual
// Has Phys only       → Phys
// Has Render only     → Render
// Temporal, neither   → Logic
```

Examples:
| Entity Type | Components | Partition |
|---|---|---|
| `EInstanced` | CTransform + CJoltBody + CMeshRef + CColor + CScale | Dual |
| `EPlayer` | CTransform + CVelocity + CMeshRef + CColor + CScale | Dual |
| `EPoint` | CTransform only | Phys |

---

## TemporalComponentCache

The temporal and volatile slabs are managed by `TemporalComponentCache` (base: `ComponentCacheBase`).

**Ring buffer math:** frame N for a given field is at:
```
base + (frame % frameCount) * frameStride
```
Cold fields degenerate to `base + 0` (frameCount=1, frameStride=0) — no tier branching needed in `BuildFieldArrayTable`.

**Key APIs:**
- `GetWriteFramePtr(void*)` — returns pointer to field array for the current write frame (T+1)
- `GetReadFramePtr(void*)` — returns pointer to field array for the current read frame (T)
- `GetPhysicsRange()` — returns `[DualStart, PhysEnd)` in cache index units for dense physics iteration

### Dirty Bit Tracking

Three bits in `TemporalFlagBits` per entity:

| Bit | Name | Meaning |
|---|---|---|
| bit 31 | `Active` | Entity is alive and should be processed |
| bit 30 | `Dirty` | Cumulative upload trigger — cleared when render acknowledges |
| bit 29 | `DirtiedFrame` | Per-frame flag — cleared unconditionally at frame start |

The `Dirty` bit drives selective GPU upload. The same bit drives rollback blast radius: the dirty set from a correction propagates naturally through update logic to exactly the entities that need recomputation.

---

## Bitplane Gap Skipping

The universal active strip is scanned 64 entities at a time (one 64-bit word):

- **Zero word** (64 inactive entities): skipped instantly — no field data touched
- **Mixed word** (some active/inactive): `FieldProxy` uses AVX2 masked loads/stores (8-wide, branchless)

This covers the unused space between RENDER and DUAL buckets in Arena 1 and sparse regions at startup with no measurable overhead.

---

## Memory Sizing

See [Configuration](../getting-started/Configuration.md) for full sizing tables. Quick reference:

```
Volatile slab = MAX_CACHED_ENTITIES × bytes/entity × 3 frames
Temporal slab = MAX_CACHED_ENTITIES × bytes/entity × TemporalFrameCount

Example (100k entities, 92 B/entity, TemporalFrameCount=8):
  Volatile: 100k × 92 × 3 = ~27.6 MB
  Temporal: 100k × 92 × 8 = ~73.6 MB

Example (TemporalFrameCount=128 for 250ms rollback @ 512Hz):
  Temporal: 100k × 92 × 128 = ~1.18 GB
```
