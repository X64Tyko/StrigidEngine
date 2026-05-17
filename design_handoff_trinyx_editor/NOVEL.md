# NOVEL — Workspace Switcher + Big-Bet Features

This is the post-v1 work. Build the workspace switcher first (it's
cheap and reorganizes everything); the others can ship in any order.

## 1. Workspace Switcher

Five named workspaces along the top bar. Each owns its own ImGui dock
layout, persisted to disk. Switching swaps which panels are visible.

| Workspace | Anchor panels | Hidden by default |
|---|---|---|
| **Layout** | World Outliner · Viewport · Inspector · Content Browser | Node Script, Debugger Net tab |
| **Logic** | Outliner (collapsed) · Node Script (large) · Component Generator · Code preview | Viewport tile (small) |
| **Simulate** | Viewport · Rollback Scrubber (full-width bottom) · Inspector · Log | Content Browser |
| **Network** | Viewport tiles (1 auth + N owners) · Network Condition Simulator · Debugger Net tab | Content Browser, Node Script |
| **Profile** | Viewport (small) · Job Graph (large) · Frame Budget (large) · Engine Stats · Tracy launcher | World Outliner |

### Per-workspace layouts via DockBuilder

```cpp
enum class Workspace { Layout, Logic, Simulate, Network, Profile };

struct WorkspaceState {
    Workspace current = Workspace::Layout;
    std::array<ImGuiID, 5> rootDockId {};  // one per workspace
};

void EnsureWorkspaceLayouts(WorkspaceState& ws)
{
    static bool built = false;
    if (built) return;
    built = true;

    ImGuiID mainViewportDock = ImGui::GetID("TnxMainDock");

    // For each workspace, build the dock tree once.
    for (size_t i = 0; i < 5; ++i) {
        ImGuiID id = ImGui::DockBuilderAddNode(ImGui::GetID(WorkspaceName((Workspace)i)),
                                               ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(id, ImGui::GetMainViewport()->Size);

        switch ((Workspace)i) {
            case Workspace::Layout: {
                ImGuiID left, right, bot, center;
                ImGui::DockBuilderSplitNode(id, ImGuiDir_Left, 0.18f, &left, &center);
                ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.22f, &right, &center);
                ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.30f, &bot, &center);
                ImGui::DockBuilderDockWindow("World Outliner", left);
                ImGui::DockBuilderDockWindow("Scene",          center);
                ImGui::DockBuilderDockWindow("Inspector",      right);
                ImGui::DockBuilderDockWindow("Content Browser", bot);
                ImGui::DockBuilderDockWindow("Console",         bot);
            } break;
            // ... other workspaces
        }
        ImGui::DockBuilderFinish(id);
        ws.rootDockId[i] = id;
    }
}

void DrawTopBar(WorkspaceState& ws)
{
    ImGui::BeginGroup();
    // Logo + project name on the left.
    DrawLogoAndProject();
    ImGui::SameLine();
    ImGui::Spacing();

    // Workspace pills
    const char* names[] = {"Layout","Logic","Simulate","Network","Profile"};
    for (int i = 0; i < 5; ++i) {
        bool active = ws.current == (Workspace)i;
        if (active) {
            ImGui::PushStyleColor(ImGuiCol_Button, TnxStyle::Color::Purple);
            ImGui::PushStyleColor(ImGuiCol_Text,   ImVec4(1,1,1,1));
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
            ImGui::PushStyleColor(ImGuiCol_Text,   TnxStyle::Color::FgMuted);
        }
        if (ImGui::Button(names[i])) ws.current = (Workspace)i;
        ImGui::PopStyleColor(2);
        ImGui::SameLine();
    }

    // PIE controls + search + settings on the right
}
```

### Persistence

Save dock layout per workspace to `EditorSettings/Layout_<Workspace>.ini`.
Use `ImGui::SaveIniSettingsToDisk` keyed by workspace name.

## 2. Multi-window PIE + Network Condition Simulator

### Multi-window PIE

The docking branch already supports `ImGuiConfigFlags_ViewportsEnable`.
Each PIE `WorldViewport` is just an `ImGui::Begin` window with a render
target attached. They auto-pop out into OS windows when dragged.

```cpp
struct PIEViewport {
    World*       world;
    bool         isAuthority;
    int          ownerId;
    ImTextureID  renderTarget;
    NetStats     liveStats;
};

void DrawPIEViewports(std::vector<PIEViewport>& vps)
{
    for (auto& vp : vps) {
        char title[64];
        ImFormatString(title, IM_ARRAYSIZE(title), "%s###pie%d",
                       vp.isAuthority ? "Authority · World #0" : Format("Owner %c · P%d", 'A'+vp.ownerId, vp.ownerId+1),
                       vp.ownerId);

        if (vp.liveStats.recentDropCount > 0) {
            ImGui::PushStyleColor(ImGuiCol_Border, TnxStyle::Color::Warn);
        }
        if (ImGui::Begin(title)) {
            // Title bar — overlay our own banner over ImGui's title.
            DrawPIEBanner(vp);
            // Render target fills the rest.
            ImVec2 sz = ImGui::GetContentRegionAvail();
            ImGui::Image(vp.renderTarget, sz);
            // Footer with rtt / fps / drops
            DrawPIEFooter(vp);
        }
        ImGui::End();
        if (vp.liveStats.recentDropCount > 0) ImGui::PopStyleColor();
    }
}
```

