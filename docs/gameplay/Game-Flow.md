# Game Flow

> [← Constructs & Views](Constructs-And-Views.md) | [Physics →](Physics.md) | [Home](../Home.md)

---

## Overview

The game flow system manages **session lifecycle, level transitions, player identity persistence, and network-driven spawn flow**. It sits above the Construct/View OOP layer and networking subsystem.

**Engine/GameMode Boundary:**

```
Engine owns                          GameMode owns
───────────────────────────────      ──────────────────────────────────
HandshakeAccept                      GameModeManifest
ClockSync                            ClientModeManifest
TravelNotify                         ReadyUp / lobby countdown
LevelReady (structural ack)          LevelReady game payload
Entity replication (Alive/Active)    Spawn timing decision
OnPlayerJoined(soul) ──────────────► Everything after this line
OnPlayerLeft(soul)                   Cleanup, respawn, grace period
```

---

## Vocabulary

| Concept | Engine Type | Description |
|---|---|---|
| **State** | `FlowState` subclass | Application state node — owns World/Level/NetSession lifetime. Examples: `MainMenuState`, `LoadingState`, `InGameState` |
| **Mode** | `GameMode` base class | Server-authoritative match rules. Validates spawns, manages rounds. One per World. |
| **Level** | Loaded `.tnxscene` | Content chunk. Loaded and unloaded within a World. |
| **Soul** | `Soul` class | Session-scoped player identity. Holds `OwnerID`, input routing, confirmed body handle. NOT a Construct. |
| **Body** | `Construct<T>` with `ConstructLifetime::World` | A Soul's world presence. Created by the Mode; destroyed on World reset. |
| **GamePhase** | User-defined sub-state | Optional sub-state inside a GameMode (`Lobby`, `Warmup`, `Playing`). Engine-invisible — GameMode runs its own phase machine. |

> **Mental model:** State drives the app. Mode drives the match. Level drives the content. Souls persist identity. Bodies are world presence.

---

## Ownership Hierarchy

```
FlowManager  (Sentinel thread — manages state stack + transitions)
  │
  ├── ConstructRegistry  (ALL Constructs, regardless of lifetime tier)
  │     ├── Persistent-lifetime: MetaGameManager, CampaignTracker
  │     ├── World-lifetime:      PlayerBody(s), AIDirector
  │     └── Level-lifetime:      TurretController, DoorTrigger
  │
  ├── Souls[]  (indexed by OwnerID — owned by FlowManager, NOT Constructs)
  │
  ├── FlowState stack
  │     ├── active:   InGameState
  │     └── overlay:  PauseMenuState  (optional)
  │
  ├── NetSession  (optional — GNS context, NetThread, Connections)
  │
  └── World  (optional — ECS Registry, LogicThread, JoltPhysics, Level data)
```

`ConstructRegistry` is owned by `FlowManager` (not `World`). This allows `Persistent`-lifetime Constructs to survive World destruction and be reattached to a new World.

---

## FlowManager

`FlowManager` runs on the Sentinel thread. It manages:
- A **state stack** (push / pop / transition with declaration enforcement)
- Ownership of the `ConstructRegistry`
- A **Souls array** indexed by OwnerID
- A **thread-safe flow event queue** drained from NetThread each frame

### Bootstrap Contract

```cpp
class MyGame : public GameManager<MyGame>
{
    bool PostInitialize(TrinyxEngine& engine)
    {
        auto& flow = engine.GetFlowManager();
        flow.RegisterState("MainMenu", [](){ return std::make_unique<MainMenuState>(); });
        flow.RegisterState("InGame",   [](){ return std::make_unique<InGameState>(); });
        flow.RegisterMode("Arena",     [](){ return std::make_unique<ArenaMode>(); });
        flow.LoadDefaultState("MainMenu");
        return true;
    }
};
TNX_IMPLEMENT_GAME(MyGame)
```

Engine boots → loads one named state → user code owns the entire flow graph.

### Stack Operations

- `TransitionTo(name)` — replace the entire stack (menu → gameplay)
- `PushState(name)` — overlay (pause menu over gameplay)
- `PopState()` — return to previous state

