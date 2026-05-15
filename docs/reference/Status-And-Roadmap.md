# Status & Roadmap

> [Home](../Home.md) | [Design Decisions →](Design-Decisions.md) | [Known Issues →](Known-Issues.md)

---

## Timeline

**Project start:** ~2026-02-01  
**Current date:** 2026-05-07  
**Phase:** Foundation Stage — networking reliability fix + animation upcoming

---

## Stage 1: Foundation

| # | Milestone | Status | Notes |
|---|---|---|---|
| 1 | **Editor (bare-bones)** | ✅ Complete | 8 panels, ImGuizmo, PIE (local + networked 1–4 clients), scene snapshot/restore, asset database (.tnxid sidecars, .tnxdb), JSON .tnxscene, 50-command undo/redo, GPU picking. See [Editor Overview](../editor/Overview.md). |
| 2 | **Construct/View OOP** | ✅ Complete | `Construct<T>`, `Owned<T>`, `ConstructView<TEntity>`, `ConstructBatch`, JoltCharacter. PlayerConstruct proven. |
| 3 | **Networking** | In Progress | GNS wrapper, PIE loopback, entity replication, clock sync, input routing, delta compression. `LogicThread<TNet,TRollback,TFrame>`, `ServerClientChannel`, `AuthoritySim`/`OwnerSim` done. Two network modes (Deterministic/Non-Deterministic), `ListenNet` mode, host migration, disconnect policy, and mode-split rewrite designed and planned. |
| 4 | **Audio** | ✅ Complete | SDL3 `AudioManager`: voice pool, handle-based playback, event registry, per-voice fade, priority voice stealing. Anti-Event compatible. |
| 5 | **Camera System** | ✅ Complete | `CameraManager` (per-Soul layer stack), `CameraSlot[5]`, `CameraLayer` + mixins, `ECameraNode` (cold), `ECamera` (hot SoA), `CurveHandle`. |
| 6 | **Game Flow** | In Progress | FlowManager, FlowState, GameMode, Soul, NetChannel done. `WithSpawnManagement`, `WithLobby`, `WithTeamAssignment` ModeMixins done. `ClientRepState` 7-state machine done. |
| 7 | **Animation** | Planned | Skeletal animation: pose sampling, bone hierarchy, GPU skinning. Follows replication reliability fix. |

---

## Stage 2: Hardening

Once Editor + Networking + Audio are stable and a test arena level is running, the engine enters a dedicated cleanup and rewrite phase.

**Hardening targets:**

- Hot-path data structure audit for cache efficiency
- Archetype field allocation and meta storage cleanup
- ConstraintEntity system (constraint pool, rigid attachment pass, physics root determination)
- Static entity tier (requires asset importing)
- Reflection system robustness (static init ordering — currently fragile across TUs)
- `TNX_STRIP_NAMES` build option for shipping builds (strip registration name strings)
- Default identity quaternion for `CTransform` (`RotQW=1.0f`) to prevent zero-quaternion rendering bugs

**After hardening:** Arena shooter test level to prove the full stack — high entity counts, physics, competitive input latency, rollback netcode, networked multiplayer.

---

## Completed Work (Selected)

### Architecture

- [x] Three-thread architecture (Sentinel / Brain / Encoder) — fully operational
- [x] Raw Vulkan stack (volk 1.4.304 + VMA 3.3.0, `vk::raii::`) replacing SDL3 GPU backend
- [x] GPU-driven compute pipeline (predicate → prefix_sum → scatter, Slang shaders, BDA)
- [x] Dirty-bit-driven selective GPU upload — only modified entities uploaded per frame (2026-05)
- [x] Lock-free job system (MPMC Vyukov ring buffers, futex-based wake, core-aware pinning)
- [x] Tiered storage partition layout (Cold/Static/Volatile/Temporal, dual-ended arena)
- [x] 5 GPU InstanceBuffers (cycling independently of 2 GPU frame-in-flight slots)

### ECS

- [x] `FieldProxy` (Scalar / Wide / WideMask, `FieldProxyMask` zero-size base)
- [x] `TemporalComponentCache` SoA ring buffer
- [x] `TemporalFlagBits` typed enum (Active, Dirty, DirtiedFrame, Replicated, Alive, Tombstone, …)
- [x] `TNX_TEMPORAL_FIELDS` / `TNX_VOLATILE_FIELDS` with SystemGroup tag (auto-derives partition)
- [x] `SchemaValidation.h` (no vtable + all fields must be FieldProxy — compile-time)

### Physics

