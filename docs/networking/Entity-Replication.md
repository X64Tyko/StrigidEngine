# Entity Replication

> [← Connection Flow](Connection-Flow.md) | [Rollback Netcode →](Rollback-Netcode.md) | [Home](../Home.md)

---

## Overview

Entity replication is Authority-push, gated on `ClientRepState`. The `ReplicationSystem` runs server-side, one `ServerClientChannel` per connected Owner. It sends `EntitySpawn` (reliable) for entities not yet seen by an Owner, then batches `StateCorrection` (unreliable) for all live entities every net tick.

---

## `ReplicationSystem` — Tick Sequence

Each net tick (default 30Hz) the `ReplicationSystem` runs per connected `Loaded+` Owner:

```
1. SendSpawns()        — reliable EntitySpawn for not-yet-replicated entities
2. SendStateCorrections() — unreliable batched transforms for all live entities
3. After all spawns:   — send FlowEvent::ServerReady → Owner sweeps Alive→Active
```

**Gated push model:** After each logic tick, `PublishCompletedFrame` → `OnFramePublished` dispatches one read-only job per `Loaded+` Owner. Each job exclusively owns its `ServerClientChannel` — zero contention with other channels or with the ECS slab (reads are read-only against the slab).

---

## `ServerClientChannel`

One per connected Owner. Owned by `ReplicationSystem::Clients[MaxOwnerIDs]` for O(1) OwnerID lookup.

```cpp
struct ServerClientChannel {
    PlayerInputLog        InputLog;            // inbound input frames from this Owner
    std::vector<bool>     Replicated;          // per-entity spawn tracking
    std::vector<uint32_t> PendingActivations;  // net handles queued for EntityActivate
    NetChannel            Channel;             // typed per-connection send wrapper
    PendingPacketQueue    SendQueue;           // lock-free MPSC; drained by Sentinel
    ConnectionInfo*       CI;
    uint8_t               OwnerID;
    uint32_t              LastAckedSimFrame;
    // TentativeDestroys / PendingNetDespawns — not yet implemented (see Despawn Protocol)
};
```

`ServerClientChannel` lives inside the `World` it belongs to — PIE worlds are naturally isolated. Each World's `ReplicationSystem` has its own `Clients[]` array.

---

## `EntityNetHandle` — Wire Identity

```
uint32_t: NetOwnerID:8 | NetIndex:24
```

