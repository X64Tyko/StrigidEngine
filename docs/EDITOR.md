# TrinyxEngine Editor

> **Navigation:** [← Back to README](../README.md) | [Architecture →](ARCHITECTURE.md) | [Status →](STATUS.md)

---

## Overview

The editor is an optional layer enabled by `-DTNX_ENABLE_EDITOR=ON`. It runs on the Encoder (render) thread and shares the engine's live ECS state — all inspection and editing operates on real slab data, not a copy. It is intentionally scoped to scene editing, not a general-purpose game editor.

**Panels (8):** World Outliner, Details, Content Browser, Engine Stats, Log, Node Script, Component Generator, Debugger

---

## Core Architecture

### EditorContext

Main UI manager. Creates and owns all panels. Called once per render frame between `ImGui::NewFrame()` and `ImGui::Render()`.

Responsibilities:
- Panel registration via `AddPanel<T>(...)` template
- Scene load/save (`LoadScene`, `SaveToFile`)
- Entity spawn/delete via the engine spawn handshake
- Prefab instantiation (`SpawnPrefab`)
- Undo/redo stack (max 50 commands, merge-capable)
- Play-In-Editor lifecycle (`StartPIE`, `StopPIE`)
- Scene snapshot/restore for play/stop cycles
- Gizmo rendering (`DrawGizmo` — ImGuizmo)
- File drag-drop handling

### EditorState

Transient per-frame state shared across all panels. Lives on the render thread — no synchronization required.

| Field | Description |
|---|---|
| `SelectionType` | `None` / `Archetype` / `Entity` |
| Selected ClassID, Archetype*, Chunk*, LocalIndex, CacheIndex | Triple-index entity selection |
| `GizmoOp` | Translate / Rotate / Scale |
| `bGizmoWorldMode` | World vs local space |
| `GizmoSnapTranslate/Rotate/Scale` | Snap increments (default 0.5 / 15° / 0.25) |
| `CurrentScenePath` / `CurrentSceneName` | Loaded .tnxscene |
| `bSceneDirty` | Unsaved-changes flag (shown as `*` in menu bar) |
| `SceneDefaultState` / `SceneDefaultMode` | FlowState and GameMode names used for PIE |
| Engine references | Registry, EngineConfig, LogicThread, TrinyxEngine, MeshManager, AssetDatabase |
| `ReplicationSystem*` | Non-null only during PIE |

### WorldViewport

Per-world GPU resource bundle. Supports multi-viewport rendering (editor world + PIE server + up to 4 PIE clients).

Each viewport owns:
- **ColorTarget** (RGBA8) — rendered scene
- **DepthTarget** (D32_SFLOAT)
- **PickTarget** (R32_UINT) — entity cache index per pixel (`TNX_GPU_PICKING` only)
- **ImGuiTexture** — descriptor set for ImGui compositing
- **GpuData[MaxFramesInFlight]** — per-frame BDA push constants
- **FieldSlabs[N]** — per-world entity data slabs with dirty tracking

---

## Panels

### World Outliner

Hierarchical entity browser.

- Archetypes as collapsible tree nodes showing entity count
- Chunks within each archetype; individual entities as leaves (cache index)
- Click archetype → selects archetype mode; click entity → selects entity mode
- Right-click entity → context menu with **Delete**
- **Delete** key deletes selected entity when panel is focused
- Accepts prefab drag-drops from Content Browser (spawns into scene via handshake)

### Details

Component field inspector.

- **Archetype mode:** shows DebugName, ClassID, entity/chunk counts, component list
- **Entity mode:** shows editable field values (only when simulation is paused)
- Per-field editors: Float32, Fixed32, Float64, Int32, Uint32, asset references
- Asset reference fields show as combo dropdowns (MeshManager slots)
- Accepts mesh drag-drops from Content Browser onto asset fields
- All edits mark entity dirty, push an undo command, and set the scene dirty flag
- Editing is disabled while the simulation is running

