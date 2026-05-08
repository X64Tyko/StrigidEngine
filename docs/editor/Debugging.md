# Debugging Suite

> [← Editor Overview](Overview.md) | [Home](../Home.md)

---

## Overview

The debugging suite is a blueprint for tooling that bridges the gap between the engine's data-oriented internals and a developer-friendly inspection experience. Tools are implemented a-la-carte as project needs dictate.

**Implemented:** Debugger panel (network stats plots, logic frame time history), Tracy integration (3 profiling levels), ImGuizmo gizmo, GPU picking.

**Designed, not yet implemented:** All tools below unless noted otherwise.

---

## Part 1 — Memory & Temporal Visualization

### Slab Heatmap

A GPU-generated texture visualising the entire Volatile and Temporal SoA slab with zero CPU overhead. Rendered via a Slang compute shader (`slab_visualizer.slang`) reading from the active `InstanceBuffer`.

- **Field-major layout:** X-axis = entities; Y-axis = fields stacked with history frames T through T-N vertically per field. Visualises temporal "streaks" from entity movement.
- **Macro view (5px/64 entities):** Uses `popcount()` on the 64-bit `TemporalFlags` bitplane. Black = macro-gaps, dim green = fragmented chunks, bright green = packed memory. Instantly reveals partition boundaries (RENDER, DUAL, PHYS, LOGIC).
- **Micro view (2x2 px/cell):** Exact data representation. Hovering triggers CPU-side reverse lookups.
- **Chunk overlay:** GPU draws borders and fullness gradients for AoS chunk allocations over the SoA heatmap.

### DoD-to-OOP Side-Table

Entities don't store `Owner*` pointers in the slab (would pollute every SoA array column). The side-table maintains a `FlatMap<EntityCacheIndex, ConstructMetadata>` in `ConstructRegistry`.

When hovering over an anonymous pixel in the heatmap, the editor queries the side-table to display the owning Construct's RTTI name and View name (e.g., *"BarrelAssembly owned by Turret_3"*).

### Deep Temporal Scrubbing

When `TNX_ENABLE_EDITOR` is defined, `EngineConfig::TemporalFrameCount` can override to a deep buffer (e.g., 256 frames / 0.5s at 512Hz) to allow human-readable timeline debugging.

Scrubbing the paused timeline performs a blocking CPU-to-GPU transfer of the requested `HistorySlab` frame, updating the heatmap.

### Play-From-Here Resimulation

1. Scrub to T-4 and edit a value in the Details panel
2. Press **Propagate Resim**
3. Engine restores the Jolt Physics snapshot for T-4 and fast-forwards the Brain thread 4 ticks
4. `PresentationReconciler` suppresses all `TemporalEvents` during fast-forward to prevent audiovisual spam

---

## Part 2 — Spatial & Physics Debugging

### Debug Draw API

A thread-safe, triple-buffered command ring buffer for drawing transient 3D shapes from logic code.

Logic code (512Hz) pushes commands with a `Duration` parameter. The Encoder thread consumes the buffer, converting `Fixed32` cell-relative coordinates to `float32` camera-relative coordinates, and issues a batched graphics draw call.

### Post-Process Non-Visibility Overlays

For debugging spatial systems that aren't strictly visual (Physically Based Audio volumes, AI perception, trigger volumes) without fighting the depth buffer.

The compute culling pass flags the instance payload with a debug flag. A post-process pass reads the `InstanceBuffer` and scene depth, projecting the bounds of flagged entities and drawing translucent debug shaders directly on top of the rendered scene (x-ray behavior).

### Jolt Sleep/Awake Heatmap

Colors physics entities based on their Jolt activation state (Awake = white, Sleeping = dark gray). Essential for diagnosing missed explicit awakenings at the `JoltJobSystemAdapter` boundary.

---

## Part 3 — Performance & Profiling

### Tracy Integration *(implemented)*

Three profiling levels via `TRACY_PROFILE_LEVEL`:

| Level | Zones | Overhead |
|---|---|---|
| 1 | `TNX_ZONE_COARSE()` — frame/system level | ~1–2% |
| 2 | + `TNX_ZONE_MEDIUM()` — per-chunk | ~5–10% |
| 3 | + `TNX_ZONE_FINE()` — per-entity | ~50%+ |

Tracks the Trinyx Trinity (Sentinel, Brain, Encoder threads) and lock contention on job system MPMC ring buffers.