### Network Condition Simulator

Sliders bound to `NetConnectionManager`'s injection points. The engine
side is the real work; the UI is trivial.

```cpp
struct NetSim {
    float latencyMs    = 0.0f;   // 0..1000
    float jitterMs     = 0.0f;   // 0..500
    float lossPct      = 0.0f;   // 0..100
    float duplicatePct = 0.0f;   // 0..10
    float reorderPct   = 0.0f;   // 0..10
    int   dropNextCorrections = 0;
};

void DrawNetSimPanel(NetSim& s)
{
    TnxWidgets::PanelHeader("\xee\x80\x88", "Condition Simulator", []{
        TnxWidgets::Chip("● active", TnxWidgets::ChipStyle::Yellow);
    });

    TnxWidgets::SliderFloat("Latency",   &s.latencyMs,    0, 1000, "ms",
                            TnxStyle::Color::PurpleHot);
    TnxWidgets::SliderFloat("Jitter ±",  &s.jitterMs,     0,  500, "ms",
                            TnxStyle::Color::Info);
    TnxWidgets::SliderFloat("Loss",      &s.lossPct,      0,  100, "%",
                            TnxStyle::Color::Warn);
    TnxWidgets::SliderFloat("Duplicate", &s.duplicatePct, 0,   10, "%",
                            TnxStyle::Color::FgMuted);
    TnxWidgets::SliderFloat("Reorder",   &s.reorderPct,   0,   10, "%",
                            TnxStyle::Color::FgMuted);

    if (TnxWidgets::Button("Preset · LAN"))       s = NetSim{};
    ImGui::SameLine();
    if (TnxWidgets::Button("Preset · 4G"))        s = NetSim{75,15,1.5f,0,0.5f};
    ImGui::SameLine();
    if (TnxWidgets::Button("Preset · Satellite")) s = NetSim{600,40,3,0,1};

    if (TnxWidgets::ButtonYellow("Drop next 3 corrections")) s.dropNextCorrections += 3;

    // Apply to engine
    NetConnectionManager::Get().SetSim(s);
}
```

## 3. Slab Heatmap

You have `slab_visualizer.slang` designed in `Debugging.md`. The ImGui
side is small:

```cpp
struct SlabHeatmap {
    VkImage      texture;       // populated by Slang compute pass
    VkSampler    sampler;
    VkImageView  view;
    ImTextureID  imguiHandle;   // from ImGui_ImplVulkan_AddTexture
    enum Mode { Macro, Micro } mode = Macro;
};

void DrawSlabHeatmap(SlabHeatmap& hm)
{
    TnxWidgets::PanelHeader("\xee\x80\x8C", "Slab Visualizer", [&]{
        if (TnxWidgets::Toggle("Macro", hm.mode == SlabHeatmap::Macro)) hm.mode = SlabHeatmap::Macro;
        ImGui::SameLine();
        if (TnxWidgets::Toggle("Micro", hm.mode == SlabHeatmap::Micro)) hm.mode = SlabHeatmap::Micro;
    });

    // Axis labels — partition bands across X
    DrawBandLabels({"RENDER","DUAL","PHYS","LOGIC"});

    // The texture
    ImVec2 sz = ImGui::GetContentRegionAvail();
    sz.y -= 24;  // reserve for X axis labels
    ImVec2 cursor = ImGui::GetCursorScreenPos();
    ImGui::Image(hm.imguiHandle, sz);

    // Hover → cell tooltip via DoD-to-OOP side-table
    if (ImGui::IsItemHovered()) {
        ImVec2 mouse = ImGui::GetMousePos();
        float u = (mouse.x - cursor.x) / sz.x;
        float v = (mouse.y - cursor.y) / sz.y;
        EntityCacheIndex idx = HeatmapPixelToEntity(u, v);
        FieldId fid          = HeatmapPixelToField(v);
        DrawHeatmapTooltip(idx, fid);  // uses ConstructRegistry::SideTable
    }

    DrawXAxisLabels();
}
```

## 4. Node Script (Visual Blueprint)

Use `imgui-node-editor` from thedmd. It handles:
- Nodes with input/output pins
- Curved wires with hit-testing
- Box-select, drag, copy/paste
- Save/load layout in its own format

You provide:
- Your 25 node types as registered factories
- Your codegen walker (already exists)
- The determinism linter (already enforces — surface results inline)

Panel layout:

