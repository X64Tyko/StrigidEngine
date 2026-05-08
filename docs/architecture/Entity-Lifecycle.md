# Entity Lifecycle

> [← Component System](Component-System.md) | [Home](../Home.md)

---

## Handle Spaces

The Registry manages three independent handle/index spaces. Index 0 is reserved/invalid in all three.

| Handle | Type | Indexes into | Recycling | Purpose |
|---|---|---|---|---|
| `GHandle` | `GlobalEntityHandle` | `Records[]` | Immediate (generation-bumped) | Internal record identity |
| `LHandle` | `EntityHandle` | `LocalToRecord[]` | Deferred via `PendingLocalRecycles` | OOP/Construct-facing handle |
| `NetHandle` | `EntityNetHandle` | `NetToRecord[]` | Deferred via `PendingNetRecycles` | Network replication handle |

**Deferred recycling** prevents ABA aliasing: a stale `LHandle` held by OOP code (or a stale `NetHandle` held by a remote client) could alias a newly created entity if the index were reused immediately. Freed local/net indices sit in pending lists until `ConfirmLocalRecycles()`/`ConfirmNetRecycles()` is called after the safety window.

### EntityRecord Fields

Each `EntityRecord` stores the entity's archetype placement:

| Field | Type | Meaning |
|---|---|---|
| `ArchIndex` | `uint32_t` | Flat index across all chunks in the archetype |
| `ChunkIndex` | `uint32_t` | Which chunk in the archetype's `Chunks[]` array |
| `LocalIndex` | `uint32_t` | Index within the chunk (0..EntitiesPerChunk−1) |

---

## Spawn

The calling thread provides a lambda and performs a **synchronized handshake** with the Logic thread at the top of its frame. The Logic thread allows the spawning thread to write new entity data, then the spawning thread signals Logic to continue.

This handshake is synchronous, thread-safe, and wraps the fundamental contract behind all spawning variants: deferred, queued, and batched.

**Registration outside the handshake window is deferred** to the next handshake. One pattern for all state mutation: spawning, despawning, tick registration, tick deregistration.

### Creation Flow

1. `CreateInternal(classID, span<GHandle>)` — allocates GHandles, populates EntityRecords, pushes archetype slots
2. `MakeEntityHandle(GHandle, classID)` — allocates local index, wires `LocalToRecord[localIdx] → GHandle`, stores LHandle on record
3. Public API (`Create<T>`, `CreateByClassID`) wraps the above, returns `EntityHandle` (LHandle)

---

## Despawn

Entity lifecycle on destruction:

1. **Tombstone.** `Destroy(LHandle)` resolves to GHandle, defers to `PendingDestructions`. The Active flag is cleared immediately — the tombstone propagates through all systems for free:
   - GPU predicate pass sees `Active=0`, skips it
   - Physics awake-only pull sees no body awakening
   - 64-entity bitplane scan sees a zero bit, skips it
2. **Deferred destroy.** `ProcessDeferredDestructions()` runs on the Logic thread: generation-checks each GHandle, calls `RemoveEntity` (tombstone in archetype), then `FreeGlobalHandle`.
3. **Slot reclaim.** `FreeGlobalHandle` reclaims the record index immediately (generation-bumped). Local/net handle indices enter `PendingLocalRecycles`/`PendingNetRecycles`.
4. **Handle recycling.** `ConfirmLocalRecycles()`/`ConfirmNetRecycles()` moves pending indices to the free pool after the safety window.

Visual and physics despawn happen at tombstone time. Memory reclamation happens at deferred destroy time. These are decoupled but aligned — the bitplane skip handles the gap at zero overhead.

---

## Archetype Slot Management

Each archetype tracks entity slots with two independent counters:

| Counter | Mutated on | Purpose |
|---|---|---|
| `AllocatedEntityCount` | Fresh push only | High-water mark of allocated slots (includes tombstoned). Used by `GetAllocatedChunkCount` for iteration bounds. |
| `TotalEntityCount` | Push (+1), Remove (−1) | Live (non-tombstoned) count. Used by `GetLiveChunkCount` for UI/diagnostics. |

`RemoveEntity` tombstones in place — clears the Active flag and moves the slot to `InactiveEntitySlots` but does **not** compact data. Iteration bounds use `AllocatedEntityCount` (the high-water mark) so loops never skip live entities near the tail of a chunk.

`PushEntities` reuses tombstoned slots from `InactiveEntitySlots` before allocating fresh slots at the high-water mark. Reuse does not increment `AllocatedEntityCount`.

---

## ArchetypeFieldLayout

`ArchetypeFieldLayout` (`FlatMap<FieldKey, FieldDescriptor>`) is the single source of truth for every field in the archetype — its chunk slot index, cache tier, frame count, stride, and element size.

`BuildFieldArrayTable` resolves all fields uniformly:
```
outTable[idx] = base + (frame % fieldFrameCount) * fieldFrameStride
```
Cold fields degenerate to `base + 0` (frameCount=1, frameStride=0) — no tier branching needed.

---

## Defragmentation

Defrag improves slab locality but is **incompatible with determinism builds for authoritative data**, since moving an entity changes its `EntityCacheIndex` — the column coordinate that all tiers share.

### Two Kinds of Defrag

1. **Entity slot defrag (within chunk):** Moves live entities between columns within the chunk's column range. Changes `EntityCacheIndex`; all Views must rehydrate.

2. **Chunk mirror relocation (chunk-level compaction):** Relocates a chunk's entire contiguous column range to close holes in the global slab. Also changes `EntityCacheIndex` for every entity in the moved chunk; same rehydration requirement.

### Determinism Build Policy

In determinism builds:
- Entity slot defrag that moves live authoritative entities is **disabled**
- Slot reuse after deterministic destruction is **allowed** (doesn't change existing indices)
- Chunk mirror storage must still be able to reclaim freed space from destroyed chunks — the slab allocator uses a free-range list for this

### Non-Deterministic Builds / Editor Sessions

- Entity slot defrag may be enabled to improve locality
- Slab segment defrag may run as an optional background job

When defrag moves entities, the engine fires an identity-change notification (`EntityIDChanged()` / `OnEntityMoved`) so Views rebind their FieldProxy cursors. Constructs do not need to manually repair pointers — rehydration happens through the listener subscription.

**Safe point:** Defrag/compaction runs only at a defined barrier (top of the Brain frame, after joining jobs) so no worker is reading or writing slab memory while it runs.

### Slab Segment Allocator

Freed chunk mirrors are reclaimed into a free-range list:
- Insertion on free; coalescing of adjacent ranges
- Allocation: try free-range list first (first-fit), then bump `head`
- In determinism builds: lowest-offset first-fit with deterministic tie-breaks; free order normalized to Logic thread finalization phase

---

## View Rehydration Contract

`ConstructView<TEntity>` and entity `EntityView` hydrations register for identity-change notifications. When defrag fires `EntityIDChanged(oldIndex, newIndex)`:

1. The FieldProxy cursor updates to the new array position
2. The Construct does not need to manually repair anything
3. The defrag system is responsible for firing this hook for all subscribers before returning

`ConstructView` also rehydrates automatically when the write frame advances (every logic tick). This handles the normal case where the slab ring buffer wraps and field array base pointers change.
