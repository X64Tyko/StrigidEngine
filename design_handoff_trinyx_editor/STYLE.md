# STYLE — Drop-in ImGui Theme

Pre-computed sRGB values (oklch → sRGB) so you can paste these directly
into your code without re-converting.

## Color table

| Token | Hex | RGB | ImVec4 |
|---|---|---|---|
| `bg-viewport` | `#07070A` | `(7, 7, 10)` | `ImVec4(0.027f, 0.027f, 0.040f, 1.0f)` |
| `bg-deep` | `#0D0D11` | `(13, 13, 17)` | `ImVec4(0.050f, 0.050f, 0.068f, 1.0f)` |
| `bg-app` | `#131319` | `(19, 19, 25)` | `ImVec4(0.075f, 0.075f, 0.098f, 1.0f)` |
| `bg-panel` | `#1A1A21` | `(26, 26, 33)` | `ImVec4(0.101f, 0.101f, 0.128f, 1.0f)` |
| `bg-elev` | `#23232B` | `(35, 35, 43)` | `ImVec4(0.138f, 0.138f, 0.167f, 1.0f)` |
| `bg-input` | `#0B0B0F` | `(11, 11, 15)` | `ImVec4(0.042f, 0.042f, 0.060f, 1.0f)` |
| `border` | `#2D2C36` | `(45, 44, 54)` | `ImVec4(0.178f, 0.174f, 0.213f, 1.0f)` |
| `border-strong` | `#4D4B59` | `(77, 75, 89)` | `ImVec4(0.300f, 0.295f, 0.347f, 1.0f)` |
| `border-soft` | `#1E1E26` | `(30, 30, 38)` | `ImVec4(0.119f, 0.119f, 0.148f, 1.0f)` |
| `fg` | `#EBEBEE` | `(235, 235, 238)` | `ImVec4(0.921f, 0.920f, 0.932f, 1.0f)` |
| `fg-muted` | `#98979F` | `(152, 151, 159)` | `ImVec4(0.595f, 0.593f, 0.625f, 1.0f)` |
| `fg-dim` | `#63626B` | `(99, 98, 107)` | `ImVec4(0.387f, 0.384f, 0.419f, 1.0f)` |
| `fg-ghost` | `#424148` | `(66, 65, 72)` | `ImVec4(0.259f, 0.256f, 0.284f, 1.0f)` |
| `purple` (primary) | `#906AE5` | `(144, 106, 229)` | `ImVec4(0.565f, 0.415f, 0.898f, 1.0f)` |
| `purple-hot` | `#AA7EFF` | `(170, 126, 255)` | `ImVec4(0.666f, 0.495f, 1.000f, 1.0f)` |
| `purple-soft` | `#4E3680` | `(78, 54, 128)` | `ImVec4(0.305f, 0.213f, 0.503f, 1.0f)` |
| `purple-faint` | `#302749` | `(48, 39, 73)` | `ImVec4(0.189f, 0.153f, 0.285f, 1.0f)` |
| `yellow` (accent) | `#F7CD3A` | `(247, 205, 58)` | `ImVec4(0.967f, 0.803f, 0.227f, 1.0f)` |
| `yellow-hot` | `#FFE244` | `(255, 226, 68)` | `ImVec4(1.000f, 0.888f, 0.265f, 1.0f)` |
| `yellow-soft` | `#A28200` | `(162, 130, 0)` | `ImVec4(0.636f, 0.511f, 0.000f, 1.0f)` |
| `good` | `#61C568` | `(97, 197, 104)` | `ImVec4(0.382f, 0.771f, 0.407f, 1.0f)` |
| `warn` | `#F9AD26` | `(249, 173, 38)` | `ImVec4(0.976f, 0.677f, 0.147f, 1.0f)` |
| `bad` | `#F4514F` | `(244, 81, 79)` | `ImVec4(0.958f, 0.318f, 0.311f, 1.0f)` |
| `info` | `#16B3EB` | `(22, 179, 235)` | `ImVec4(0.087f, 0.704f, 0.921f, 1.0f)` |
| `tier-cold` | `#43A6C3` | `(67, 166, 195)` | `ImVec4(0.262f, 0.653f, 0.764f, 1.0f)` |
| `tier-static` | `#B0A57A` | `(176, 165, 122)` | `ImVec4(0.691f, 0.646f, 0.478f, 1.0f)` |
| `tier-volatile` | `#FB8371` | `(251, 131, 113)` | `ImVec4(0.984f, 0.513f, 0.441f, 1.0f)` |
| `tier-temporal` | `#A181EF` | `(161, 129, 239)` | `ImVec4(0.630f, 0.506f, 0.939f, 1.0f)` |
| `th-sentinel` | `#00D1DA` | `(0, 209, 218)` | `ImVec4(0.000f, 0.821f, 0.857f, 1.0f)` |
| `th-brain` | `#E493F6` | `(228, 147, 246)` | `ImVec4(0.894f, 0.577f, 0.966f, 1.0f)` |
| `th-encoder` | `#F7CD3A` | `(247, 205, 58)` | `ImVec4(0.967f, 0.803f, 0.227f, 1.0f)` |

