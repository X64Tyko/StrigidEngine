# Networking Overview

> [Home](../Home.md) | [Connection Flow →](Connection-Flow.md)

---

## Model

Server-authoritative over GameNetworkingSockets (GNS). The transport is an implementation detail — gameplay-facing API is built around `NetChannel`, `Soul`, and `ConstructHandle`. PIE loopback is the primary development target; dedicated server follows naturally from the same code paths.

---

## Vocabulary

**Never use "server" or "client" as standalone nouns.** Use these terms:

| Term | Meaning |
|---|---|
| `Authority` | Dedicated server, or the authoritative sim side of a Host |
| `Owner` | The local player; the Soul that owns input for an entity |
| `Host` | Listen server — Soul with both `Authority + Owner` roles (`EngineMode::Host`) |
| `Echo` | Non-owning entity stance on a client (remote player's representation) |
| `Solo` | Offline, no networking (`EngineMode::Standalone`, `SoloSim`) |
| `AuthorityNet` | Authority-side net handler (`AuthorityNet.h`) |
| `OwnerNet` | Owner-side net handler (`OwnerNet.h`) |

---

## Core Components

### GNSContext

Thin wrapper around GNS library init/teardown. Isolates GNS headers from the rest of the engine. Statically linked. One per process lifetime.

### NetConnectionManager

Socket API: `Listen(port)` / `Connect(address, port)`, `PollIncoming()` / `Send()`.

Per-connection state (`ConnectionInfo`): GNS handle, OwnerID, sequence numbers, RTT, ack bitfield, `ClientRepState` machine. Max simultaneous connections = 2^`NetOwnerID_Bits` (currently 8 bits → 256 connections).

### NetThread

Dedicated network poller. Default rate: **30Hz** (configurable via INI `NetworkUpdateHz`). Runs `HandleMessage` dispatch loop.

| Message | Direction | Handler |
|---|---|---|
| `ConnectionHandshake` | Owner→Authority | Assign OwnerID, send ack |
| `InputFrame` | Owner→Authority | Route to World `GetSimInput()` by OwnerID |
| `EntitySpawn` | Authority→Owner | `ReplicationSystem::HandleEntitySpawn` |
| `StateCorrection` | Authority→Owner | `ReplicationSystem::HandleStateCorrections` |
| `FlowEvent` | Authority→Owner | `FlowManager::PostNetEvent` (TravelNotify, ServerReady) |
| `ClockSync` | Both | RTT + clock offset estimation |
| `PlayerBeginRequest` | Owner→Authority | Soul RPC: Owner requests spawn |
| `PlayerBeginConfirm` | Authority→Owner | Soul RPC: Authority confirms spawn |
| `PlayerBeginReject` | Authority→Owner | Soul RPC: Authority rejects spawn |
| `Ping/Pong` | Both | RTT EWMA (0.875/0.125 weights) |

### NetChannel

Per-connection typed send API — the natural home for all future per-connection state (delta compression baselines, coalescing buffers, reliability policy tables, RPC dispatch).

```cpp
class NetChannel
{
    uint8_t OwnerID;

    template<typename T>
    void Send(NetMessageType type, const T& payload);

    template<typename T>
    void SendTo(uint8_t targetOwnerID, NetMessageType type, const T& payload);

    void Flush();
};
```

All gameplay messages route through the Authority. The Owner specifies a target `OwnerID`; the Authority relays without inspecting the payload. This is addressed routing, not multicast — no "everyone filters it" overhead.

---

## `LogicThread<TNet, TRollback, TFrame>` — Sim Mode Policy

`LogicThread` is templatized on three policy axes to eliminate all in-line Authority/Owner branching:

```cpp
template <typename TNet, typename TRollback, typename TFrame>
class LogicThread : public LogicThreadBase { ... };
```

**Net policy types:**

- **`AuthoritySim`** — `OnSimInput` injects per-player `PlayerInputLog` from `ServerClientChannel`; `OnFramePublished` advances `CommittedFrameHorizon`
- **`OwnerSim`** — `OnSimInput` pushes to `InputAccumRing`; `OnFramePublished` is a no-op
- **`SoloSim`** — both are no-ops

Dead code never compiles in a given build. Add new Authority/Owner branching to the sim mode policy — not to `LogicThread` directly.

---

## `ServerClientChannel` — Per-Client Replication State

One per connected Owner. Replaces a single global `Replicated[]` bitvector. O(1) OwnerID lookup.

```cpp
struct ServerClientChannel {
    PlayerInputLog        InputLog;            // inbound input frames from this Owner
    std::vector<bool>     Replicated;          // per-entity spawn-tracking
    std::vector<uint32_t> PendingActivations;  // net handles queued for EntityActivate
    NetChannel            Channel;             // typed per-connection send wrapper
    PendingPacketQueue    SendQueue;           // lock-free MPSC; drained by Sentinel
    ConnectionInfo*       CI;
    uint8_t               OwnerID;
    uint32_t              LastAckedSimFrame;
};
```

`ReplicationSystem` holds `Clients[MaxOwnerIDs]` for O(1) lookup. Lives inside the `World` it belongs to — PIE worlds are naturally isolated.

---

## Network Identity

### EntityNetHandle

Packed `uint32_t`: `NetOwnerID:8 | NetIndex:24`. OwnerID 0 = Authority/global, 1–255 = Owners. NetIndex allocated per entity on the Authority. `NetToRecord[]` maps NetIndex → `EntityRecord`.

**Three handle spaces in Registry:**

| Handle | Space | Purpose |
|---|---|---|
| `GlobalEntityHandle` | Internal | Generation + Records[] index |
| `EntityHandle` | Local OOP | LocalToRecord mapping |
| `EntityNetHandle` | Network | NetToRecord mapping, wire-safe |

### EntityRef vs EntityNetHandle (Two-Tier)

**Internal (32-bit) `EntityNetHandle`:** Used exclusively inside engine systems. Compact, fast, no generation. Valid on bulk paths where the Authority is pushing state.

**External (64-bit) `EntityRef`:** Used in gameplay code, RPCs, and all Owner→Authority messages. Embeds the generation captured at spawn time for ABA protection.

```cpp
struct EntityRef {
    EntityNetHandle Handle;      // 32-bit wire handle
    uint16_t        Generation;  // from GlobalEntityHandle::Generation at spawn
    uint16_t        Flags;       // IsPredicted:1, IsOwned:1, reserved:14
};  // 8 bytes total
```

When an Owner sends a targeted message, the Authority validates the generation before any state mutation. Stale references are rejected.

### ConstructHandle (Planned)

Pure-logic Constructs (Soul, GameMode, AIDirector) have no `EntityNetHandle` — they're not entities. `ConstructHandle` fills this gap:

```cpp
struct ConstructHandle {  // fits in uint32_t
    uint8_t  OwnerID;     // Soul that owns this Construct (0 = Authority-owned)
    uint16_t LocalIndex;  // index into that owner's ConstructRegistry PagedMap
    uint8_t  Generation;  // stale-handle detection
};
```

RPCs targeting a Construct as a whole use `ConstructHandle`. Multi-entity Constructs (Turret with barrel + base entities) have multiple `EntityNetHandle`s but one `ConstructHandle`.

---

## PIE Loopback

Editor creates Authority + N Owner Worlds in the same process with loopback GNS connections.

- Each Owner World gets its own OwnerID, viewport, field slab, and InputBuffer
- Authority World runs headless (no renderer)
- Input routes to the focused viewport's World via `InputTargetWorld`
- Both `AuthorityNet::HandleMessage` and `OwnerNet::HandleMessage` run in-process; `WorldMap[ownerID]` routes messages to the correct World

PIE is not a simulation of networking — it IS networking over a loopback socket. Bugs found in PIE are real bugs.

**PIE disambiguation note:** In PIE, two `ConnectionInfo` entries share the same OwnerID (one `bServerSide=true`, one `bClientInitiated=true`). `FindConnectionByOwnerID` must be called with `requireServerSide=true` when the Authority side needs to set state on its own leg.

---

## Gated Push Replication

```
PublishCompletedFrame → OnFramePublished → dispatch one read-only job per Loaded+ client
```

Each job owns its `ServerClientChannel` exclusively — zero contention. Replication state correction and spawn sends are read-only against the ECS slab; they run async alongside the render thread. Logic rate (512Hz) and net rate (30Hz) are decoupled.

---

## Known Gaps

- No interest management / relevancy culling — Authority sends all entities to all Owners today
- Phase 0 tentative despawn not implemented — see [Despawn Protocol](Despawn-Protocol.md)
