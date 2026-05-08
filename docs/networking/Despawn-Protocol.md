# Despawn Protocol

> [← Rollback Netcode](Rollback-Netcode.md) | [Home](../Home.md)

---

## Overview

Networked entity despawn is four phases, gated on `CommittedFrameHorizon`. The gate ensures that a net slot is never freed while a rollback could still revive the entity — the Authority cannot free a net slot until all Owners have confirmed inputs for the frame in which the entity died.

**Implementation status:** Phases 1–3 (Commit → Send → Client Apply) are implemented. Phase 0 (Tentative tracking) is not yet implemented — see the Known Gap section.

---

## Four Phases

### Phase 0 — Tentative *(not yet implemented)*

Entity dies in speculative simulation (frame not yet committed to `CommittedFrameHorizon`).

```
Entity death → recorded in TentativeDestroys[frame] on per-Owner ServerClientChannel
Net index held. No packet sent.
Rollback cancels this and revives the entity.
```

- The net index is held — not freed, not sent
- If a rollback reverts the death frame, the entry is removed from `TentativeDestroys` and the entity is alive again
- Only graduates to Phase 1 when `CommittedFrameHorizon` advances past the death frame

**Known gap:** The `CommittedFrameHorizon` gate prevents incorrect early send, but explicit rollback-cancellable tentative tracking requires the per-frame `TentativeDestroys` ring buffer to be implemented on `ServerClientChannel`.

### Phase 1 — Commit

`CommittedFrameHorizon` advances past the entity's death frame (all Owners have confirmed inputs).

```
TentativeDestroys entry → graduates to PendingNetDespawns
Replicated[i] cleared on the channel
Server cannot free the net slot before this transition
```

- The `Replicated[]` bitvector for this entity is cleared on the channel — the Authority knows the Owner no longer expects updates for it
- The entity is in `PendingNetDespawns`, waiting to be batched and sent

### Phase 2 — Send

Authority-side `SendDespawns()` batches and sends.

```
SendDespawns() → batches N × uint32_t net handle values → sends reliable
ConfirmNetRecycles() fires after send
GNS reliable ordering makes explicit ACK unnecessary
```

- Wire format: `EntityDestroyPayload` = N × `uint32_t` (net handle values), `count = PayloadSize / 4`
- Sent reliable — GNS guarantees ordered delivery, so `ConfirmNetRecycles()` can fire immediately after send without waiting for an explicit ACK
- `ConfirmNetRecycles()` moves the pending net indices from `PendingNetRecycles` to the free pool

### Phase 3 — Owner Apply

`OwnerNet::HandleEntityDestroy` receives the batch.

```
EntityDestroy handler: batch of N × uint32_t net handle values
For each handle:
    Look up via NetToRecord[handle.NetIndex]
    Call Destroy() locally
ConfirmLocalRecycles() + ConfirmNetRecycles() run normally
```

- Each handle is resolved via `NetToRecord` to get the local `EntityRecord`
- `Destroy()` tombstones the entity locally — the same deferred-destruction path as any local despawn
- Handle recycling safety windows (`ConfirmLocalRecycles` / `ConfirmNetRecycles`) ensure stale handles held by OOP code or in-flight net messages cannot alias a newly-created entity

---

## Why Gate on `CommittedFrameHorizon`

Consider what happens without the gate:

1. Entity dies at frame 100 (speculative)
2. Authority frees the net slot at frame 100 and assigns it to a new entity at frame 101
3. A rollback to frame 98 revives the dead entity
4. Now two entities share the same net slot — the engine cannot distinguish them

`CommittedFrameHorizon` is the frame below which no rollback can occur. By requiring the horizon to pass the death frame before freeing the net slot, the four-phase protocol guarantees net slots are never aliased across rollback windows.

---

## `ConfirmNetRecycles` and Deferred Handle Safety

From `Entity-Lifecycle.md`: freed net indices enter `PendingNetRecycles` and only move to the free pool when `ConfirmNetRecycles()` is called after the safety window. This prevents ABA aliasing where a remote Owner holds a stale `EntityNetHandle` that gets recycled before the despawn message arrives.

GNS reliable ordering means the despawn message is guaranteed to arrive before any message that references the recycled net slot — so `ConfirmNetRecycles()` is safe to call immediately after `SendDespawns()`.

---

## `ConstructDestroy` — Construct Despawn

Analogous wire type for Constructs (not entities):

```
EntityDestroyPayload  = N × uint32_t (EntityNetHandle values)
ConstructDestroyPayload = N × uint32_t (ConstructNetHandle values)
```

`ConstructNetHandle` uses the same packing as `EntityNetHandle`. The receiver resolves via `ConstructRegistry`'s `PagedMap` rather than `NetToRecord`.

---

## State at a Glance

```
Entity death (speculative)
        │
        ▼
Phase 0: TentativeDestroys[deathFrame]     ← not yet implemented
        │
        │  CommittedFrameHorizon > deathFrame
        ▼
Phase 1: PendingNetDespawns                Replicated[i] = false
        │
        │  SendDespawns() called (net tick)
        ▼
Phase 2: EntityDestroyPayload sent reliable  ConfirmNetRecycles()
        │
        │  Owner receives packet
        ▼
Phase 3: Owner Destroy()                   ConfirmLocalRecycles() + ConfirmNetRecycles()
```
