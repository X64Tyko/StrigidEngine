#include "Panels/ConstructEditorWindow.h"
#if !defined(TNX_ENABLE_EDITOR)
#error "ConstructEditorWindow.cpp requires TNX_ENABLE_EDITOR"
#endif

#include "EditorState.h"
#include "EngineConfig.h"
#include "TnxStyle.h"
#include "TnxWidgets.h"
#include "imgui.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

static const char* SlotNames[] = { "Pre-Physics", "Physics Step", "Post-Physics", "Scalar Update" };

// ---------------------------------------------------------------------------
// ConstructDoc
// ---------------------------------------------------------------------------

ConstructDoc::ConstructDoc(const char* typeName)
    : TypeName(typeName)
{
    // Each graph starts with an appropriate event node for its slot.
    using TS  = ConstructDoc::TickSlot;
    using NK  = NodeGraphCanvas::NodeKind;
    static const NK SlotEvents[] = {
        NK::Event_OnPrePhysics,
        NK::Event_OnSpawn,         // PhysicsStep has no direct event; use OnSpawn as placeholder
        NK::Event_OnPostPhysics,
        NK::Event_OnUpdate,
    };
    for (int i = 0; i < static_cast<int>(TS::Count); ++i)
    {
        Graphs[i].Clear();
        Graphs[i].AddEventNode(SlotEvents[i], 80.0f, 120.0f);
    }
}

// ---------------------------------------------------------------------------
// ConstructEditorWindow
// ---------------------------------------------------------------------------

ConstructEditorWindow::ConstructEditorWindow()
    : EditorPanel("Construct Editor")
{
}

void ConstructEditorWindow::OpenConstruct(const char* typeName)
{
    for (int i = 0; i < static_cast<int>(Docs.size()); ++i)
    {
        if (Docs[i].TypeName == typeName) { ActiveTab = i; bVisible = true; return; }
    }
    Docs.emplace_back(typeName);
    ActiveTab = static_cast<int>(Docs.size()) - 1;
    bVisible  = true;
}

// ---------------------------------------------------------------------------
// Draw
// ---------------------------------------------------------------------------