### Content Browser

Asset manager for the project content directory.

- Table view: Path, Type, MeshID, UUID columns
- Type filter combo (All, Data, Static Mesh, …)
- **Import Mesh…** button → import dialog
- Prefab entries: drag to World Outliner to instantiate (payload = absolute path)
- Mesh entries: drag to Details asset field (payload = uint32_t slot index)
- Double-click level files to load the scene

### Engine Stats

Real-time telemetry read-out.

- Render FPS/ms (ImGui frame rate)
- Logic FPS/ms and fixed-step FPS/ms (from LogicThreadBase)
- Current logic frame number
- Total entity and chunk counts
- Full EngineConfig dump: TargetFPS, FixedUpdateHz, PhysicsUpdateInterval, NetworkUpdateHz, InputPollHz, MAX_RENDERABLE_ENTITIES, MAX_JOLT_BODIES, MAX_CACHED_ENTITIES, TemporalFrameCount, JobCacheSize

### Log

Real-time log viewer.

- Per-level filter checkboxes: Trace, Debug, Info, Warn, Error, Fatal
- Color-coded output: Trace (gray), Debug (cyan), Info (green), Warning (yellow), Error (red), Fatal (magenta)
- Auto-scroll toggle
- Horizontal scroll for long lines
- Reads from `Logger::Get().GetLogRing()` ring buffer

### Node Script

Visual blueprint-style scripting with C++ code generation.

**25 node types across 5 categories:**

| Category | Nodes |
|---|---|
| Events (execution roots) | OnPrePhysics, OnPostPhysics, OnUpdate, OnSpawn, OnDestroy |
| Flow | Sequence (2-way), Branch (conditional exec split) |
| Properties | GetProperty, SetProperty |
| Math | Add, Subtract, Multiply, Divide, Clamp, Lerp |
| Vectors | Make Vec3, Break Vec3, Length, Normalize, Scale |
| Entity | GetPosition, SetPosition, GetVelocity, SetVelocity, ApplyImpulse |

**Canvas interaction:**
- Pan: middle-mouse or spacebar-drag
- Node drag-to-move, multi-select
- Link creation: drag output pin → input pin
- Right-click: context menu with node palette (filterable)

**Code generation:**
- Topological walk from event roots
- Inlines data expressions per node; variable naming `_n<nodeID>_p<pinID>`
- Validates determinism constraints: Branch nodes rejected in pre/post-physics paths (error shown before export)
- Output: target entity name input, output path selector, live code preview, export to file

### Component Generator

Form-based ECS component header generator.

- Component name field
- Storage tier: Temporal / Volatile / Cold
- SystemGroup (Render / Physics / Logic / Dual) — for Temporal and Volatile only
- Dynamic field list: per-field Name + Type (float, int32_t, uint32_t, bool, Fixed32)
- Live code preview pane
- Export to file with status feedback
- Emits the correct macro: `TNX_TEMPORAL_FIELDS()`, `TNX_VOLATILE_FIELDS()`, or `TNX_REGISTER_FIELDS()`

### Debugger

PIE and networking diagnostics.

**Network tab:**
- Active channel count, dirty entity count
- StateCorrection bytes/frame and EntityDelta bytes/frame
- Delta vs full-state ratio
- 128-sample ring buffer plots for correction bytes, delta bytes, dirty entity count

**Profiler tab:**
- Fixed update budget (1000/Hz ms) with color-coded status (green / yellow at 80% / red at over)
- Logic thread and fixed-step frame time history plots
- Freezes on last-known values when PIE is inactive

---

## Asset Database

Editor-only UUID↔path authority. Runtime `AssetRegistry` is populated from it at startup.

**Sidecar files (.tnxid):**  
Created once per asset at import. Binary: 8 bytes UUID + 8 bytes ContentHash + 4 bytes SchemaVersion + 1 byte Flags. Never manually edited.

