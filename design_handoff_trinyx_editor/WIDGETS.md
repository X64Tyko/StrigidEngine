# WIDGETS — Custom Atoms

ImGui doesn't ship these natively. Each is small (10–60 lines) and lives in
`TnxWidgets.{hpp,cpp}`. Reusable across every panel.

## Tier badge

A colored pill chip for the partition tier on each component.

```cpp
// TnxWidgets.hpp
namespace TnxWidgets {
    enum class Tier { Cold, Static, Volatile, Temporal };
    void TierBadge(Tier tier);
}

// TnxWidgets.cpp
void TnxWidgets::TierBadge(Tier tier)
{
    const char* label = "";
    ImVec4 color;
    switch (tier) {
        case Tier::Cold:     label = "COLD";     color = TnxStyle::Color::TierCold;     break;
        case Tier::Static:   label = "STATIC";   color = TnxStyle::Color::TierStatic;   break;
        case Tier::Volatile: label = "VOLATILE"; color = TnxStyle::Color::TierVolatile; break;
        case Tier::Temporal: label = "TEMPORAL"; color = TnxStyle::Color::TierTemporal; break;
    }

    ImGui::PushFont(TnxStyle::Font::MonoBold);
    const ImVec2 textSize = ImGui::CalcTextSize(label);
    const float padX = 6.0f, padY = 2.0f, dotR = 2.0f, dotGap = 4.0f;
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const ImVec2 size = ImVec2(padX*2 + dotR*2 + dotGap + textSize.x, textSize.y + padY*2);
    ImDrawList* dl = ImGui::GetWindowDrawList();

    ImVec4 bg  = color; bg.w  = 0.18f;
    ImVec4 brd = color; brd.w = 0.55f;

    dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), ImGui::ColorConvertFloat4ToU32(bg), 2.0f);
    dl->AddRect      (pos, ImVec2(pos.x + size.x, pos.y + size.y), ImGui::ColorConvertFloat4ToU32(brd), 2.0f, 0, 1.0f);
    dl->AddCircleFilled(ImVec2(pos.x + padX + dotR, pos.y + size.y*0.5f), dotR,
                        ImGui::ColorConvertFloat4ToU32(color));
    dl->AddText(ImVec2(pos.x + padX + dotR*2 + dotGap, pos.y + padY),
                ImGui::ColorConvertFloat4ToU32(color), label);

    ImGui::Dummy(size);
    ImGui::PopFont();
}
```

## Chip / Kbd

Generic pill text used everywhere (entity-cache-index, network-tick rate, queue tag).

```cpp
namespace TnxWidgets {
    enum class ChipStyle { Default, Purple, Yellow, Good, Warn, Bad };
    void Chip(const char* label, ChipStyle style = ChipStyle::Default, bool mono = true);
    void Kbd(const char* label);
}

void TnxWidgets::Chip(const char* label, ChipStyle style, bool mono)
{
    if (mono) ImGui::PushFont(TnxStyle::Font::MonoRegular);
    ImVec4 bg, fg, brd;
    using namespace TnxStyle::Color;
    switch (style) {
        case ChipStyle::Default: bg = BgElev;        fg = FgMuted;   brd = BorderSoft; break;
        case ChipStyle::Purple:  bg = PurpleWash;    fg = PurpleHot; brd = PurpleSoft; break;
        case ChipStyle::Yellow:  bg = ImVec4(0.35f,0.27f,0.02f,0.40f);
                                 fg = Yellow;        brd = YellowSoft; break;
        case ChipStyle::Good:    bg = ImVec4(0.10f,0.30f,0.15f,0.40f);
                                 fg = Good;          brd = Good; break;
        case ChipStyle::Warn:    bg = ImVec4(0.30f,0.20f,0.05f,0.40f);
                                 fg = Warn;          brd = Warn; break;
        case ChipStyle::Bad:     bg = ImVec4(0.30f,0.10f,0.10f,0.50f);
                                 fg = Bad;           brd = Bad; break;
    }

    const ImVec2 textSize = ImGui::CalcTextSize(label);
    const float padX = 6.0f, padY = 2.0f;
    const ImVec2 pos  = ImGui::GetCursorScreenPos();
    const ImVec2 size = ImVec2(padX*2 + textSize.x, textSize.y + padY*2);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), ImGui::ColorConvertFloat4ToU32(bg), 2.0f);
    dl->AddRect      (pos, ImVec2(pos.x + size.x, pos.y + size.y), ImGui::ColorConvertFloat4ToU32(brd), 2.0f, 0, 1.0f);
    dl->AddText(ImVec2(pos.x + padX, pos.y + padY), ImGui::ColorConvertFloat4ToU32(fg), label);
    ImGui::Dummy(size);
    if (mono) ImGui::PopFont();
}

void TnxWidgets::Kbd(const char* label)
{
    ImGui::PushFont(TnxStyle::Font::MonoRegular);
    using namespace TnxStyle::Color;
    const ImVec2 textSize = ImGui::CalcTextSize(label);
    const float padX = 5.0f, padY = 1.0f;
    const ImVec2 pos  = ImGui::GetCursorScreenPos();
    const ImVec2 size = ImVec2(ImMax(18.0f, padX*2 + textSize.x), textSize.y + padY*2);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), ImGui::ColorConvertFloat4ToU32(BgInput), 2.0f);
    dl->AddRect      (pos, ImVec2(pos.x + size.x, pos.y + size.y - 1), ImGui::ColorConvertFloat4ToU32(Border), 2.0f);
    dl->AddLine(ImVec2(pos.x, pos.y + size.y - 1), ImVec2(pos.x + size.x, pos.y + size.y - 1),
                ImGui::ColorConvertFloat4ToU32(Border), 2.0f); // bottom shadow
    dl->AddText(ImVec2(pos.x + (size.x - textSize.x)*0.5f, pos.y + padY),
                ImGui::ColorConvertFloat4ToU32(FgMuted), label);
    ImGui::Dummy(size);
    ImGui::PopFont();
}
```

