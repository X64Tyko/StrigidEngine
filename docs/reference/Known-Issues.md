# Known Issues

> [← Design Decisions](Design-Decisions.md) | [Schema Errors →](Schema-Error-Reference.md) | [Home](../Home.md)

---

## Networking

### 1. `Soul::Channel` is a dead stored member

`Soul` holds a persistent `NetChannel` member, but `DispatchServerRPC` / `DispatchClientRPC` unconditionally overwrite it from `RPCContext` on every call. The stored value is never read between dispatches.

**Correct fix:** Thread `NetChannel` through the dispatch call and have RPC thunks capture it from dispatch context. Eliminates `GetNetChannel()` and the stored member entirely.

---

### 2. `NetChannel` CI pointer can dangle in PIE

`NetChannel` stores a raw `ConnectionInfo*`. If `NetConnectionManager::Connections` (a `std::vector`) reallocates on a new connection, all outstanding `NetChannel` objects hold dangling pointers.

**Correct fix:** Stable storage for `ConnectionInfo` (e.g. index-based or pointer-stable pool), or `NetChannel` stores an index + generation instead of a raw pointer.

---

### 3. `FindConnectionByOwnerID` was ambiguous in PIE

In PIE, two `ConnectionInfo` entries share the same `OwnerID` — one `bServerSide=true`, one `bClientInitiated=true`. The function now accepts a `requireServerSide` flag to disambiguate.

**Audit required:** All call sites must pass the correct flag. Any new call site added without the flag will silently mutate the wrong leg.

---

### 4. `SendPong` builds its header manually, bypassing `MakeHeader`

Every other send path goes through `NetChannel::MakeHeader` which stamps `LastAckedClientFrame`. `SendPong` was constructing the header by hand and omitting this field — fixed — but the pattern is a regression risk.

**Correct fix:** Make `SendPong` call `MakeHeader` like all other sends.

---

### 5. Heartbeat Ping reuses the clock-sync message type

`TickReplication` sends a `NetMessageType::Ping` to propagate ACKs during quiet frames. This conflates ACK heartbeats with RTT measurement pings.

**Correct fix:** A dedicated `NetMessageType::Ack` (header-only, no clock-sync semantics) would be cleaner.

---

### 6. Souls do not exist in standalone mode *(blocking for local play)*

The Soul creation path is gated behind the networking handshake. In standalone, no Souls are created — gameplay code that queries Souls finds nothing.

**Correct design:** Always create Souls — synthesise a local Soul per player during standalone World init, `OwnerID` from a local counter, no net session. This unifies the code path and enables local multiplayer without a divergent flow graph. `Soul::GetNetChannel()` must be null-safe when no CI exists.

**Status:** Not yet implemented. See [Status & Roadmap](Status-And-Roadmap.md).

---

### 7. `PlayerInputLog::Store()` high-water guard is belt-and-suspenders

`HighWaterFirstFrame` was stuck at 1 for the entire session because ACK trimming was broken. The `Store()` loop is now clamped to `LastConsumedFrame - Depth + 1` regardless of ACK state, bounding it to `O(ring_depth)`. Once ACK trimming is verified stable in production, the belt-and-suspenders clamp can be removed if desired.

---

## ECS / Memory

### 8. ~~`TemporalFrameStride` duplicated on Archetype~~ ✅ Fixed (2026-04-21)

`BuildFieldArrayTable` moved out-of-line to `Archetype.cpp`; queries `cache->GetFrameStride()` directly.

---

### 9. ~~`GetTemporalFieldWritePtr` lived on Archetype~~ ✅ Fixed (2026-04-21)

`GetWriteFramePtr(void*)` and `GetReadFramePtr(void*)` added to `ComponentCacheBase`; all call sites updated.

---

### 10. Reflection system relies on static initialisation order *(fragile)*

`TNX_REGISTER_COMPONENT`, `TNX_TEMPORAL_FIELDS`, `TNX_REGISTER_SCHEMA` etc. are driven by static constructors. Cross-TU ordering is undefined in C++. Currently works because all registrations resolve before `TrinyxEngine::Initialize()`, but this is fragile.

**Correct fix:** A dedicated precompile step (like UBT) or an explicit registration call per module. See [Determinism](../math-and-determinism/Determinism.md) for the planned `MetaRegistry` design.

---

## Rendering

### 11. Default identity quaternion not enforced on `CTransform`

A zero quaternion (`RotQW=0`) is mathematically invalid and produces degenerate rendering. Spawned entities that don't explicitly set rotation will render incorrectly.

**Correct fix:** Initialise `RotQW=1.0f` in `CTransform`'s default state, or assert in the scatter shader.

---

## Planned Fixes (Next Phase)

- [ ] **Replication reliability** — client entities appear then vanish; root cause in the `ClientRepState` / activation pipeline
- [ ] **Animation** — skeletal animation: pose sampling, bone hierarchy, GPU skinning
- [ ] **Phase 0 tentative despawn** — per-frame `TentativeDestroys` ring buffer for rollback-safe entity death
- [ ] **Frustum culling** — SIMD 6-plane test, GPU-side predicate enhancement
- [ ] **State-sorted rendering** — 64-bit sort keys, GPU radix sort after scatter
- [ ] **Standalone Soul synthesis** — synthesise a local Soul per player in `FlowManager` for offline play (Known Issue #6)
