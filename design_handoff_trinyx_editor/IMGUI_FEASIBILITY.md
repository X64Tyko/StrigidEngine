# ImGui Feasibility

Honest assessment of what's achievable in Dear ImGui (docking branch) plus
a few standard addons. Effort estimates assume a developer already familiar
with ImGui internals.

## Required dependencies

Add these to your CMake/vcpkg if not already present.

| Library | Version | Why |
|---|---|---|
| `imgui` (docking branch) | `>= 1.91` | Already vendored. Multi-viewport for PIE. |
| `imgui_freetype` | bundled with imgui | Crisp anti-aliased fonts for Manrope / Space Grotesk. Compile with `IMGUI_ENABLE_FREETYPE`. |
| `imgui-node-editor` (thedmd) | `>= 0.9.3` | Node Script panel. Production-grade — Unreal-style graph editor. |
| `ImPlot` | `>= 0.16` | Sparklines, frame budget history, job-graph timelines, network packet plots. |
| `ImGuizmo` | already vendored | Translate/Rotate/Scale gizmo. Reuse what's there. |
| `IconFontCppHeaders` + `Material Design Icons` (or your own SVG-to-font atlas) | n/a | Icons. The design uses 1.5px stroke icons; a single icon font is fine for v1. |

**Fonts to bundle** (TTF, MIT/OFL):

- Space Grotesk (400, 500, 600, 700) — display + wordmark
- Manrope (400, 500, 600, 700) — UI
- JetBrains Mono (400, 500, 600) — code / values

Load each weight as a separate `ImFont*` via `io.Fonts->AddFontFromFileTTF`,
or merge weights via FreeType into one font with named variants. See
`STYLE.md` for the loader snippet.

## Capability matrix

### ✅ Easy — out of the box

| Feature | Approach |
|---|---|
| Royal-purple/yellow dark theme | `ImGuiStyle` + `Colors[]`. Full code in `STYLE.md`. |
| Workspace pills (top bar) | `ImGui::PushStyleColor` + `Selectable` per pill, with `dock_id` swap on click. |
| World Outliner with two roots | `ImGui::TreeNode` recursive — Constructs root + Archetypes root. |
| Inspector field rows | `ImGui::Columns(2)` or `BeginTable` with right-aligned label + right column input. |
| Tier badges (pill chips) | Custom widget via `ImDrawList::AddRectFilled` + colored text. ~20 LoC. See `WIDGETS.md`. |
| Frame budget bars | `ImGui::ProgressBar` with custom color + overlay text. Or custom DrawList for the tick mark at the 100% budget. |
| Sparklines | `ImGui::PlotLines` (built-in!) or `ImPlot::PlotLine`. Built-in is enough for v1. |
| Content Browser asset thumbnails | `ImGui::ImageButton` over a render-target. Existing code does this. |
| Hierarchy filter input | `ImGui::InputText` + `ImGui::TextFilter`. Trivial. |
| Status bar (bottom) | Single `BeginChild` with `Text` items. |
| Multi-window PIE viewports | Already supported by docking branch's `ImGuiConfigFlags_ViewportsEnable`. Each `WorldViewport` is just a `Begin` window. |

### 🟡 Medium — needs a custom widget or pattern

| Feature | Approach |
|---|---|
| Command palette overlay | `BeginPopupModal` with `ImGuiWindowFlags_NoMove\|NoResize` + dim full-screen `ImGui::GetForegroundDrawList()->AddRectFilled` behind it. Fuzzy match against a registered `vector<Command>`. ~150 LoC. |
| Selection glow on selected entity | `ImDrawList::AddRect` with 2 passes at decreasing alpha. Or render a stencil pass in your scene shader (the engine path) — likely better. |
| Hover transition (color lerps) | Manual: store hover state per widget, lerp toward target using `io.DeltaTime`. ImGui has no native transitions. |
| Workspace dock layouts | `ImGui::DockBuilderSetNodeSize` / `DockBuilderDockWindow`. Save/load layouts to disk keyed by workspace name. |
| Kbd glyph (`⌘K`) | A `Text` with the right font + a 1px bordered box, or a tiny custom widget. ~10 LoC. |
| Slab Heatmap | Compute-shader output → Vulkan image → `ImTextureID` → `ImGui::Image()`. Hover position → cell coords → side-table lookup → tooltip. Already designed in `Debugging.md`. |
| Node graph (visual script) | `imgui-node-editor` is purpose-built; we have to lay out the palette + code preview side-by-side ourselves. ~1–2 days of work to wire your existing 25 nodes into the graph editor's `ed::BeginNode/Pin/Link` API. |
| Network condition simulator | Sliders + buttons. Easy ImGui. Real work is on the engine side (`NetConnectionManager` injection points). |
| Job graph timeline | Custom widget over `ImDrawList`. Lane rectangles + critical-path overlay path. ~300 LoC. ImPlot can help but a custom widget gives finer control. |
| Rollback scrubber | `ImGui::SliderInt` for the playhead + custom DrawList for the per-frame budget histogram. Event markers are `AddTriangle` glyphs along the track. |

### 🔴 Hard — fake or skip

| Feature | What to do |
|---|---|
| Backdrop blur behind command palette | Skip. Use a flat dark overlay at 55% alpha — looks intentional. |
| Soft shadows on floating panels | Approximate with 3–4 passes of `AddRect` at decreasing alpha + increasing extent. Costs more than it's worth — flat outlines look fine. |
| Smooth font weight transitions | Don't try — just load discrete weights. |
| Per-pixel anti-aliased rounded borders on HiDPI | Set `ImGui::GetStyle().AntiAliasedLinesUseTex = true` + appropriate FrameRounding. Acceptable. |
| Subtle radial vignette on viewport | Done in the engine's render shader, not in ImGui. Flag your render path with a `Debug.ViewportVignette` CVar. |
| Animated playhead "pulse" | Sine-wave the alpha of an outline; `sin(ImGui::GetTime() * 6.f)`. |

## Effort estimate

Rough estimate to land v1 of the redesign (style + workspace switcher +
restyled existing panels):

- Style + fonts + tokens — **0.5 day**
- Custom widgets (tier badge, chip, kbd, frame budget, panel chrome) — **1 day**
- Workspace switcher + dock layout persistence — **1 day**
- World Outliner restructure (Constructs + Archetypes roots) — **1 day**
- Inspector rebadge (entity/component/field) — **0.5 day**
- Command palette MVP (fuzzy + 20 commands) — **1 day**

**Total v1: ~5 days** for a single developer, assuming you don't rewrite
existing panel internals — just restyle.

Novel features (post v1):

- Slab Heatmap (needs the Slang compute shader from `Debugging.md`) — **3–5 days**
- Network Condition Simulator (engine injection + UI) — **2–3 days**
- Node Script graph editor — **3–5 days**
- Job Graph timeline — **2 days**
- Multi-window PIE wiring — depends on existing `WorldViewport` work