## TnxStyle.hpp — drop-in style header

```cpp
// TnxStyle.hpp
#pragma once
#include <imgui.h>

namespace TnxStyle {

namespace Color {
    constexpr ImVec4 BgViewport     {0.027f, 0.027f, 0.040f, 1.0f};
    constexpr ImVec4 BgDeep         {0.050f, 0.050f, 0.068f, 1.0f};
    constexpr ImVec4 BgApp          {0.075f, 0.075f, 0.098f, 1.0f};
    constexpr ImVec4 BgPanel        {0.101f, 0.101f, 0.128f, 1.0f};
    constexpr ImVec4 BgElev         {0.138f, 0.138f, 0.167f, 1.0f};
    constexpr ImVec4 BgInput        {0.042f, 0.042f, 0.060f, 1.0f};
    constexpr ImVec4 Border         {0.178f, 0.174f, 0.213f, 1.0f};
    constexpr ImVec4 BorderStrong   {0.300f, 0.295f, 0.347f, 1.0f};
    constexpr ImVec4 BorderSoft     {0.119f, 0.119f, 0.148f, 1.0f};
    constexpr ImVec4 Fg             {0.921f, 0.920f, 0.932f, 1.0f};
    constexpr ImVec4 FgMuted        {0.595f, 0.593f, 0.625f, 1.0f};
    constexpr ImVec4 FgDim          {0.387f, 0.384f, 0.419f, 1.0f};
    constexpr ImVec4 FgGhost        {0.259f, 0.256f, 0.284f, 1.0f};

    constexpr ImVec4 Purple         {0.565f, 0.415f, 0.898f, 1.0f};
    constexpr ImVec4 PurpleHot      {0.666f, 0.495f, 1.000f, 1.0f};
    constexpr ImVec4 PurpleSoft     {0.305f, 0.213f, 0.503f, 1.0f};
    constexpr ImVec4 PurpleFaint    {0.189f, 0.153f, 0.285f, 1.0f};
    constexpr ImVec4 PurpleWash     {0.189f, 0.153f, 0.285f, 0.50f};

    constexpr ImVec4 Yellow         {0.967f, 0.803f, 0.227f, 1.0f};
    constexpr ImVec4 YellowHot      {1.000f, 0.888f, 0.265f, 1.0f};
    constexpr ImVec4 YellowSoft     {0.636f, 0.511f, 0.000f, 1.0f};
    constexpr ImVec4 YellowOnYellow {0.165f, 0.142f, 0.020f, 1.0f}; // text on yellow btn

    constexpr ImVec4 Good           {0.382f, 0.771f, 0.407f, 1.0f};
    constexpr ImVec4 Warn           {0.976f, 0.677f, 0.147f, 1.0f};
    constexpr ImVec4 Bad            {0.958f, 0.318f, 0.311f, 1.0f};
    constexpr ImVec4 Info           {0.087f, 0.704f, 0.921f, 1.0f};

    constexpr ImVec4 TierCold       {0.262f, 0.653f, 0.764f, 1.0f};
    constexpr ImVec4 TierStatic     {0.691f, 0.646f, 0.478f, 1.0f};
    constexpr ImVec4 TierVolatile   {0.984f, 0.513f, 0.441f, 1.0f};
    constexpr ImVec4 TierTemporal   {0.630f, 0.506f, 0.939f, 1.0f};

    constexpr ImVec4 ThSentinel     {0.000f, 0.821f, 0.857f, 1.0f};
    constexpr ImVec4 ThBrain        {0.894f, 0.577f, 0.966f, 1.0f};
    constexpr ImVec4 ThEncoder      {0.967f, 0.803f, 0.227f, 1.0f};
}

namespace Font {
    extern ImFont* DisplayRegular;   // Space Grotesk 400
    extern ImFont* DisplaySemibold;  // Space Grotesk 600
    extern ImFont* DisplayBold;      // Space Grotesk 700
    extern ImFont* UiRegular;        // Manrope 400
    extern ImFont* UiSemibold;       // Manrope 600
    extern ImFont* MonoRegular;      // JetBrains Mono 400
    extern ImFont* MonoBold;         // JetBrains Mono 600
}

void Apply();
void LoadFonts();
} // namespace TnxStyle
```

## TnxStyle.cpp — Apply() implementation

