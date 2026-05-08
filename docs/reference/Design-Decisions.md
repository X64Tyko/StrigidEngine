# Design Decisions

> [← Status & Roadmap](Status-And-Roadmap.md) | [Known Issues →](Known-Issues.md) | [Home](../Home.md)

A record of non-obvious architectural decisions, tradeoffs considered, and why the chosen path was taken.

---

## Simulation Rate — 512Hz

The fixed update rate started at 512Hz as an ambitious target inspired by knowing that games like Valorant run at 128Hz. Early profiling showed 256Hz was trivially achievable, so the target doubled. The 1.95ms/frame budget is tight enough to force discipline but has been met comfortably at 100K+ entities.

The rate is a load-bearing constraint, not a tunable knob. Everything downstream — input timestamping, physics divisor, rollback depth, clock sync — is designed assuming a stable, known tick rate. Variable timestep for authoritative simulation is incompatible with rollback netcode and deterministic replay.

The engine has a variable-rate update — the **Scalar Update** tick — which has existed since the beginning. Camera, cosmetics, UI, and non-authoritative Construct ticks run outside the fixed loop at whatever rate the machine can sustain. This is an explicit split: fixed rate for everything deterministic; variable rate for everything that only needs to look smooth.

---

## Why Three Threads, Not Two or Four

Two threads (logic + render) is the conventional split. The problem: render thread stalls on VSync hold the slab read lock, which blocks logic. Three threads — Sentinel, Brain, Encoder — isolates input polling from both. Sentinel runs at 1000Hz and is never blocked by physics or GPU.

Four dedicated threads (separate physics thread) was considered and rejected. Jolt's internal job system already parallelises physics work across the worker pool. A dedicated physics thread would steal a core the worker pool uses more effectively. Brain acts as a physics coordinator: it submits Jolt jobs and then steals from the physics queue while waiting.

---

## Brain and Encoder as Coordinators, Not Workers

On an 8-core machine: 1 Sentinel + 1 Brain + 1 Encoder = 5 workers remaining. If Brain and Encoder were pure coordinators (blocked waiting for jobs), that's 5 effective cores. By acting as workers themselves while waiting, you get 6 effective logic cores and 6 effective render cores — ~20% throughput gain for free. No extra threads, no extra synchronisation.

---

## SoA Field Arrays + FieldProxy OOP Syntax

SoA is the right layout for SIMD batch processing — all PosX values contiguous, all PosY values contiguous, near-100% cache-line utilisation during sweeps. The problem: SoA is hostile to component authors. Writing `posXArray[i] += velXArray[i] * dt` for every field leaks the data layout into gameplay code.

`FieldProxy<T, WIDTH>` wraps raw array pointers behind operator overloads so that `transform.PosX += velocity.VelX * dt` compiles to a direct SoA array access. Gameplay authors write OOP-style code; the engine layout is completely opaque. No virtual dispatch, no map lookups — `operator T()` and `operator=` are direct pointer dereferences.

The three widths (Scalar / Wide / WideMask) let the same component type participate in both scalar Construct ticks and 8-wide AVX2 entity sweeps without any code change at the component level.

---

## Tiered Storage — Why Four Tiers

A single SoA ring buffer for everything would waste memory on entities that never roll back and waste bandwidth on entities that never render. The tier is declared on the **component**, not the entity. An entity's effective tier is the highest tier of any of its components — a single macro change at the component level promotes an entire class of entities to rollback-capable.

| Tier | Ring depth | Rationale |
|---|---|---|
| Cold | 0 | Rarely-updated config data. AoS in chunks. |
| Static | 0 | Read-only geometry. Separate array. |
| Volatile | 3 | Triple-buffer for Logic↔Render handoff. No rollback needed. |
| Temporal | N | Rollback history. Only entities that matter for netcode pay the cost. |

When `TNX_ENABLE_ROLLBACK` is off, Temporal is treated as Volatile. Games without rollback pay zero memory cost.

---

## Volatile = 3 Frames (Not More)

Originally 5 frames were used for render thread headroom. After moving to GPU-driven rendering with a persistent previous-frame InstanceBuffer on the GPU side, the render thread only needs frame T — the GPU interpolates T-1 from its own buffer. The CPU slab needs only 1 frame for logic and 1 for render simultaneously, so triple-buffer (3) is the correct minimum. 5 frames wasted memory.

