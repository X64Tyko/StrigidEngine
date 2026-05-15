# Connection Flow

> [← Overview](Overview.md) | [Entity Replication →](Entity-Replication.md) | [Home](../Home.md)

---

## Overview

A full multiplayer connection passes through four sequential phases. The `ClientRepState` machine (one instance per connected Owner, living on the Authority's `ConnectionInfo`) tracks progress. Transitions are driven by network events on the NetThread and forwarded to `FlowManager`'s event queue for processing on the Sentinel thread.

```cpp
enum class ClientRepState : uint8_t
{
    PendingHandshake, // GNS connected; waiting for HandshakeRequest
    Synchronizing,    // Handshake accepted; clock sync probes in flight
    Loading,          // FlowEvent::LoadLevel sent; waiting for ClientReady
    LevelLoading,     // Level load in progress on Owner
    LevelLoaded,      // Owner level loaded; waiting for Mode approval
    Loaded,           // Mode approved; waiting for initial replication flush
    Playing,          // Full replication active; PlayerBeginConfirm sent
};
```

---

## Phase 1 — Connection & Handshake

```
Owner                              Authority
  │
  ├── GNS Connect ──────────────────►│
  ├── HandshakeRequest ─────────────►│  version, client UUID
  │◄──────── HandshakeAccept ────────┤  OwnerID, tick rate, server frame#
  │
  │  PingProbe ×N ──────────────────►│  RTT measurement (N ≈ 8 probes)
  │◄──────── PongProbe ×N ──────────┤
  │
  ├── ClockSyncRequest ─────────────►│  client timestamp
  │◄──────── ClockSyncResponse ─────┤  server frame# + server timestamp
  │  Owner computes InputLead         │  (InputLead = RTT/2 + jitter buffer)
  │
  ├── ClientReady ──────────────────►│
  │
  │  [RepState: PendingHandshake → Synchronizing]
```

**What happens:**
- GNS establishes the transport connection
- `HandshakeRequest` carries the client's engine version and a stable UUID; the Authority assigns an `OwnerID` (1–255) and creates a `Soul` for that connection
- RTT is measured via ~8 Ping/Pong probes; an EWMA (0.875 old / 0.125 new weight) is maintained throughout the session
- `ClockSync` establishes the **Base Frame Offset** — how many frames ahead the Owner must timestamp inputs so they arrive at the Authority before the Authority simulates that frame. Formula: `RTT/2 + jitter_buffer`. This is the stable target client-server relationship for the session, not a zero-lag target; subsequent drift is measured relative to it, not relative to zero. Cristian's algorithm with 5–6 RTT samples (median) is used for measurement; the server time offset is refreshed periodically during the session.
- Three drift correction mechanisms operate against the Base Frame Offset:
  - **Fast-forward:** client is behind the target offset — rollback ring buffer used to consume ticks faster than real time
  - **Stall:** client is ahead of the target offset — hold one tick
  - **Disconnect policy:** drift exceeds the variance window — health metrics fire, game callback decides action
- `ClientReady` signals clock sync complete; Authority advances `ClientRepState` to `Synchronizing`

---

## Phase 2 — Level Loading

```
  │◄──── FlowEvent::LoadLevel(uuid) ─┤  Authority asks Mode for current level UUID
  │                                   │
  │  Owner:                           │
  │  - Transitions to LoadingState    │
  │  - Creates World                  │
  │  - Loads .tnxscene from UUID      │
  │                                   │
  ├── FlowEvent::ClientReady ────────►│
  │◄──── FlowEvent::ServerReady ─────┤  Mode decides when all initial spawns are done
  │                                   │
  │  [RepState: Synchronizing → Loading → LevelLoading → LevelLoaded → Loaded]
```

**What happens:**
- Authority sends `FlowEvent::LoadLevel` carrying the 16-byte scene asset UUID
- Owner's `FlowManager` transitions its `FlowState` to `LoadingState`, creates a `World`, loads the `.tnxscene` file
- When loading completes, Owner sends `FlowEvent::ClientReady`
- Authority runs `ReplicationSystem::SendSpawns` — all currently-alive entities are replicated to the new Owner. After flushing all initial spawns, Authority sends `FlowEvent::ServerReady`
- Owner receives `ServerReady` and sweeps all entities from `Alive → Active` (entities were spawned in an Alive but inactive state, invisible until the full initial set arrives)
- Authority advances `ClientRepState` from `Loading` through `LevelLoading` → `LevelLoaded` → `Loaded`

**`FlowEventPayload` wire format:**

```cpp
struct FlowEventPayload
{
    uint8_t  EventType;      // FlowEventType discriminator
    uint8_t  Flags;          // Reserved; zeroed
    uint16_t PayloadSize;    // Trailing data size in bytes
    uint8_t  AssetUUID[16];  // Populated for Load/Unload events; zeroed otherwise
};

enum class FlowEventType : uint8_t
{
    LoadLevel,    // Authority → Owner: load this level UUID
    LoadAdditive, // Authority → Owner: load on top of current level
    UnloadLevel,  // Authority → Owner: unload a specific level
    KillWorld,    // No payload — destroy World, keep session
    KillSession,  // No payload — destroy session entirely
    ClientReady,  // Owner → Authority: finished loading, ready for spawn
    ServerReady,  // Authority → Owner: all initial spawns sent
};
```

---

## Phase 3 — Spawn (Client-Predicted)

```
  ├── PlayerBeginRequest ───────────►│  ClassID, desired position, PredictionID
  │                                  │
  │  Owner predicts locally:         │  Authority validates via Mode:
  │  - Creates Body Construct        │  - Mode::OnPlayerBeginRequest(soul, req)
  │  - Attaches ConstructView        │  - Mode picks authoritative spawn point
  │  - Stores in PredictionLedger    │  - Creates entity, replicates to others
  │                                  │
  │◄──── PlayerBeginConfirm ────────┤  NetHandle, frame#, auth position, PredictionID
  │  or                              │
  │◄──── PlayerBeginReject ─────────┤  reason code, PredictionID
  │                                  │
  │  If confirmed:                   │
  │  - Wire NetHandle → entity       │
  │  - Soul.ActiveNetHandle = handle │
  │  - Reconcile position if needed  │
  │                                  │
  │  If rejected:                    │
  │  - Destroy predicted Body        │
  │  - Clear PredictionLedger entry  │
  │                                  │
  │  [RepState: Loaded → Playing]
```

**What happens:**
- Owner sends `PlayerBeginRequest` with the desired character class and a `PredictionID`
- Owner immediately creates a predicted `Body` Construct and stores the prediction in its `PredictionLedger` (keyed by `PredictionID`)
- Authority calls `Mode::OnPlayerBeginRequest(soul, req)`. The Mode validates the request, picks an authoritative spawn point, creates the ECS entity, and replicates it
- On confirm: the Owner uses the echoed `PredictionID` to look up the predicted Body in `PredictionLedger`, wires the authoritative `NetHandle`, and reconciles position if the Authority's spawn point differs from the prediction
- On reject: the Owner destroys the predicted Body and clears the `PredictionLedger` entry

**`PredictionLedger`** tracks in-flight spawn predictions:

```cpp
struct PredictionEntry
{
    uint16_t  PredictionID;   // echoed by Authority in Confirm/Reject
    uint32_t  RequestFrame;   // logic frame when request was sent
    LocalRef  BodyRef;        // handle to the predicted Body Construct
    UUID      PrefabUUID;     // which prefab was predicted
};
```

---

## Phase 4 — Playing

```
  ├── InputFrame (every logic tick) ►│  Owner sends input at logic rate
  │◄──── StateCorrection (batched) ──┤  unreliable, sent at NetworkUpdateHz (30Hz)
  │                                  │
  │  Owner predicts locally           │  Authority is authoritative
  │  Corrects on misprediction        │
  │  Heartbeat / RTT continuous       │
```

**What happens:**
- Owner sends `InputFrame` messages containing delta-compressed input state for the current logic tick. `InputDeltaFlags` marks which fields changed (KeyState / MouseDX / MouseDY / MouseButtons / Events)
- Authority receives `InputFrame` messages on `NetThread`, routes them to the correct `PlayerInputLog` in `ServerClientChannel` by `OwnerID`
- `AuthoritySim::OnSimInput` injects `PlayerInputLog` data for each client into the simulation
- Authority batches `StateCorrection` messages (entity transforms) and sends them unreliable at the `NetworkUpdateHz` rate (default 30Hz)
- When an Owner's predicted state diverges from a `StateCorrection`, the rollback system triggers: slab rewinds to the correction frame, Jolt snapshot restores, the simulation resimulates forward. `ResimFrameDelta` in the correction annotates the exact resim root frame.

---

## PIE Disambiguation Note

In PIE, two `ConnectionInfo` entries share the same `OwnerID`: one with `bServerSide=true` (the Authority-accepted leg), one with `bClientInitiated=true` (the Owner-opened leg). `HandleMessage` in `PIENetThread` routes by `bServerSide` — Authority messages go to `AuthorityNet::HandleMessage`, Owner messages go to `OwnerNet::HandleMessage`.

`FindConnectionByOwnerID` must be called with `requireServerSide=true` when the Authority side needs to set state on its own leg, or it will silently mutate the Owner leg (which shares the `OwnerID`).