---

## FlowState

`FlowState` declares what it requires. `FlowManager` creates/destroys subsystems automatically:

```cpp
class FlowState
{
public:
    virtual void OnEnter(FlowManager& flow, World* world) {}
    virtual void OnExit() {}
    virtual void Tick(float dt) {}
    virtual StateRequirements GetRequirements() const { return {}; }
    virtual const char* GetName() const = 0;
};

struct StateRequirements
{
    bool NeedsWorld      = false;
    bool NeedsLevel      = false;
    bool NeedsNetSession = false;
    bool AllowsSouls     = true;
};
```

```cpp
class MainMenuState : public FlowState
{
    const char* GetName() const override { return "MainMenu"; }
    // GetRequirements() returns {} — no World, no NetSession needed
};

class InGameState : public FlowState
{
    const char* GetName() const override { return "InGame"; }
    StateRequirements GetRequirements() const override
    {
        return { .NeedsWorld = true, .NeedsNetSession = true };
    }
};
```

### Transition Logic

```
1. Compare current vs next state requirements (GetRequirements())
2. If current NeedsWorld and next does not:
       → Destroy World (destroys Level + all World/Level-lifetime Constructs)
       → Persistent-lifetime Constructs stay — untouched
3. If current NeedsNetSession and next does not:
       → Destroy NetSession
4. Call OnExit() on current state
5. Call OnEnter(flow, world) on next state
```

Gameplay programmers can't forget to create/destroy systems — the declaration is the contract and `FlowManager` enforces it.

---

## Construct Lifetime Tiers

```cpp
enum class ConstructLifetime : uint8_t
{
    Level,      // Destroyed when the Level unloads
    World,      // Destroyed when the World resets
    Session,    // Survives World reset. Destroyed when the session ends.
    Persistent, // Survives everything. Destroyed only explicitly.
};
```

Constructs surviving a World reset receive:
- `OnWorldTeardown()` — Views about to be invalidated; null out entity handles, save needed state
- `OnWorldInitialized(World*)` — Fresh World ready; reinitialize Views, re-create entities

---

## Travel — Level Transitions

Travel is three orthogonal levers combined independently. There is no single "travel" policy.

### Lever A — Domain Lifetime

| Travel Type | World | Level | Session |
|---|---|---|---|
| Keep World, Swap Level | ✓ | ✗ | ✓ |
| Reset World | ✗ | ✗ | ✓ |
| Keep nothing | ✗ | ✗ | Soul only |

### Lever B — Construct Lifetime

| Lifetime | Survives World Reset | Survives Level Swap |
|---|---|---|
| `Level` | ✗ | ✗ |
| `World` | ✗ | ✓ (if World survives) |
| `Session` | ✓ | ✓ |
| `Persistent` | ✓ | ✓ |

### Lever C — Network Continuity

- **Keep NetSession** — same Authority connection, no reconnect
- **Swap NetSession** — server handoff (MMO zone transition, dedicated server rotation)

### Example Travel Scenarios

**Arena shooter — between rounds:**
Mode sends `FlowEvent::LoadLevel(nextArenaUUID)`. Bodies (World-lifetime) are destroyed. Souls survive. Mode resets round state in `OnWorldInitialized`. Souls request new Bodies when `ServerReady` arrives.

**Roguelike — floor transition:**
Mode calls `TransitionWorld(nextFloorUUID, ResetWorld)`. World torn down. Persistent-lifetime Constructs survive via `OnWorldTeardown` / `OnWorldInitialized`. New Bodies spawn when client completes loading.

**MMO — server handoff:**
Souls serialize state to a transfer record. NetSession swaps. Souls restore state on new Authority. Bodies spawn fresh.

---

## Soul / Body Pattern

**Soul** — session-scoped identity (not a Construct). Created at network handshake (or synthesized directly by FlowManager for singleplayer). Survives World resets.

**Body** — the player's world presence (`Construct<T>` with `ConstructLifetime::World`). Created by GameMode when spawn conditions are met. Destroyed on World reset.