---

## Dual-Ended Arena Partition Layout

Physics must iterate players + AI + physics props densely. Rendering must iterate particles + decals + players densely. These sets overlap but are not identical. A single contiguous array forces both systems to skip irrelevant entities — gap-skipping adds branches and breaks prefetch.

The dual-ended arena solves this with zero padding overhead:

```
Arena 1:  [RENDER →] .... [← DUAL]
Arena 2:  [PHYS →]   .... [← LOGIC]
```

Physics iterates DUAL + PHYS contiguously across the arena boundary — a dense wall. Render iterates RENDER + DUAL with one gap in Arena 1 — handled by the GPU predicate pass at negligible cost. The group is auto-derived from component `SystemGroup` tags — no manual annotation, which would be a footgun.

---

## Why Jolt Owns Physics State

The conventional pattern: ECS pushes transforms to physics every frame, physics steps, ECS pulls results. This requires pushing every entity's state even when nothing changed.

Jolt owns physics state here. The ECS only writes to Jolt on explicit overrides (spawn, teleport, impulse, kinematic target). After each step, only **awake** bodies are pulled back. 50K bodies where 200 are moving pays for 200 pulls, not 50K.

Tradeoff: velocities live in Jolt, not the ECS. Gameplay code that needs velocity queries Jolt directly during ScalarUpdate. This is acceptable — velocity reads are rare and scalar.

---

## Physics Divisor and Rollback

Running Jolt at 512Hz is prohibitively expensive for any meaningful scene. The 8:1 divisor (64Hz physics) is configurable. For rollback: when rolling back to frame N, snap to the nearest Jolt execution frame at or before N (rollback to 100 → restore from 96 at 8:1), restore Jolt state, resimulate. At most 7 frames of physics approximation — acceptable for competitive multiplayer.

Rebuild-from-slab (positions only) was tested and rejected: a cold solver restart diverges because constraint forces re-initialize from scratch. Snapshot restore is the only correct approach.

---

## Constructs vs Entities

Two fundamentally different things exist in a game:

- **The horde** — zombies, bullets, particles. Homogeneous. High count. No bespoke logic. Swept by SIMD. → **Entity**
- **The thinkers** — Player, GameMode, AIDirector. Singular. Complex. Bespoke logic. → **`Construct<T>`**

Forcing the horde into Constructs makes SIMD batch processing impossible. Forcing the thinkers into ECS makes per-object logic unnatural. The split lets entity authors write SIMD-swept code while Construct authors write OOP code. Neither pays the cost of the other.

---

## CRTP vs Virtual for Constructs

`Construct<T>` uses CRTP. Tick hooks are auto-registered via `if constexpr` concept detection. If you implement the method, you get the tick. If you don't, you pay nothing.

Virtual functions were explicitly ruled out for components (SchemaValidation enforces this) because vtables break SoA decomposition. Constructs are scalar so virtual dispatch would technically be fine there, but CRTP was chosen anyway to keep the pattern consistent and to enable compile-time interface contracts via C++20 concepts on `Owned<T>`.

---

## `Owned<T>` for Construct Composition

Complex Constructs compose via `Owned<T>` value members rather than heap allocation or inheritance:

- **Deterministic init/destroy order** — declaration order init, reverse-declaration-order destroy
- **Zero allocation** — owned objects live inline
- **Compile-time interface contracts** — `Owned<T>` can require C++20 concepts (`Targetable`, `Damageable`)
- **Each owned Construct is exactly as heavy as it needs to be** — `TargetingSystem` without a `ConstructView` pays no ECS cost

---

## Soul / Body Split

`Soul` (session identity) and `Body` (world presence) are separate because they have different lifetimes. A Soul survives level transitions — it is the player, not the player's character. A Body is destroyed when the World resets and recreated by the GameMode when the player re-enters gameplay.

This cleanly handles spectators (Soul exists, no Body), disconnected players (Soul in grace period, Body released), and late joiners (Soul created at handshake, Body created when GameMode decides conditions are met).

The pattern is designed to work identically in standalone — Souls are always created, just synthesised locally rather than triggered by a handshake. This unifies the flow graph across all play modes. *(Note: standalone Soul synthesis not yet implemented — see [Known Issues](Known-Issues.md).)*

---

## Three Travel Levers, Not a Single Policy

