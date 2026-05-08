# Configuration

> [← Build Options](Build-Options.md) | [Home](../Home.md)

---

## `EngineConfig`

All engine timing, threading, and memory parameters live in `EngineConfig` (`src/Runtime/Core/Public/EngineConfig.h`). Set fields before calling `TrinyxEngine::Initialize()`.

```cpp
struct EngineConfig
{
    // === Mode ===
    EngineMode Mode = EngineMode::Standalone;

    // === Timing ===
    int TargetFPS             = 0;     // 0 = uncapped render rate
    int FixedUpdateHz         = 128;   // logic rate (512 = engine default)
    int PhysicsUpdateInterval = 8;     // PhysicsHz = FixedUpdateHz / this
    int NetworkUpdateHz       = 30;    // state corrections / net tick
    int InputNetHz            = 128;   // InputFrame send rate (decoupled from net tick)
    int ClockSyncProbes       = 8;     // RTT probe count during Synchronizing phase
    int InputPollHz           = 1000;  // Sentinel polling rate
    int InputDelayFrames      = 0;     // lockstep artificial delay (0 = disabled)
    int MaxClientInputLead    = 16;    // Authority stall threshold (frames)

    // === Entity Budgets ===
    int MAX_RENDERABLE_ENTITIES = 11000;  // Arena 1 size (RENDER + DUAL)
    int MAX_JOLT_BODIES         = 8000;   // Jolt body array size (independent)
    int MAX_CACHED_ENTITIES     = 25000;  // Total both arenas

    // === History ===
    int TemporalFrameCount = 8;   // ring buffer depth (must be power of 2, minimum 8)
    int JobCacheSize       = 16 * 1024;

    // === Paths ===
    char ProjectDir[512]    = "";
    char DefaultScene[256]  = "";
    char DefaultState[256]  = "";

    // === Networking ===
    uint16_t NetPort        = 27015;
    char NetAddress[128]    = "127.0.0.1";
};
```

---

## Timing Fields

### `FixedUpdateHz`

The Brain thread's fixed simulation rate. All deterministic simulation state advances at this rate.

Common values: 60 (RPG), 128 (standard game), 256, 512 (competitive).

`PhysicsHz = FixedUpdateHz / PhysicsUpdateInterval`. Default: 512/8 = 64Hz physics.

### `TemporalFrameCount`

Ring buffer depth for Temporal-tier components. Must be a power of 2, minimum 8.

Examples at 512Hz:

| Value | History window | Use case |
|---|---|---|
| 8 | ~15.6ms | Minimum — render handoff + one rollback step |
| 16 | ~31.2ms | Client prediction window |
| 64 | ~125ms | Lag compensation, replay |
| 128 | ~250ms | GGPO-style full rollback |

When `TNX_ENABLE_ROLLBACK` is not defined, Temporal entities fall back to 3-frame triple-buffer (same as Volatile). The `TemporalFrameCount` setting is ignored.

### `MaxClientInputLead`

How many simulation frames ahead of a client's last confirmed input the Authority is willing to simulate before stalling. At 512Hz sim / 128Hz input send rate = 4 frames per input batch → default 16 frames = 4 batches of headroom.

---

## Entity Budget Configuration

Two fixed-size dual-ended arenas:

```
Arena 1: Renderable  [0 .. MAX_RENDERABLE_ENTITIES)
  RENDER (→) from 0              — render-only (particles, decals, ambient props)
  DUAL   (←) from MAX_RENDERABLE — physics + render (players, AI, physics props)

Arena 2: Cached  [MAX_RENDERABLE_ENTITIES .. MAX_CACHED_ENTITIES)
  PHYS  (→) from MAX_RENDERABLE  — physics-only bodies, triggers
  LOGIC (←) from MAX_CACHED      — logic/rollback-only entities
```

Validation rules (checked at startup):
```
MaxRenderEntities + MaxDualEntities <= MAX_RENDERABLE_ENTITIES
MAX_RENDERABLE_ENTITIES <= MAX_CACHED_ENTITIES
```

Oversizing a partition wastes memory. Undersizing causes a startup assertion failure.

`MAX_JOLT_BODIES` sizes Jolt's body arrays independently from the arena layout.

---

## Configuration Presets

### Single-Player / RPG

```cpp
EngineConfig cfg;
cfg.FixedUpdateHz         = 60;
cfg.PhysicsUpdateInterval = 4;    // 15Hz physics
cfg.MAX_RENDERABLE_ENTITIES = 10000;
cfg.MAX_CACHED_ENTITIES   = 50000;
cfg.TemporalFrameCount    = 8;
cfg.NetworkUpdateHz       = 0;
```

### Balanced (Standard Online Game)

```cpp
EngineConfig cfg;
cfg.FixedUpdateHz         = 128;
cfg.PhysicsUpdateInterval = 2;    // 64Hz physics
cfg.MAX_RENDERABLE_ENTITIES = 25000;
cfg.MAX_CACHED_ENTITIES   = 100000;
cfg.TemporalFrameCount    = 16;   // ~125ms history @ 128Hz
cfg.NetworkUpdateHz       = 30;
```

### Competitive (512Hz, Full Rollback)

```cpp
EngineConfig cfg;
cfg.FixedUpdateHz         = 512;
cfg.PhysicsUpdateInterval = 8;    // 64Hz physics
cfg.InputPollHz           = 1000;
cfg.MAX_RENDERABLE_ENTITIES = 15000;
cfg.MAX_CACHED_ENTITIES   = 25000;
cfg.TemporalFrameCount    = 128;  // ~250ms rollback @ 512Hz
cfg.NetworkUpdateHz       = 60;
// Requires TNX_ENABLE_ROLLBACK
```

### Simulation (Maximum Entities)

```cpp
EngineConfig cfg;
cfg.FixedUpdateHz         = 60;
cfg.MAX_RENDERABLE_ENTITIES = 50000;
cfg.MAX_CACHED_ENTITIES   = 250000;
cfg.TemporalFrameCount    = 8;
cfg.NetworkUpdateHz       = 0;
```

---

## INI Configuration

Runtime overrides are loaded from `*Defaults.ini` files in the project source directory (scanned from `TNX_PROJECT_DIR`). INI keys map to `EngineConfig` fields by name:

```ini
[Engine]
NetworkUpdateHz = 60
FixedUpdateHz = 512
```

---

## Volatile Frame Count

The Volatile slab is always a 3-frame triple-buffer (hardcoded in `ComponentCache<CacheTier::Volatile>::GetFrameCount`). One frame is being written by Logic, one is being read by the Encoder, and one is free. This is not configurable — and deliberately so after reducing from 5 frames when the GPU-driven render path eliminated the need for the extra CPU-side frames.

## GPU Instance Buffers

Five `PersistentMapped` GPU InstanceBuffers (`InstanceBufferCount = 5`) are maintained by `RendererCore`. This count is hardcoded to decouple the VSync clock from logic thread slab locks and is not exposed through `EngineConfig`.