### Job Graph Visualizer

A node-based dependency graph representing the actual scheduling logic of the Brain thread. Visualises the critical path of PrePhysics, Physics, PostPhysics, and ScalarUpdate batches. Helps identify if a Construct's tick registration is forming a pipeline bubble or forcing a thread sync.

### Debugger Panel *(implemented)*

The current Debugger panel in the editor provides:

**Network tab:** Active channel count, dirty entity count, StateCorrection bytes/frame, EntityDelta bytes/frame, delta vs full-state ratio. 128-sample ring buffer plots.

**Profiler tab:** Fixed update budget (1000/Hz ms) with color-coded status (green / yellow at 80% / red at over). Logic thread and fixed-step frame time history plots.

---

## Part 4 — Network & Architecture Tooling

### Network Condition Simulator

Built into `NetConnectionManager` to inject artificial latency, packet loss, and jitter. Allows local stress-testing of `ReplicationSystem` and rollback artifacting without a real network.

### Net Packet & Bandwidth Profiler

A scrolling timeline showing outgoing/incoming packet sizes. Clicking a packet reveals exactly how many bytes were spent on `EntitySpawns` vs `StateCorrections`, and which specific components consume the most bandwidth. Crucial for tuning delta compression.

### Registry Health & ABA Debugger

Tracks `AllocatedEntityCount` vs `TotalEntityCount`, alongside live visualisations of the `PendingDestructions`, `PendingLocalRecycles`, and `PendingNetRecycles` queues. Manual buttons to force defragmentation and observe slab compaction in real-time.

### Deterministic Input Replay

Records user inputs and RNG seeds to a file. Load the file to play back a sequence frame-for-frame, guaranteeing 100% reproduction of physics or logic bugs. Leverages the engine's fixed-point math and strict fixed timestep — if inputs are identical, the simulation is identical.

### Flow Graph Visualizer

A real-time tree view of `FlowManager`: active `FlowState` stack, `GameMode`, and all instantiated Constructs grouped by lifetime scope (Persistent / Session / World / Level). Catches memory leaks across travel transitions.

---

## Part 5 — Render & GPU Debugging

### VizBuffer Inspectors

A dropdown in the PIE viewport that modifies the composite shader to output intermediate attachments (Base Color, World Normals, Depth, Motion Vectors) instead of the final lit frame.

### Shader Complexity & Overdraw Heatmap

Swaps standard fragment shaders with an additive blend pipeline. Pixels accumulate color from Green → Yellow → Red → White, highlighting expensive transparent geometry and fill-rate bottlenecks.

### GPU-Driven Culling Visualizer

Freeze the frustum and fly the camera outside of it. The compute pipeline flags culled entities in the `InstanceBuffer` payload instead of dropping them, allowing a post-process pass to render them in a translucent "error" state. Debugs the predicate → prefix_sum → scatter pipeline.

### 1-Click RenderDoc Capture

Dynamically loads the RenderDoc In-Application API, captures exactly one Vulkan frame, and auto-launches the RenderDoc UI for deep shader and BDA inspection.

---

## Part 6 — Core Systems & Asset Tooling

### Asset Dependency Graph

A node-based viewer showing the exact reference tree (e.g., `PlayerConstruct → CharacterMesh → HeroMaterial → Albedo_8K.png`). Spots unreferenced assets or accidental hard-references that break async streaming.

### VRAM Fragmentation Analyzer

Leverages VMA via `vmaBuildStatsString`. Parses the internal JSON to display a block-by-block visual map of physical VRAM, highlighting fragmented free space.

### Global CVar Console

A Quake-style drop-down console overlay for runtime tweaking of engine and gameplay variables (`r.ShadowQuality`, `phys.Gravity`, `net.SimulatedPing`) without custom editor UI.

---

## Part 7 — Trinyx-Specific

### Fixed-Point Precision Inspector

Visualises the quantisation error introduced when converting `Fixed32` coordinates to render `float32`. Heat gradient per entity based on precision lost — diagnoses visual-only jitter that doesn't manifest in the simulation.

### Anti-Event Stress Tester

An editor toggle to force artificial presentation mispredictions. The engine delays presentation evaluation, forcing `PresentationReconciler` to generate Anti-Events (rapid fades/decays). Tests audio and VFX graceful degradation without a live networked session.
