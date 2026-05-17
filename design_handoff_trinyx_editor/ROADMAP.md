# ROADMAP — Phased Delivery

Don't try to ship everything at once. Land the visual change first so
the team can feel the new direction, then build novel features one
workspace at a time.

## Phase 0 — Foundation (½ day)

The visual change. Should be invisible to the engine.

- [ ] Vendor FreeType into ImGui (compile flag `IMGUI_ENABLE_FREETYPE`)
- [ ] Vendor `Space Grotesk`, `Manrope`, `JetBrains Mono` TTFs into `assets/fonts/`
- [ ] Implement `TnxStyle::Apply()` and `TnxStyle::LoadFonts()` (from `STYLE.md`)
- [ ] Call them at editor startup, before the first frame
- [ ] Replace ImGui::PushStyleColor calls scattered across panels — they're now defaults

**Verification:** color sampler on the running editor matches `STYLE.md`
hex values ±2/255. Three fonts visible (display in window titles, UI
default, mono in number labels).

## Phase 1 — Atoms (1 day)

Build the custom widgets in `WIDGETS.md`. Test in a throwaway scratch panel.

- [ ] `TnxWidgets::TierBadge`
- [ ] `TnxWidgets::Chip` (+ all styles)
- [ ] `TnxWidgets::Kbd`
- [ ] `TnxWidgets::PanelHeader`
- [ ] `TnxWidgets::FieldFloat`, `FieldVec3`, `FieldBool`, `FieldAssetRef`
- [ ] `TnxWidgets::Button*` variants
- [ ] `TnxWidgets::FrameBudgetBar`

**Verification:** scratch panel shows one of each widget; matches mock.

## Phase 2 — Restyle existing panels (1 day)

Don't restructure logic, just visuals. Each panel:

- Open with `TnxWidgets::PanelHeader`
- Use the atom widgets where you find raw ImGui equivalents
- Apply the new badge/queue model where applicable

Panels to touch (existing):

- [ ] **World Outliner** — see Phase 3 below; this is the big restructure
- [ ] **Details / Inspector** — see Phase 3
- [ ] **Content Browser** — header swap, button restyle, grid layout polish
- [ ] **Engine Stats** — header swap, use `FrameBudgetBar` for the three threads
- [ ] **Log** — header swap, color level chips with `Chip(...)`
- [ ] **Node Script** — header swap only; deeper work in Phase 5
- [ ] **Component Generator** — header swap, add "Replicated" checkbox
- [ ] **Debugger** — header swap, sparklines via `PlotLines`

**Verification:** every panel uses the new typography and chrome.

## Phase 3 — World Outliner + Inspector restructure (1 day)

The model corrections from `PANELS.md`. Two roots in the outliner;
three-layer badges in the inspector.

- [ ] Outliner: split into `CONSTRUCTS` + `ARCHETYPES` rooted sections
- [ ] Hook `ConstructRegistry::Roots()` for the constructs side
- [ ] Drill-down: Archetype → Chunks → Entity-leaves with cache index
- [ ] Inspector entity header: queue tag, EView<T> chip, idx, arch + chunk
- [ ] Inspector component header: tier badge + net chip on the SAME line
- [ ] Inspector field rows: clean — no metadata badges
- [ ] Wire right-click menus per `PANELS.md`

**Verification:** select Player.Body → see `[queue: Physics]` on entity
header, `[TEMPORAL] [net]` on CTransform, clean field rows below.

## Phase 4 — Workspace Switcher (1 day)

The top-bar pills and per-workspace dock layouts from `NOVEL.md §1`.

- [ ] Five workspace pills in top bar
- [ ] Dock layout per workspace via `DockBuilder`
- [ ] Persist current workspace + per-workspace layout to ini
- [ ] Move PIE play/pause/step buttons to top bar (alongside pills)
- [ ] Add the always-visible Frame Budget strip (anchored bottom-right, doesn't switch with workspace)

**Verification:** clicking Profile rearranges every panel; reload editor
and Profile is still the active workspace with the user's dock state.

## Phase 5 — Command Palette (1 day)

The ⌘K overlay from `WIDGETS.md`. Start with 20 commands.

- [ ] Modal popup with dim full-screen background
- [ ] Fuzzy match (vendor forrestthewoods/fts_fuzzy_match)
- [ ] ↑↓ nav, ↵ execute, ESC dismiss, ⌘K open
- [ ] Register starter commands:
  - [ ] Switch workspace × 5
  - [ ] Save / Open / New scene
  - [ ] Spawn pyramid (15, 25 layers)
  - [ ] Spawn 100k ambient cubes
  - [ ] Start PIE / Stop PIE
  - [ ] Switch to PIE loopback (2 / 3 / 4 owners)
  - [ ] Force defragment
  - [ ] Open Tracy
  - [ ] Toggle Wireframe / Partitions / Thread overlays

**Verification:** ⌘K → "spawn pyr" → ↵ spawns the pyramid; editor stays
visible behind a dim overlay.

## v1 — Ship after Phase 5

That's the corrected visual editor. Five days, single dev.

---

## Phase 6+ — Novel features

Order them by what unblocks the next milestone for the engine.

### Phase 6 — Slab Heatmap (3–5 days)

Highest leverage: it makes the engine's data-oriented architecture
*visible* in a way no other tool can. Aligns with `Debugging.md §Part 1`.

- [ ] Write `slab_visualizer.slang`
- [ ] Bind compute output to a Vulkan image
- [ ] Expose `ImTextureID` to ImGui via `ImGui_ImplVulkan_AddTexture`
- [ ] Hover → cell → DoD-to-OOP side-table → tooltip
- [ ] Macro / Micro mode toggle
- [ ] Chunk overlay on top

### Phase 7 — Multi-window PIE + Network Condition Simulator (2–3 days)

- [ ] Enable `ImGuiConfigFlags_ViewportsEnable`
- [ ] One `WorldViewport` panel per PIE world; auto-tile on PIE start
- [ ] NetConnectionManager injection points (latency, loss, jitter, dup, reorder)
- [ ] Condition Simulator panel bound to those points
- [ ] Network workspace dock layout

### Phase 8 — Node Script editor upgrade (3–5 days)

The Logic workspace's centerpiece.

- [ ] Vendor `imgui-node-editor` (thedmd)
- [ ] Migrate your 25 node types to the editor's node API
- [ ] Wire your codegen walker to a live `.gen.cpp` preview pane
- [ ] Inline determinism linter callout

### Phase 9 — Job Graph + Rollback Scrubber (2 days each)

- [ ] Job Graph custom widget over `ImDrawList` per `NOVEL.md §5`
- [ ] Rollback Scrubber per `NOVEL.md §6`
- [ ] Enable deep `TemporalFrameCount = 256` when editor is on

## Done

The full editor when all phases ship. Each is independently shippable;
don't gate Phase 7 on Phase 6 etc.

## What's NOT on this list (intentional)

- VR/AR editor view
- Touch / iPad support
- Cloud sync of editor layouts
- Multi-user collaborative editing
- Animation timeline editor
- Particle FX editor

These are real things, just not v1. Ask if they become priorities.

## When you're stuck

If a feature feels harder than its line in this roadmap suggests, stop
and ask. The mocks are guidance, not law — when ImGui's idioms point a
different direction than the HTML mocks, ImGui usually wins.