```
OnPlayerJoined(soul)
    │
    ├─ engine: Soul created, OwnerID assigned
    ├─ game:   GameModeManifest sent
    │          ClientModeManifest received, stored on Soul
    ├─ game:   Phase → Playing, server spawns Body
    │          Soul::ClaimBody(ConstructRef) called by GameMode
    │          Soul::OnBodyConfirmed() fires on Owner side
    └─ Soul.ConfirmedBodyHandle valid — input routing live
```

This cleanly handles edge cases:
- **Spectators:** Soul exists, no Body
- **Disconnected players:** Soul in grace period, Body released
- **Late joiners:** Soul created at handshake, Body created when Mode decides spawn conditions are met

### Singleplayer and Local Co-op

The engine/GameMode boundary is **identical** to the multiplayer case. In singleplayer, `TravelNotify` becomes a local `FlowManager::Travel()` call, and Souls are synthesized directly by `FlowManager` rather than triggered by a network handshake. In local co-op, `FlowManager` creates one Soul per connected controller. GameMode sees `OnPlayerJoined` fire per Soul and spawns Bodies normally — completely unaware whether players are local or remote.

| | Singleplayer | Local Co-op | Online Multiplayer |
|---|---|---|---|
| FlowState machine | ✓ same | ✓ same | ✓ same |
| GameMode API | ✓ same | ✓ same | ✓ same |
| Soul lifecycle | ✓ (local) | ✓ (per pad) | ✓ (per connection) |
| ModeMixins | ✓ all work | ✓ all work | ✓ all work |
| NetChannel | ✗ absent | ✗ absent | ✓ |

---

## ModeMixin System

GameModes compose opt-in CRTP mixins for common feature sets. No mixin = no cost.

```cpp
class ArenaMode : public GameMode
               , public WithSpawnManagement<ArenaMode>
               , public WithTeamAssignment<ArenaMode>
               , public WithLobby<ArenaMode>
{
    ConstructRef GetCharacterPrefab(const Soul& soul) override;
    void OnTeamAssigned(Soul& soul, uint8_t team) override;
};
```

### Engine-defined Mixin Surface

| Mixin | IDs | Override Points |
|---|---|---|
| `WithSpawnManagement<T>` | 16–19 | `GetCharacterPrefab`, `ValidateSpawn`, `OnSpawnConfirmed` |
| `WithTeamAssignment<T>` | 20–23 | `AssignTeam`, `OnPreferenceReceived` |
| `WithLobby<T>` | 24–27 | `OnAllReady`, `GetCountdownDuration` |
| `WithRespawn<T>` | 28–31 | `GetRespawnDelay`, `OnRespawnReady` |
| `WithSpectator<T>` | 32–35 | `CanSpectate`, `OnSpectatorJoined` |

User-defined mixins register via `TNX_REGISTER_MODEMIX(MyMixin)` and receive message type IDs from band 128–255.

---

## FlowEvent — Network Flow Control

`FlowEvent` (`NetMessageType::FlowEvent = 7`) routes flow control messages from NetThread to FlowManager's event queue. It is never handled inline on the NetThread.

```cpp
enum class FlowEventID : uint8_t
{
    TravelNotify,         // Server → Client: load level UUID + mode name
    ServerReady,          // Server → Client: all initial spawns sent
    PlayerBeginConfirm,   // Server → Client: spawn accepted (ConstructNetHandle, EntityNetHandle, authPos)
    PlayerBeginReject,    // Server → Client: spawn rejected (reason code)
};
```

---

## ClientRepState

Server-side per-connection state machine:

```cpp
enum class ClientRepState : uint8_t
{
    PendingHandshake, // GNS connected; waiting for HandshakeRequest
    Synchronizing,    // Handshake accepted; clock sync probes in flight
    Loading,          // FlowEvent::LoadLevel sent; waiting for ClientReady
    LevelLoading,     // Level load in progress on client
    LevelLoaded,      // Client level loaded; waiting for Mode approval
    Loaded,           // Mode approved; waiting for initial replication flush
    Playing,          // Full replication active; PlayerBeginConfirm sent
};
```

See [Connection Flow](../networking/Connection-Flow.md) for the full 4-phase sequence.
