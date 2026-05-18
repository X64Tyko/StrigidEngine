#include "TnxStyle.h"

#ifdef IMGUI_ENABLE_FREETYPE
#include "imgui_freetype.h" // ImGuiFreeTypeLoaderFlags_LightHinting
#endif

#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

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

void LoadFonts(const char* projectDir, const char* engineRoot, float dpiScale)
{
    namespace fs = std::filesystem;

    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();

    // Build candidate base directories in priority order.
    std::vector<std::string> searchPaths;
    if (projectDir && projectDir[0])
        searchPaths.push_back(std::string(projectDir) + "/assets/fonts");
    if (engineRoot && engineRoot[0])
        searchPaths.push_back(std::string(engineRoot) + "/Trinyx/assets/fonts");
    searchPaths.push_back("assets/fonts"); // CWD fallback

    ImFontConfig cfg;
    cfg.OversampleH         = 2;
    cfg.OversampleV         = 2;
    cfg.RasterizerDensity   = dpiScale > 1.01f ? dpiScale : 1.0f;

    const float UI_SIZES      = 20.0f * dpiScale;
    const float DISPLAY_SIZE  = 23.0f * dpiScale;
    const float MONO_SIZE     = 18.0f * dpiScale;

    auto tryLoad = [&](const char* filename, float size) -> ImFont*
    {
        for (const auto& base : searchPaths)
        {
            std::string path = base + "/" + filename;
            if (!fs::exists(path)) continue;
            ImFont* f = io.Fonts->AddFontFromFileTTF(path.c_str(), size, &cfg);
            if (f) return f;
        }
        fprintf(stderr, "[TnxStyle] Font not found in any search path: %s\n", filename);
        return nullptr;
    };

    // UI default — Manrope Regular (first-added = ImGui default font)
    Font::UiRegular      = tryLoad("Manrope-Regular.ttf",      UI_SIZES);
    Font::UiSemibold     = tryLoad("Manrope-SemiBold.ttf",     UI_SIZES);
    Font::DisplayRegular = tryLoad("SpaceGrotesk-Regular.ttf",  DISPLAY_SIZE);
    Font::DisplaySemibold= tryLoad("SpaceGrotesk-SemiBold.ttf", DISPLAY_SIZE);
    Font::DisplayBold    = tryLoad("SpaceGrotesk-Bold.ttf",     DISPLAY_SIZE + 2.0f * dpiScale);
    Font::MonoRegular    = tryLoad("JetBrainsMono-Regular.ttf", MONO_SIZE);
    Font::MonoBold       = tryLoad("JetBrainsMono-SemiBold.ttf",MONO_SIZE);

    // If nothing loaded (fonts not downloaded yet), add the built-in font as fallback
    // so the editor still renders something readable.
    bool anyLoaded = Font::UiRegular || Font::UiSemibold || Font::DisplayRegular;
    if (!anyLoaded)
    {
        ImFontConfig fallback;
        fallback.SizePixels  = UI_SIZES;
        fallback.OversampleH = 2;
        fallback.OversampleV = 2;
        io.Fonts->AddFontDefault(&fallback);
    }

    // IMGUI_ENABLE_FREETYPE (defined at compile time) activates FreeType automatically.
    // FontLoaderFlags is read at build time; the Vulkan backend builds the atlas via
    // ImGuiBackendFlags_RendererHasTextures — do NOT call io.Fonts->Build() here.
#ifdef IMGUI_ENABLE_FREETYPE
    io.Fonts->FontLoaderFlags = ImGuiFreeTypeLoaderFlags_LightHinting;
#endif
}