## Panel header

Used to title every dockable panel. Icon + uppercase title + optional right-aligned content.

```cpp
namespace TnxWidgets {
    // Call after BeginChild/Begin. Don't call inside a column.
    void PanelHeader(const char* icon, const char* title, std::function<void()> right = {});
}

void TnxWidgets::PanelHeader(const char* icon, const char* title, std::function<void()> right)
{
    using namespace TnxStyle::Color;
    const float H = 30.0f;
    const ImVec2 start = ImGui::GetCursorScreenPos();
    const float wid = ImGui::GetContentRegionAvail().x;
    ImDrawList* dl = ImGui::GetWindowDrawList();

    dl->AddRectFilled(start, ImVec2(start.x + wid, start.y + H),
                      ImGui::ColorConvertFloat4ToU32(BgElev));
    dl->AddLine(ImVec2(start.x, start.y + H), ImVec2(start.x + wid, start.y + H),
                ImGui::ColorConvertFloat4ToU32(BorderSoft), 1.0f);

    ImGui::SetCursorScreenPos(ImVec2(start.x + 10, start.y + (H - ImGui::GetTextLineHeight())*0.5f));
    ImGui::PushStyleColor(ImGuiCol_Text, FgMuted);
    if (icon) ImGui::Text("%s", icon);
    ImGui::SameLine(0, 6);

    ImGui::PushFont(TnxStyle::Font::UiSemibold);
    // uppercase title with letter-spacing — fake the letter-spacing by inserting spaces? skip;
    // just rely on the all-caps text and a slightly larger letter-spacing-by-eye:
    char upper[64]; int i = 0;
    for (; title[i] && i < 63; ++i) upper[i] = (title[i] >= 'a' && title[i] <= 'z') ? title[i] - 32 : title[i];
    upper[i] = 0;
    ImGui::Text("%s", upper);
    ImGui::PopFont();
    ImGui::PopStyleColor();

    if (right) {
        ImGui::SameLine();
        ImGui::SetCursorPosX(wid - 4);  // pushes right side; caller draws right-side content
        right();
    }

    ImGui::SetCursorScreenPos(ImVec2(start.x, start.y + H + 1));
}
```

## Field row (label + input)

The atom of the inspector.