```
┌──────┬──────────────────────────────────────────┬───────────────┐
│ NODES│ Graph canvas — imgui-node-editor         │ CODE PREVIEW  │
│      │                                          │ Player_Move   │
│ EVT  │                                          │ .gen.cpp      │
│ FLOW │   [OnPrePhysics]──▶[GetPos]──▶[Add]      │               │
│ MATH │                          │     │         │ syntax-hi'd   │
│ VEC  │                          ▼     ▼         │ live          │
│ ENT  │                       [Branch]─x  ⚠      │ ↩ Validate    │
│      │                                          │ ▶ Compile     │
└──────┴──────────────────────────────────────────┴───────────────┘
```

The lint callout pinned to the bottom of the canvas is just an absolutely
positioned `ImGui::SetCursorScreenPos` block:

```cpp
if (lintErrors.size() > 0) {
    ImVec2 pos(canvasCenter.x - 270, canvasBottom - 50);
    ImGui::SetCursorScreenPos(pos);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.20f, 0.05f, 0.05f, 0.95f));
    ImGui::BeginChild("lint", ImVec2(540, 50), true);
    /* icon + message + auto-fix button */
    ImGui::EndChild();
    ImGui::PopStyleColor();
}
```

## 5. Job Graph (Profile workspace)

Custom DrawList widget on a fixed canvas. Lanes for Brain / Phys Workers /
Logic Workers / Encoder. Boxes are job spans (start, end, color by tier).
The critical path is an overlay polyline at higher z.

```cpp
struct JobSpan {
    int    lane;       // 0=brain 1=phys 2=logic 3=encoder
    float  startMs;
    float  endMs;
    ImU32  color;
    const char* label;
    const char* sublabel;
    bool   critical;
};

void DrawJobGraph(const std::vector<JobSpan>& spans, float frameMs)
{
    ImVec2 size = ImGui::GetContentRegionAvail();
    ImVec2 origin = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    const float laneHeight = size.y / 4.0f;
    const float msToPx = size.x / frameMs;

    // Lane labels + separators
    for (int i = 0; i < 4; ++i) {
        float y = origin.y + i * laneHeight;
        dl->AddLine(ImVec2(origin.x, y),
                    ImVec2(origin.x + size.x, y),
                    ImGui::ColorConvertFloat4ToU32(TnxStyle::Color::BorderSoft));
        DrawLaneLabel(i, ImVec2(origin.x + 4, y + 4));
    }

    // Spans
    for (const auto& s : spans) {
        float x0 = origin.x + s.startMs * msToPx;
        float x1 = origin.x + s.endMs   * msToPx;
        float y0 = origin.y + s.lane * laneHeight + 6;
        float y1 = y0 + laneHeight - 12;
        dl->AddRectFilled(ImVec2(x0, y0), ImVec2(x1, y1), s.color, 2.0f);
        if (s.critical) {
            dl->AddRect(ImVec2(x0-1, y0-1), ImVec2(x1+1, y1+1),
                        ImGui::ColorConvertFloat4ToU32(TnxStyle::Color::Yellow), 2.0f, 0, 1.5f);
        }
        // Labels for non-tiny spans
        if (x1 - x0 > 30) {
            dl->AddText(ImVec2(x0+5, y0+3), IM_COL32_WHITE, s.label);
        }
    }

    // Critical-path overlay polyline
    DrawCriticalPath(dl, origin, spans, msToPx, laneHeight);

    // Bubble markers
    DrawBubbles(dl, origin, spans, msToPx, laneHeight);
}
```

## 6. Rollback Scrubber

The Simulate workspace's bottom dock. Three rows:

1. **Transport row** — back / play / step / forward / "f / 18,442" / chips
2. **Histogram track** — per-frame budget bars + budget threshold line + playhead + event markers
3. **Field timelines** — selectable rows of `ImGui::PlotLines` for `Player.PosX`, `Player.vY`, etc.

The trickiest piece is the **playhead drag**. Use an invisible button
sized to the histogram track, then read mouse position:

```cpp
ImGui::InvisibleButton("##scrub", trackSize);
if (ImGui::IsItemActive()) {
    float u = (ImGui::GetMousePos().x - trackOrigin.x) / trackSize.x;
    int targetFrame = ring.OldestFrame() + (int)(u * ring.SampleCount());
    PIE::SeekTo(targetFrame);
}
```

When `EnableEditor` is on, set `EngineConfig::TemporalFrameCount = 256`
(8s of history at 512Hz) so the scrubber has range — per `Debugging.md`.

## 7. Component Generator (already exists — restyle only)

The current `Component Generator` panel works. Two things to add:

1. The tier dropdown emits the right macro (`TNX_TEMPORAL_FIELDS` /
   `TNX_VOLATILE_FIELDS` / `TNX_REGISTER_FIELDS`) — already does
2. A "Replicated" checkbox that emits `TNX_NET_REPLICATED(...)` after
   `TNX_REGISTER_COMPONENT` — new
3. Live preview panel beside it using your `ImGui::TextUnformatted` + a
   simple manual syntax highlighter (color keywords purple-hot, types
   yellow, identifiers default fg)