`SidecarFlags`: `Dirty` (content hash mismatch since last import), `SchemaOutdated` (importer version changed).

**Reconciliation** (runs at `Initialize()` and on-demand):
- Scans content directory recursively
- Creates .tnxid sidecars for new assets
- Marks sidecar Dirty on hash mismatch
- Detects moved assets (UUID stable, path updated)

**Persistence:** `.tnxdb` file in content root.

---

## Undo / Redo

Stack-based, max 50 commands. Adjacent compatible commands merge into one undo entry.

| Command | Merges? | Notes |
|---|---|---|
| `EntityTransformCommand` | Yes — same entity | Captures full field snapshot before/after via JSON serialization |
| `ComponentFieldChangeCommand` | Yes — same field | Stores old/new bytes; type-size aware |

`EditorContext::Undo()` / `Redo()` are keyboard-accessible (Ctrl+Z / Ctrl+Y).

---

## Scene File Format (.tnxscene)

JSON with a metadata header:

```json
{
  "name": "TestArena",
  "defaultState": "GameplayState",
  "defaultMode": "ArenaMode",
  "entities": [ ... ]
}
```

Per-entity fields are serialized as named key-value pairs (Float32, Fixed32, Float64, Int32, Uint32). Prefab files are single-entity .tnxscene files — detected by `EntityBuilder::SpawnEntity` vs `SpawnScene`.

**Load:** reads JSON → calls `EntityBuilder::SpawnScene()` via spawn handshake → updates `EditorState` scene metadata.  
**Save:** serializes all registry entities to JSON via `SaveToFile()`.

---

## Play-In-Editor (PIE)

**Single-world play (local):**
- `SnapshotScene()` captures all archetype field data before play
- Simulation unpauses; input is routed to the active world
- `StopPIE()` calls `RestoreSnapshot()` to byte-restore all field data

**Networked PIE (server + N clients):**
- 1–4 client worlds configurable via the Play menu spinner
- Server may be headless (no viewport) or rendered
- Each client gets its own `WorldViewport`, FlowManager, and engine config instance
- `ReplicationSystem` is activated on the server world; pointer stored in `EditorState`
- `StopPIE()` tears down all client worlds and restores the snapshot

---

## Gizmo (ImGuizmo)

Reads PosXYZ, RotQxyzw, ScaleXYZ from entity field arrays via reflection and builds a 4×4 column-major model matrix. Decomposes the manipulated matrix back to the component fields.

- **W** — Translate
- **E** — Rotate
- **R** — Scale
- World vs local space toggle (Edit menu or `EditorState::bGizmoWorldMode`)
- Snap: 0.5 units / 15° / 0.25 scale (configurable)
- Every manipulation pushes an `EntityTransformCommand`

---

## Keyboard Shortcuts

| Key | Action |
|---|---|
| W / E / R | Gizmo: Translate / Rotate / Scale |
| Ctrl+Z | Undo |
| Ctrl+Y | Redo |
| Ctrl+O | Open Scene |
| Ctrl+S | Save Scene |
| Ctrl+Shift+S | Save As |
| Delete | Delete selected entity (World Outliner focused) |
| Escape | Stop PIE |
| Shift+F1 | Toggle mouse capture between editor and engine |

---

## Menu Bar

| Menu | Items |
|---|---|
| **File** | Open Scene, Save Scene, Save As, Import Mesh, Save as Prefab, Exit |
| **Edit** | Undo, Redo, Translate/Rotate/Scale, World Space, Snap toggles |
| **View** | Toggle visibility for each panel, Reset Layout |
| **Play** | Play (Local), Pause, Stop, Play (Server+Client), Play (Headless Server+Client), Stop PIE, Pause/Resume PIE, Clients spinner (1–4), Default State/Mode |
| **Debug** | Show Demo Window, Show ImGui Metrics |

Scene name and dirty indicator (`*`) are displayed right-aligned in the menu bar.