- [x] Jolt Physics v5.5.0 (`JoltJobSystemAdapter`, `CJoltBody` volatile, slab-direct iteration)
- [x] `JoltCharacter` — `CharacterVirtual` wrapper for Construct-driven character controllers
- [x] Rollback netcode — `SaveState`/`RestoreState` snapshot ring buffer, `ExecuteRollbackTest`, byte-perfect determinism verified (2026-03-29)

### Networking

- [x] `LogicThread<TNet, TRollback, TFrame>` three-axis policy template (2026-05)
- [x] `AuthoritySim` / `OwnerSim` / `SoloSim` net policy types
- [x] `ServerClientChannel` — per-client: `PlayerInputLog`, `Replicated[]`, `PendingActivations`, `NetChannel`, `PendingPacketQueue`
- [x] `ClientRepState` 7-state machine (PendingHandshake→Synchronizing→Loading→LevelLoading→LevelLoaded→Loaded→Playing)
- [x] Delta compression — `EntityDelta` (component-level dirty patches), `InputFrameDelta` (delta-encoded input window)
- [x] Entity destruction replication (`EntityDestroy` / `ConstructDestroy` wire types)
- [x] Entity activation pipeline (`EntityActivate`, `StreamLoad`/`StreamReady`/`ChunkActivate`)
- [x] `PredictionLedger` — client-side in-flight spawn prediction tracker

### Math

- [x] `Fixed32` (int32, 0.1mm precision, all arithmetic ops, `FixedSqrt`)
- [x] `SimFloat` alias — `SimFloatImpl<float>` or `<Fixed32>` via `TNX_DETERMINISM`
- [x] `FixedTrig` (`FixedSin`/`FixedCos` LUT)
- [x] Jolt fixed-point bridge validated — engine runs deterministically

### Game Flow

- [x] `FlowManager` — state stack, travel primitives, World/Level/Mode lifetime management
- [x] `FlowState` base class with `StateRequirements` declaration hook
- [x] `GameMode` base class + `Construct<T>` opt-in for ticks
- [x] `Soul` (OwnerID identity, `ClaimBody`/`ReleaseBody`, RPC dispatch)
- [x] `NetChannel` typed per-connection send wrapper
- [x] `ModeMixin` system — `WithSpawnManagement`, `WithLobby`, `WithTeamAssignment`
- [x] Travel toolbox — three orthogonal levers (domain lifetime, Construct lifetime, network continuity)
- [x] `GameModeManifest` / `ClientModeManifest` CRTP typed payloads

---

## Not Yet Implemented

### Networking

- [ ] **`ListenNet` compile-time mode** — `ListenSim` TNet policy; `AuthorityClass` tags (`Host`/`Owner`/`Fixed`) for per-entity authority ownership in listen-server scenarios; runtime authority override table for resolution
- [ ] **Host migration (ListenNet)** — handshake protocol, candidate election (ranked by latency + snapshot recency), authority transfer with zero peer gap; prerequisite: snapshot serialization path
- [ ] **Snapshot serialization path** — serialize/deserialize full world state into snapshot format; feeds host migration, save states, late join, and debug replay
- [ ] **Disconnect policy** — `ClientHealthMetrics`, `ClientAction`, `NetDisconnectPolicy` structs; `ClientHealthCallback` for game-owned policy; default threshold enforcement wired to NetThread health checks
- [ ] **Deterministic / Non-Deterministic mode split** — separate `AuthoritySim`/`OwnerSim` paths for non-deterministic mode; uses server timestamps (`server_time_us`) instead of frame numbers, state replication as primary sync signal, variable client Hz, 30Hz snapshot interpolation
- [ ] **Phase 0 tentative despawn** — per-frame `TentativeDestroys` ring buffer for rollback-safe entity death (Phase 1–3 done; Phase 0 tracking not yet implemented)

### Simulation

- [ ] **ConstraintEntity system** — constraint pool, `ConstraintType` enum, render-thread rigid attachment pass, physics root determination
- [ ] **Space partition cell registry** — cell world origins, cell assignment at spawn, cross-cell reparenting
- [ ] **Standalone Soul synthesis** — create a local Soul in `FlowManager` for offline/solo play (currently Soul creation is gated behind network handshake)

### Presentation

- [ ] **Presentation Reconciler** — Anti-Events (RapidFadeOut, SoftCancel, RapidDecay) for rollback-driven effect correction. `AudioManager::FadeOut` is Anti-Event-compatible; diff logic not implemented.

### Rendering

- [ ] **Frustum culling** — SIMD 6-plane test + GPU-side predicate enhancement
- [ ] **State-sorted rendering** — 64-bit sort keys, GPU radix sort after scatter