void Apply()
{
    using namespace Color;
    ImGuiStyle& s = ImGui::GetStyle();

    // Layout
    s.WindowPadding       = ImVec2(0, 0);
    s.FramePadding        = ImVec2(12, 7);
    s.ItemSpacing         = ImVec2(10, 10);
    s.ItemInnerSpacing    = ImVec2(7, 6);
    s.IndentSpacing       = 20.0f;
    s.ScrollbarSize       = 14.0f;
    s.GrabMinSize         = 12.0f;
    s.TabBarOverlineSize  = 2.0f;

    // Borders + rounding
    s.WindowBorderSize    = 1.0f;
    s.FrameBorderSize     = 1.0f;
    s.PopupBorderSize     = 1.0f;
    s.TabBorderSize       = 0.0f;
    s.ChildBorderSize     = 1.0f;

    s.WindowRounding      = 4.0f;
    s.FrameRounding       = 2.0f;
    s.PopupRounding       = 6.0f;
    s.ScrollbarRounding   = 1.0f;
    s.GrabRounding        = 2.0f;
    s.TabRounding         = 2.0f;
    s.ChildRounding        = 4.0f;
    s.DockingSeparatorSize = 8.0f;

    // Alignment
    s.WindowTitleAlign    = ImVec2(0.0f, 0.5f);
    s.ButtonTextAlign     = ImVec2(0.5f, 0.5f);
    s.SelectableTextAlign = ImVec2(0.0f, 0.5f);

    s.AntiAliasedLines       = true;
    s.AntiAliasedLinesUseTex = true;
    s.AntiAliasedFill        = true;

    ImVec4* c = s.Colors;
    c[ImGuiCol_Text]                 = Fg;
    c[ImGuiCol_TextDisabled]         = FgDim;
    c[ImGuiCol_WindowBg]             = BgApp;
    c[ImGuiCol_ChildBg]              = BgPanel;
    c[ImGuiCol_PopupBg]              = BgPanel;
    c[ImGuiCol_Border]               = Border;
    c[ImGuiCol_BorderShadow]         = ImVec4(0, 0, 0, 0);

    c[ImGuiCol_FrameBg]              = BgInput;
    c[ImGuiCol_FrameBgHovered]       = ImVec4(BgInput.x, BgInput.y, BgInput.z + 0.05f, 1.0f);
    c[ImGuiCol_FrameBgActive]        = PurpleFaint;

    c[ImGuiCol_TitleBg]              = BgDeep;
    c[ImGuiCol_TitleBgActive]        = PurpleFaint;
    c[ImGuiCol_TitleBgCollapsed]     = BgDeep;
    c[ImGuiCol_MenuBarBg]            = BgDeep;

    c[ImGuiCol_ScrollbarBg]          = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_ScrollbarGrab]        = Border;
    c[ImGuiCol_ScrollbarGrabHovered] = BorderStrong;
    c[ImGuiCol_ScrollbarGrabActive]  = PurpleSoft;

    c[ImGuiCol_CheckMark]            = Purple;
    c[ImGuiCol_SliderGrab]           = Purple;
    c[ImGuiCol_SliderGrabActive]     = PurpleHot;

    c[ImGuiCol_Button]               = BgElev;
    c[ImGuiCol_ButtonHovered]        = PurpleFaint;
    c[ImGuiCol_ButtonActive]         = PurpleSoft;

    c[ImGuiCol_Header]               = PurpleWash;
    c[ImGuiCol_HeaderHovered]        = PurpleFaint;
    c[ImGuiCol_HeaderActive]         = PurpleSoft;

    c[ImGuiCol_Separator]            = BorderSoft;
    c[ImGuiCol_SeparatorHovered]     = Border;
    c[ImGuiCol_SeparatorActive]      = PurpleSoft;

    c[ImGuiCol_ResizeGrip]           = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_ResizeGripHovered]    = PurpleFaint;
    c[ImGuiCol_ResizeGripActive]     = PurpleSoft;

    c[ImGuiCol_Tab]                  = BgDeep;
    c[ImGuiCol_TabHovered]           = BgElev;
    c[ImGuiCol_TabSelected]          = PurpleFaint;
    c[ImGuiCol_TabSelectedOverline]  = Purple;
    c[ImGuiCol_TabDimmed]            = BgDeep;
    c[ImGuiCol_TabDimmedSelected]    = BgElev;

    c[ImGuiCol_DockingPreview]       = ImVec4(Purple.x, Purple.y, Purple.z, 0.4f);
    c[ImGuiCol_DockingEmptyBg]       = BgDeep;

    c[ImGuiCol_PlotLines]            = ThBrain;
    c[ImGuiCol_PlotLinesHovered]     = Yellow;
    c[ImGuiCol_PlotHistogram]        = Purple;
    c[ImGuiCol_PlotHistogramHovered] = PurpleHot;

    c[ImGuiCol_TableHeaderBg]        = BgDeep;
    c[ImGuiCol_TableBorderStrong]    = Border;
    c[ImGuiCol_TableBorderLight]     = BorderSoft;
    c[ImGuiCol_TableRowBg]           = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_TableRowBgAlt]        = ImVec4(1, 1, 1, 0.015f);

    c[ImGuiCol_TextSelectedBg]       = PurpleSoft;
    c[ImGuiCol_DragDropTarget]       = Yellow;
    c[ImGuiCol_NavHighlight]         = Purple;
}

} // namespace TnxStyle