```cpp
// TnxStyle.cpp
#include "TnxStyle.hpp"
#include <imgui_freetype.h>

namespace TnxStyle::Font {
    ImFont* DisplayRegular  = nullptr;
    ImFont* DisplaySemibold = nullptr;
    ImFont* DisplayBold     = nullptr;
    ImFont* UiRegular       = nullptr;
    ImFont* UiSemibold      = nullptr;
    ImFont* MonoRegular     = nullptr;
    ImFont* MonoBold        = nullptr;
}

namespace TnxStyle {

void LoadFonts()
{
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();

    ImFontConfig cfg;
    cfg.OversampleH = 2;
    cfg.OversampleV = 2;
    cfg.RasterizerDensity = 1.5f; // hidpi crispness via FreeType

    const float UI_SIZE      = 13.0f;
    const float DISPLAY_SIZE = 14.0f;
    const float MONO_SIZE    = 12.5f;

    // Default font is the UI regular at 13px.
    Font::UiRegular       = io.Fonts->AddFontFromFileTTF("assets/fonts/Manrope-Regular.ttf",      UI_SIZE,      &cfg);
    Font::UiSemibold      = io.Fonts->AddFontFromFileTTF("assets/fonts/Manrope-SemiBold.ttf",     UI_SIZE,      &cfg);
    Font::DisplayRegular  = io.Fonts->AddFontFromFileTTF("assets/fonts/SpaceGrotesk-Regular.ttf", DISPLAY_SIZE, &cfg);
    Font::DisplaySemibold = io.Fonts->AddFontFromFileTTF("assets/fonts/SpaceGrotesk-SemiBold.ttf",DISPLAY_SIZE, &cfg);
    Font::DisplayBold     = io.Fonts->AddFontFromFileTTF("assets/fonts/SpaceGrotesk-Bold.ttf",    DISPLAY_SIZE + 2.0f, &cfg);
    Font::MonoRegular     = io.Fonts->AddFontFromFileTTF("assets/fonts/JetBrainsMono-Regular.ttf",MONO_SIZE,    &cfg);
    Font::MonoBold        = io.Fonts->AddFontFromFileTTF("assets/fonts/JetBrainsMono-SemiBold.ttf",MONO_SIZE,   &cfg);

    // Use FreeType for crisp rendering.
    io.Fonts->FontBuilderIO = ImGuiFreeType::GetBuilderForFreeType();
    io.Fonts->FontBuilderFlags = ImGuiFreeTypeBuilderFlags_LightHinting;
    io.Fonts->Build();
}

void Apply()
{
    using namespace Color;
    ImGuiStyle& s = ImGui::GetStyle();

    // Layout
    s.WindowPadding         = ImVec2(0, 0);   // panels paint their own padding
    s.FramePadding          = ImVec2(8, 4);
    s.ItemSpacing           = ImVec2(8, 6);
    s.ItemInnerSpacing      = ImVec2(6, 4);
    s.IndentSpacing         = 14.0f;
    s.ScrollbarSize         = 10.0f;
    s.GrabMinSize           = 8.0f;

    // Borders + rounding
    s.WindowBorderSize      = 1.0f;
    s.FrameBorderSize       = 1.0f;
    s.PopupBorderSize       = 1.0f;
    s.TabBorderSize         = 0.0f;
    s.ChildBorderSize       = 1.0f;

    s.WindowRounding        = 4.0f;
    s.FrameRounding         = 2.0f;
    s.PopupRounding         = 6.0f;
    s.ScrollbarRounding     = 1.0f;
    s.GrabRounding          = 2.0f;
    s.TabRounding           = 2.0f;
    s.ChildRounding         = 4.0f;

    // Alignment
    s.WindowTitleAlign      = ImVec2(0.0f, 0.5f);
    s.ButtonTextAlign       = ImVec2(0.5f, 0.5f);
    s.SelectableTextAlign   = ImVec2(0.0f, 0.5f);

    s.AntiAliasedLines      = true;
    s.AntiAliasedLinesUseTex= true;
    s.AntiAliasedFill       = true;

    // Colors
    ImVec4* c = s.Colors;
    c[ImGuiCol_Text]                  = Fg;
    c[ImGuiCol_TextDisabled]          = FgDim;
    c[ImGuiCol_WindowBg]              = BgApp;
    c[ImGuiCol_ChildBg]               = BgPanel;
    c[ImGuiCol_PopupBg]               = BgPanel;
    c[ImGuiCol_Border]                = Border;
    c[ImGuiCol_BorderShadow]          = ImVec4(0,0,0,0);

    c[ImGuiCol_FrameBg]               = BgInput;
    c[ImGuiCol_FrameBgHovered]        = ImVec4(BgInput.x, BgInput.y, BgInput.z + 0.05f, 1.0f);
    c[ImGuiCol_FrameBgActive]         = PurpleFaint;

    c[ImGuiCol_TitleBg]               = BgDeep;
    c[ImGuiCol_TitleBgActive]         = BgElev;
    c[ImGuiCol_TitleBgCollapsed]      = BgDeep;
    c[ImGuiCol_MenuBarBg]             = BgDeep;

    c[ImGuiCol_ScrollbarBg]           = ImVec4(0,0,0,0);
    c[ImGuiCol_ScrollbarGrab]         = Border;
    c[ImGuiCol_ScrollbarGrabHovered]  = BorderStrong;
    c[ImGuiCol_ScrollbarGrabActive]   = PurpleSoft;

    c[ImGuiCol_CheckMark]             = Purple;
    c[ImGuiCol_SliderGrab]            = Purple;
    c[ImGuiCol_SliderGrabActive]      = PurpleHot;

    c[ImGuiCol_Button]                = BgElev;
    c[ImGuiCol_ButtonHovered]         = PurpleFaint;
    c[ImGuiCol_ButtonActive]          = PurpleSoft;

    c[ImGuiCol_Header]                = PurpleWash;
    c[ImGuiCol_HeaderHovered]         = PurpleFaint;
    c[ImGuiCol_HeaderActive]          = PurpleSoft;

    c[ImGuiCol_Separator]             = BorderSoft;
    c[ImGuiCol_SeparatorHovered]      = Border;
    c[ImGuiCol_SeparatorActive]       = PurpleSoft;

    c[ImGuiCol_ResizeGrip]            = ImVec4(0,0,0,0);
    c[ImGuiCol_ResizeGripHovered]     = PurpleFaint;
    c[ImGuiCol_ResizeGripActive]      = PurpleSoft;

    c[ImGuiCol_Tab]                   = BgApp;
    c[ImGuiCol_TabHovered]            = BgElev;
    c[ImGuiCol_TabActive]             = BgPanel;
    c[ImGuiCol_TabUnfocused]          = BgApp;
    c[ImGuiCol_TabUnfocusedActive]    = BgPanel;

    c[ImGuiCol_DockingPreview]        = ImVec4(Purple.x, Purple.y, Purple.z, 0.4f);
    c[ImGuiCol_DockingEmptyBg]        = BgDeep;

    c[ImGuiCol_PlotLines]             = ThBrain;
    c[ImGuiCol_PlotLinesHovered]      = Yellow;
    c[ImGuiCol_PlotHistogram]         = Purple;
    c[ImGuiCol_PlotHistogramHovered]  = PurpleHot;

    c[ImGuiCol_TableHeaderBg]         = BgDeep;
    c[ImGuiCol_TableBorderStrong]     = Border;
    c[ImGuiCol_TableBorderLight]      = BorderSoft;
    c[ImGuiCol_TableRowBg]            = ImVec4(0,0,0,0);
    c[ImGuiCol_TableRowBgAlt]         = ImVec4(1,1,1,0.015f);

    c[ImGuiCol_TextSelectedBg]        = PurpleSoft;
    c[ImGuiCol_DragDropTarget]        = Yellow;
    c[ImGuiCol_NavHighlight]          = Purple;
}

} // namespace TnxStyle
```