```cpp
namespace TnxWidgets {
    // Right-aligned label at fixed width; input fills remaining space.
    // Returns true if value was edited.
    bool FieldFloat (const char* label, float* v,       const char* unit = nullptr, float labelW = 90.f);
    bool FieldVec3  (const char* label, float v[3], float labelW = 90.f);
}

bool TnxWidgets::FieldFloat(const char* label, float* v, const char* unit, float labelW)
{
    using namespace TnxStyle::Color;
    ImGui::BeginGroup();
    ImGui::AlignTextToFramePadding();
    ImGui::PushStyleColor(ImGuiCol_Text, FgMuted);
    ImGui::TextRightAligned(labelW, label);   // helper that uses SetCursorPosX(labelW - CalcTextSize(label).x)
    ImGui::PopStyleColor();
    ImGui::SameLine(0, 6);

    ImGui::PushFont(TnxStyle::Font::MonoRegular);
    bool changed = ImGui::DragFloat(ImFormatString_("##%s", label), v, 0.01f, 0.f, 0.f, "%.3f");
    if (unit) {
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, FgDim);
        ImGui::Text("%s", unit);
        ImGui::PopStyleColor();
    }
    ImGui::PopFont();
    ImGui::EndGroup();
    return changed;
}

// FieldVec3: three DragFloats with X/Y/Z prefix labels colored by axis.
// Axis colors:
//   X = ImVec4(0.96f, 0.39f, 0.31f, 1.0f)  // hot red
//   Y = ImVec4(0.46f, 0.83f, 0.40f, 1.0f)  // green
//   Z = ImVec4(0.35f, 0.62f, 0.95f, 1.0f)  // blue
```

## Frame budget bar

Used in the Frame Budget strip (bottom-right) and in the Debugger panel.

```cpp
namespace TnxWidgets {
    void FrameBudgetBar(const char* label,
                        const char* freqHz,
                        float usedMs, float budgetMs,
                        ImVec4 color,
                        const char* note);
}

void TnxWidgets::FrameBudgetBar(const char* label, const char* freqHz,
                                float usedMs, float budgetMs, ImVec4 color, const char* note)
{
    using namespace TnxStyle::Color;
    ImGui::BeginGroup();

    // Row 1: dot + label + Hz + "used / budget ms"
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    dl->AddRectFilled(p, ImVec2(p.x + 8, p.y + 8), ImGui::ColorConvertFloat4ToU32(color), 2.0f);
    ImGui::Dummy(ImVec2(8, 8)); ImGui::SameLine();

    ImGui::PushFont(TnxStyle::Font::MonoBold);
    ImGui::Text("%s", label);
    ImGui::PopFont();
    ImGui::SameLine();

    ImGui::PushFont(TnxStyle::Font::MonoRegular);
    ImGui::PushStyleColor(ImGuiCol_Text, FgDim);
    ImGui::Text("%s", freqHz);
    ImGui::PopStyleColor();

    char rhs[64];
    ImFormatString(rhs, IM_ARRAYSIZE(rhs), "%.2f / %.2f ms", usedMs, budgetMs);
    float rhsW = ImGui::CalcTextSize(rhs).x;
    ImGui::SameLine(0, 0);
    ImGui::SetCursorPosX(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - rhsW);
    ImGui::Text("%s", rhs);
    ImGui::PopFont();

    // Row 2: bar
    const float W = ImGui::GetContentRegionAvail().x;
    const float H = 8.0f;
    ImVec2 b0 = ImGui::GetCursorScreenPos();
    ImVec2 b1 = ImVec2(b0.x + W, b0.y + H);
    dl->AddRectFilled(b0, b1, ImGui::ColorConvertFloat4ToU32(BgInput), 1.0f);
    float fillW = ImClamp(usedMs / budgetMs, 0.0f, 1.5f) * W;
    if (fillW > 0) {
        ImU32 fillCol = usedMs > budgetMs ? ImGui::ColorConvertFloat4ToU32(Bad)
                                          : ImGui::ColorConvertFloat4ToU32(color);
        dl->AddRectFilled(b0, ImVec2(b0.x + ImMin(fillW, W), b1.y), fillCol, 1.0f);
        // Glow
        ImVec4 glow = usedMs > budgetMs ? Bad : color; glow.w = 0.35f;
        dl->AddRectFilled(ImVec2(b0.x, b0.y - 1),
                          ImVec2(b0.x + ImMin(fillW, W), b1.y + 1),
                          ImGui::ColorConvertFloat4ToU32(glow), 1.0f);
    }
    // Budget tick at 100%
    dl->AddLine(ImVec2(b1.x, b0.y - 2), ImVec2(b1.x, b1.y + 2),
                ImGui::ColorConvertFloat4ToU32(FgDim), 1.0f);
    ImGui::Dummy(ImVec2(W, H + 2));

    // Row 3: note
    ImGui::PushStyleColor(ImGuiCol_Text, FgDim);
    ImGui::TextUnformatted(note);
    ImGui::PopStyleColor();

    ImGui::EndGroup();
}
```

## Sparkline

Two-liner — built-in `PlotLines` is enough for v1.