void ConstructEditorWindow::Draw(EditorState& state)
{
    if (!BeginPadded(ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
    { ImGui::End(); return; }

    TnxWidgets::PanelHeader(nullptr, "Construct Editor");

    // --- Top toolbar ---
    if (ImGui::Button("+ New"))
        bShowNewDialog = true;

    if (bShowNewDialog)
    {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(160.0f);
        ImGui::InputText("##NewName", NewTypeBuf, sizeof(NewTypeBuf));
        ImGui::SameLine();
        if (ImGui::Button("Create"))
        {
            if (NewTypeBuf[0] != '\0') { OpenConstruct(NewTypeBuf); bShowNewDialog = false; }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) bShowNewDialog = false;
    }

    if (Docs.empty())
    {
        ImGui::Separator();
        ImGui::TextDisabled("No Constructs open. Click '+ New' or open one from the Content Browser.");
        ImGui::End();
        return;
    }

    // --- Tab bar + pane toggles on the same line ---
    DrawTabBar();

    if (ActiveTab < 0 || ActiveTab >= static_cast<int>(Docs.size()))
    { ImGui::End(); return; }

    ConstructDoc& doc = Docs[static_cast<size_t>(ActiveTab)];

    // Pane toggle buttons — drawn on their own line, right-aligned before the separator.
    {
        float spacing = ImGui::GetStyle().ItemSpacing.x;
        float btnW    = ImGui::CalcTextSize("[ Comp ]").x + ImGui::GetStyle().FramePadding.x * 2.0f;
        float totalBtns = btnW * 2.0f + spacing;
        ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x - totalBtns);
        if (ImGui::SmallButton(bLeftPaneVisible  ? "[ Comp ]" : "[ > ]")) bLeftPaneVisible  = !bLeftPaneVisible;
        ImGui::SameLine();
        if (ImGui::SmallButton(bRightPaneVisible ? "[ Info ]" : "[ < ]")) bRightPaneVisible = !bRightPaneVisible;
    }

    ImGui::Separator();

    // --- Three-column layout: Composition | Node Graph | Info ---
    float totalH = ImGui::GetContentRegionAvail().y;

    if (bLeftPaneVisible)
    {
        DrawCompositionPanel(doc, totalH);
        ImGui::SameLine();
    }
    DrawSlotTabs(doc, 0.0f, totalH);
    if (bRightPaneVisible)
    {
        ImGui::SameLine();
        DrawInfoPanel(doc, totalH, state);
    }

    ImGui::End();
}

void ConstructEditorWindow::DrawTabBar()
{
    if (ImGui::BeginTabBar("##ConstructTabs"))
    {
        for (int i = 0; i < static_cast<int>(Docs.size()); ++i)
        {
            bool open = true;
            ImGuiTabItemFlags flags = (i == ActiveTab) ? ImGuiTabItemFlags_SetSelected : 0;
            if (ImGui::BeginTabItem(Docs[static_cast<size_t>(i)].TypeName.c_str(), &open, flags))
            {
                ActiveTab = i;
                ImGui::EndTabItem();
            }
            if (!open)
            {
                Docs.erase(Docs.begin() + i);
                if (ActiveTab >= static_cast<int>(Docs.size())) ActiveTab = static_cast<int>(Docs.size()) - 1;
                break;
            }
        }
        ImGui::EndTabBar();
    }
}

void ConstructEditorWindow::DrawCompositionPanel(ConstructDoc& doc, float height)
{
    ImGui::BeginChild("##ConstructComp", ImVec2(180.0f, height), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX);

    ImGui::TextDisabled("Views");
    ImGui::SameLine();
    if (ImGui::SmallButton("+##AddView"))
        doc.Views.push_back({});
    ImGui::Separator();

    for (int i = 0; i < static_cast<int>(doc.Views.size()); ++i)
    {
        auto& v = doc.Views[static_cast<size_t>(i)];
        ImGui::PushID(i);
        ImGui::SetNextItemWidth(80.0f);
        ImGui::InputText("##VType", v.TypeName, sizeof(v.TypeName));
        ImGui::SameLine();
        ImGui::SetNextItemWidth(60.0f);
        ImGui::InputText("##VName", v.MemberName, sizeof(v.MemberName));
        ImGui::SameLine();
        if (ImGui::SmallButton("x")) { doc.Views.erase(doc.Views.begin() + i); ImGui::PopID(); break; }
        ImGui::PopID();
    }

    ImGui::Spacing();
    ImGui::TextDisabled("Owned<T>");
    ImGui::SameLine();
    if (ImGui::SmallButton("+##AddOwned"))
        doc.Owned.push_back({});
    ImGui::Separator();

    for (int i = 0; i < static_cast<int>(doc.Owned.size()); ++i)
    {
        auto& o = doc.Owned[static_cast<size_t>(i)];
        ImGui::PushID(1000 + i);
        ImGui::SetNextItemWidth(80.0f);
        ImGui::InputText("##OType", o.TypeName, sizeof(o.TypeName));
        ImGui::SameLine();
        ImGui::SetNextItemWidth(60.0f);
        ImGui::InputText("##OName", o.MemberName, sizeof(o.MemberName));
        ImGui::SameLine();
        if (ImGui::SmallButton("x")) { doc.Owned.erase(doc.Owned.begin() + i); ImGui::PopID(); break; }
        ImGui::PopID();
    }

    ImGui::EndChild();
}

void ConstructEditorWindow::DrawSlotTabs(ConstructDoc& doc, float /*width*/, float height)
{
    ImGui::BeginChild("##ConstructCenter", ImVec2(0.0f, height), ImGuiChildFlags_ResizeX,
                      ImGuiWindowFlags_NoScrollbar);

    if (ImGui::BeginTabBar("##SlotTabs"))
    {
        for (int s = 0; s < static_cast<int>(ConstructDoc::TickSlot::Count); ++s)
        {
            if (ImGui::BeginTabItem(SlotNames[s]))
            {
                doc.ActiveSlot = s;
                ImGui::EndTabItem();
            }
        }
        ImGui::EndTabBar();
    }

    float graphH = ImGui::GetContentRegionAvail().y;
    float graphW = ImGui::GetContentRegionAvail().x;

    // Unique per-doc-per-slot ID to prevent ImGui widget collisions
    char canvasID[64];
    snprintf(canvasID, sizeof(canvasID), "ce_%p_%d", (void*)&doc, doc.ActiveSlot);

    doc.Graphs[doc.ActiveSlot].DrawCanvas(canvasID, graphW, graphH);

    ImGui::EndChild();
}

void ConstructEditorWindow::DrawInfoPanel(ConstructDoc& doc, float height, EditorState& state)
{
    ImGui::BeginChild("##ConstructInfo", ImVec2(0.0f, height), ImGuiChildFlags_Borders);

    ImGui::TextDisabled("Type");
    ImGui::TextUnformatted(doc.TypeName.c_str());
    ImGui::Spacing();

    ImGui::TextDisabled("Output Path");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputText("##OutPathCE", doc.OutputPath, sizeof(doc.OutputPath));

    ImGui::Spacing();
    if (ImGui::Button("Export .h##CEExp", ImVec2(-1.0f, 0.0f)))
    {
        // TODO: code generation from node graphs
        snprintf(doc.StatusMsg, sizeof(doc.StatusMsg), "Code gen not yet implemented.");
        doc.bStatusError = true;
    }

    if (doc.StatusMsg[0] != '\0')
    {
        ImVec4 col = doc.bStatusError ? ImVec4(1.0f, 0.35f, 0.35f, 1.0f) : ImVec4(0.35f, 1.0f, 0.35f, 1.0f);
        ImGui::TextColored(col, "%s", doc.StatusMsg);
    }

    ImGui::Spacing();
    ImGui::TextDisabled("Tick slots");
    for (int s = 0; s < static_cast<int>(ConstructDoc::TickSlot::Count); ++s)
    {
        int nodeCount = static_cast<int>(doc.Graphs[s].Nodes.size());
        ImGui::Text("  %s: %d node%s", SlotNames[s], nodeCount, nodeCount == 1 ? "" : "s");
    }

    ImGui::EndChild();
}