Most engines expose "seamless travel" vs "hard travel" as a binary. This conflates three orthogonal concerns: what happens to the World, what happens to Constructs, and what happens to the network session.

Trinyx exposes three independent levers: **Domain lifetime** × **Construct lifetime** × **Network continuity**. Games compose these to build their flow. A `FlowState` declares what it requires; `FlowManager` creates and destroys accordingly. The programmer can't forget — they never call the create/destroy functions directly.

---

## NetChannel as the Replication Boundary

`NetChannel` wraps a `ConnectionInfo*` and `ISteamNetworkingSockets*` into a typed send API. The intent is that it becomes the natural home for all per-connection state over time: delta compression baselines, coalescing buffers, reliability policy tables, RPC dispatch.

This keeps the transport (GNS today) entirely behind an implementation boundary. Gameplay calls `channel.Send<T>(type, payload)` and never sees a socket handle.

---

## 5 GPU InstanceBuffers — Breaking the VSync Chain

Without multiple InstanceBuffers: VSync holds the GPU buffer → render thread blocks → render thread holds the slab read lock → logic thread stalls. One stall cascades all the way to the simulation.

5 InstanceBuffers (cycling independently of the 2 in-flight GPU frame slots) ensures the render thread always has a free buffer. If the render thread falls behind by more than 5 frames it becomes a renderer performance problem — not a synchronisation problem contaminating the simulation.

---

## SoA Is Already the GPU Layout

The temporal component cache is already SoA — all PosX values contiguous, all PosY contiguous. GPU upload path uploads raw slab slices directly into SSBOs via Buffer Device Address. There is no AoS→SoA conversion on the upload path; the data is already in the layout the GPU wants. The decision made for SIMD batch processing turned out to be exactly correct for GPU-driven rendering.

---

## EntityCacheIndex as a Globally Stable Coordinate

The entire engine uses a single flat `EntityCacheIndex` as the column coordinate across all tiers. Any field for any entity is at `(field_array_base + EntityCacheIndex)`. No tier-switching lookup, no secondary index, no translation table.

Consequence: defrag that moves entities must fire identity-change notifications so Views rebind. In determinism builds, defrag of live authoritative entities is disabled. Slot reuse after tombstoning is allowed because it doesn't change existing indices.

---

## Fixed-Point → GPU float32 is the Only Lossy Step

The full simulation pipeline uses Fixed32 (int32, 0.1mm precision). The only conversion to float32 happens at render-thread upload: cell-relative Fixed32 → camera-relative float32 for the GPU. At ≤1km from the camera, float32 gives ≈0.05mm precision — finer than the 0.1mm unit definition. The conversion is lossless in practice.

The entire determinism guarantee lives on the simulation side; the render side gets full GPU float32 throughput with no precision concerns.

---

## Singleplayer and Multiplayer Share One Code Path

`FlowState` transitions, `GameMode` lifecycle, Soul lifecycle, and all `ModeMixins` behave identically across singleplayer, local co-op, and online multiplayer. The network layer is additive. In singleplayer: `TravelNotify` becomes a local `FlowManager::Travel()` call, `NetChannel` is absent, and Souls are synthesised directly by `FlowManager`. Everything else — `WithSpawnManagement`, `WithLobby`, `WithRespawn`, `WithTeamAssignment` — compiles and runs unchanged. Game code written for singleplayer is online multiplayer code.

---

## Constructs Serialize Only Through Views

Construct scalar C++ members are not serialised. Only View-owned ECS data is serialised, flowing through the existing ECS path with no special-case code. If a value needs to survive serialisation (e.g. `TurretBase::MaxAmmo`), it belongs in a component. This is a forcing function: anything worth saving belongs in the data model, not in OOP object state.

Consequence: loading a level is just hydrating ECS data and calling `PostInitialize()`. No save-game pointer fixup, no object graph serialisation, no version migration for OOP members.

---

## FlowState Declaration Contracts Replace Lifecycle Boilerplate

In most engines, the gameplay programmer is responsible for manually creating and destroying the World, NetSession, and physics when transitioning states. Forgetting a step leaks resources or crashes.

`FlowState::GetRequirements()` declares what a state needs; `FlowManager` creates and destroys accordingly. The programmer can't forget — they never call the create/destroy functions. The declaration is the contract; the engine enforces it.