```cpp
// In drawing context, after some text:
ImGui::SameLine();
ImGui::PushStyleColor(ImGuiCol_PlotLines, TnxStyle::Color::Good);
ImGui::PlotLines("##spark", samples.data(), (int)samples.size(),
                 0, nullptr, FLT_MAX, FLT_MAX, ImVec2(80, 18));
ImGui::PopStyleColor();
```

For per-frame budget timelines, `ImPlot::PlotShaded` with a budget threshold
line gives the right look — see the Rollback Scrubber design.

## Button styles

Use `PushStyleColor` to swap between three button styles:

```cpp
namespace TnxWidgets {
    bool Button         (const char* label);   // default — grey w/ purple hover
    bool ButtonPrimary  (const char* label);   // purple — used sparingly (1-2 per screen)
    bool ButtonYellow   (const char* label);   // yellow — Play, danger, attention
    bool ButtonGhost    (const char* label);   // transparent — toolbar / dismiss
}

bool TnxWidgets::ButtonPrimary(const char* label)
{
    using namespace TnxStyle::Color;
    ImGui::PushStyleColor(ImGuiCol_Button,        Purple);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, PurpleHot);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  PurpleSoft);
    ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(1,1,1,1));
    ImGui::PushFont(TnxStyle::Font::UiSemibold);
    bool clicked = ImGui::Button(label);
    ImGui::PopFont();
    ImGui::PopStyleColor(4);
    return clicked;
}
```

## Command palette (overlay)

The command palette is just a modal popup, dimmed background. No fancy lib.

```cpp
namespace TnxPalette {
    struct Command {
        const char* title;
        const char* subtitle;
        const char* icon;
        const char* keybind;    // optional, e.g. "Cmd+Enter"
        std::function<void()> exec;
    };

    void Register(Command c);
    void Open();      // sets a flag; processed next frame
    void Draw();      // call once per frame at the top of your render
}

void TnxPalette::Draw()
{
    static bool open = false;
    static char query[128] = "";

    if (ImGui::IsKeyPressed(ImGuiKey_K) && (ImGui::IsKeyDown(ImGuiKey_LeftSuper) || ImGui::IsKeyDown(ImGuiKey_LeftCtrl))) {
        open = true;
        query[0] = '\0';
    }
    if (!open) return;

    // Dim full-screen overlay (no blur — flat 55% alpha)
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    dl->AddRectFilled(vp->WorkPos, ImVec2(vp->WorkPos.x + vp->WorkSize.x,
                                          vp->WorkPos.y + vp->WorkSize.y),
                      IM_COL32(13, 13, 20, 140));

    // Center the palette
    const float W = 620.0f;
    ImGui::SetNextWindowSize(ImVec2(W, 0), ImGuiCond_Always);
    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x*0.5f - W*0.5f,
                                   vp->WorkPos.y + vp->WorkSize.y*0.12f),
                            ImGuiCond_Always);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_Border, TnxStyle::Color::PurpleSoft);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, TnxStyle::Color::BgPanel);

    if (ImGui::Begin("##palette", &open,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::PushFont(TnxStyle::Font::MonoRegular);
        ImGui::SetKeyboardFocusHere();
        ImGui::InputTextWithHint("##q", "find command, entity, asset…", query, IM_ARRAYSIZE(query));
        ImGui::PopFont();

        ImGui::Separator();
        for (const Command& cmd : MatchAndSort(query)) {
            // Top match — render with purple wash
            // Others — flat
            DrawCommandRow(cmd, /*topMatch=*/ &cmd == &MatchAndSort(query).front());
        }
    }
    ImGui::End();

    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);

    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) open = false;
}
```

**Fuzzy match algorithm:** SubSequence + bonus-points scoring is sufficient
for v1. There's a public-domain ~200-line `fts::fuzzy_match` (forrestthewoods)
that's fine to vendor.

## Tooltip with chip / icon

For Slab Heatmap tooltips ("BarrelAssembly owned by Turret_3"). Just nest
the chip widget inside an `ImGui::BeginTooltip()`.

```cpp
if (ImGui::IsItemHovered()) {
    ImGui::BeginTooltip();
    ImGui::Text("cell (e 0x00B1F4, Vel·T-1)");
    ImGui::Text("BarrelAssembly owned by Turret_3");
    TnxWidgets::Chip("chunk 14 [62/64]");
    ImGui::SameLine();
    TnxWidgets::Chip("phys", TnxWidgets::ChipStyle::Purple);
    ImGui::EndTooltip();
}
```