## Wire it up

```cpp
// At engine init, before first ImGui frame:
TnxStyle::LoadFonts();
TnxStyle::Apply();

// Push specific fonts for specific widgets, e.g. a wordmark in the top bar:
ImGui::PushFont(TnxStyle::Font::DisplayBold);
ImGui::Text("trinyx");
ImGui::PopFont();
```

## Spacing / sizing reference

Match these in your widgets to align with the mocks:

| Element | Size |
|---|---|
| Top bar (workspace + PIE controls) | `44px` |
| Toolbar (gizmo + view toggles) | `34px` |
| Status bar | `22px` |
| Panel header height | `30px` |
| Button — default | `26px` |
| Button — small | `22px` |
| Field input height | `22px` |
| Chip height | `18px` |
| Kbd height | `18px` |
| Tier badge height | `16px` |
| Panel padding (outer) | `8px` between panels |
| Component block padding (inspector) | `8px / 12px` |
| Field row height | `24px` (min) |
| Workspace pill — padding | `4px 10px` |

## Anti-patterns

- **Don't enable `ImGuiCol_BorderShadow`.** Flat borders only — adding shadow breaks the design language.
- **Don't use `WindowRounding > 4`.** The mocks intentionally feel "sharp / pro-tool".
- **Don't tint the viewport background.** Keep `BgViewport` as the deepest neutral; let the scene render shader handle vignette / atmosphere.
- **Don't use default `Header` blue.** The purple-wash header is part of the brand language.
