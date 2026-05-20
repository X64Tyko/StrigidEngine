#pragma once
#if !defined(TNX_ENABLE_EDITOR)
#error "ConstructEditorWindow.h requires TNX_ENABLE_EDITOR"
#endif

#include "EditorPanel.h"
#include "NodeGraphCanvas.h"
#include <string>
#include <vector>

/// One open Construct type in the editor — node graph per tick slot + view/composition list.
struct ConstructDoc
{
    std::string TypeName;

    /// One graph per tick slot. Index matches TickSlot enum.
    enum class TickSlot : int { PrePhysics = 0, PhysicsStep, PostPhysics, ScalarUpdate, Count };
    NodeGraphCanvas Graphs[static_cast<int>(TickSlot::Count)];
    int ActiveSlot = static_cast<int>(TickSlot::ScalarUpdate);

    struct ViewEntry  { char TypeName[64] = "EMyEntity";  char MemberName[64] = "Body"; };
    struct OwnedEntry { char TypeName[64] = "MySubObject"; char MemberName[64] = "Sub";  };

    std::vector<ViewEntry>  Views;
    std::vector<OwnedEntry> Owned;

    char OutputPath[512] = {};
    char StatusMsg[256]  = {};
    bool bStatusError    = false;

    explicit ConstructDoc(const char* typeName);
};

/// Dockable editor window for Construct types.
/// Tab bar across the top — one tab per open ConstructDoc.
/// Per-tab layout: Views/Owned panel (left) | Node graph (centre) | Info (right).
class ConstructEditorWindow : public EditorPanel
{
public:
    ConstructEditorWindow();
    ~ConstructEditorWindow() override = default;

    void Draw(EditorState& state) override;

    /// Open (or focus) a Construct type by name.
    void OpenConstruct(const char* typeName);

private:
    std::vector<ConstructDoc> Docs;
    int ActiveTab = 0;

    // New-tab dialog
    char NewTypeBuf[64]   = "MyConstruct";
    bool bShowNewDialog   = false;

    // Pane visibility (persists for the window lifetime, not per-doc)
    bool bLeftPaneVisible  = true;
    bool bRightPaneVisible = true;

    void DrawTabBar();
    void DrawCompositionPanel(ConstructDoc& doc, float height);
    void DrawSlotTabs(ConstructDoc& doc, float width, float height);
    void DrawInfoPanel(ConstructDoc& doc, float height, EditorState& state);
};