- `OwnerID 0` — Authority/global entities
- `OwnerID 1–255` — Owner-created entities (indexed by the Owner's OwnerID)
- `NetIndex` — allocated per entity on the Authority, stored in `NetToRecord[]`

**Three handle spaces in `Registry`:**

| Handle | Space | Purpose |
|---|---|---|
| `GlobalEntityHandle` | Internal | Generation + Records[] index |
| `EntityHandle` | Local OOP | LocalToRecord mapping |
| `EntityNetHandle` | Network | NetToRecord mapping, wire-safe |

---

## `EntitySpawnPayload`

Sent reliable. Carries enough state for the Owner to recreate the entity and construct a valid `EntityRef`.

`SpawnFlags` packs entity generation alongside flags in a single `int32_t` to avoid payload size growth:

```
bits [31 : 32-EntityGeneration_Bits]  = entity generation
bits [EntityGeneration_Bits-1 : 0]   = spawn flags (Active, Background, etc.)
```

On the Owner:
- `GetGeneration(spawnFlags)` extracts the generation to construct a valid `EntityRef`
- `GetFlags(spawnFlags)` extracts flags to write into `CacheSlotMeta` — never writes the raw value directly

Authority-side: `EntitySpawnPayload::Pack(flags, generation)` produces the packed value.

---

## `EntityRef` — External 64-bit Handle

Internal engine code uses the compact 32-bit `EntityNetHandle`. Gameplay code, RPCs, and all Owner→Authority messages use the 64-bit `EntityRef` which embeds the generation for ABA protection:

```cpp
struct EntityRef {
    EntityNetHandle Handle;      // 32-bit wire handle
    uint16_t        Generation;  // from GlobalEntityHandle::Generation at spawn
    uint16_t        Flags;       // IsPredicted:1, IsOwned:1, reserved:14
};  // 8 bytes total
```

When an Owner sends a targeted message, the Authority validates:
```
NetToRecord[ref.Handle.NetIndex].Generation == ref.Generation
```
Stale references (recycled slot) are rejected before any state mutation.

A `static_assert` on `EntityRef` catches any mismatch if `EntityGeneration_Bits` in `RegistryTypes.h` ever exceeds 16.

---

## `StateCorrection` — Authoritative Transform Batches

Sent unreliable at the net tick rate (30Hz). Each entry is compact:

```cpp
struct StateCorrectionEntry {
    EntityNetHandle Handle;
    Fixed32         PosX, PosY, PosZ;
    float           RotQx, RotQy, RotQz, RotQw;
    uint8_t         ResimFrameDelta;  // server-annotated resim root
    Fixed32         ResimPosX, ResimPosY, ResimPosZ;  // position at resim root
};
```

`ResimFrameDelta` + `ResimPos/Rot` allow the Owner to patch the slab at the resim root frame rather than only the current frame, enabling the rollback system to replay from the correct starting state.

---

## Delta Compression

`EntityDelta` sends only changed component fields (component-level dirty field patches). `InputFrameDelta` sends only changed input fields, with `InputDeltaFlags` marking which fields are present per frame:

```
InputDeltaFlags: KeyState | MouseDX | MouseDY | MouseButtons | Events
```

Each flag indicates whether the corresponding field is included in the wire packet for that frame. Absent fields are carry-forwarded from the previous frame.

---

## Late Join / Reconnect

`SendSpawns` iterates **all** entities with a valid net handle — no relevancy filter, no distance gate. Full world state on connection.

On reconnect, the Owner:
1. Tombstones all replicated entities
2. Flushes its registry
3. Reconnects as a late joiner

Generation bumps on `GlobalEntityHandle` and `EntityNetHandle` prevent aliasing between the previous session's entities and the freshly replicated ones.

---

## `EInterpEntity<Derived>` — Visual Interpolation

For entities whose authoritative position arrives from state corrections (rather than being locally predicted), `EInterpEntity<Derived>` provides CRTP-based visual smoothing:

```cpp
// CVisualTransform component
struct CVisualTransform {
    Fixed32  VisPosX, VisPosY, VisPosZ;
    SimFloat VisBlend;
};
```

`PostPhysics` lerps `CVisualTransform` toward the authoritative position each tick. The render thread uploads `VisPosXYZ` instead of the raw authoritative position, giving smooth visuals even under correction.

---

## Input Routing — `PlayerInputLog`

`PlayerInputLog` lives on `ServerClientChannel` and stores the ring buffer of incoming input frames from one Owner:

- `Store(frame, input)` — stores a frame's input, bounded to `O(ring_depth)` by a high-water clamp
- `Consume(frame)` → returns input for that frame
- `LastConsumedFrame` advances on each `AuthoritySim::OnSimInput` call
- `AdvanceCommittedHorizon` runs on `AuthoritySim::OnFramePublished` when all players have inputs confirmed

`MaxClientInputLead` (currently 64 frames, ~125ms at 512Hz) bounds how far ahead the Authority will simulate without a client's input before stalling. This is a tunable value — the correct setting is RTT- and game-dependent.

---

## Known Gaps

- **No interest management.** Authority sends all entities to all Owners. Per-connection relevancy sets are needed before large-scale multiplayer.
- **Per-archetype net tick rates.** All entities replicate at the same 30Hz. A future `NetTickDivisor` per archetype would allow high-priority objects (players, projectiles) at full rate and low-priority props on-change only.
