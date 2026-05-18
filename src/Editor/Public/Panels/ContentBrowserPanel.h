#pragma once
#include "AssetTypes.h"
#include "EditorPanel.h"
#include <filesystem>
#include <string>

struct AssetDatabaseEntry;

class ContentBrowserPanel : public EditorPanel
{
public:
    ContentBrowserPanel() : EditorPanel("Content Browser") {}
    void Draw(EditorState& state) override;

private:
    void DrawToolbar(EditorState& state);
    void DrawFolderPane(EditorState& state);
    void DrawFolderNode(const std::filesystem::path& dir, int depth);
    void DrawAssetPane(EditorState& state);
    void DrawAssetTile(EditorState& state, const AssetDatabaseEntry& entry);
    void DrawAssetList(EditorState& state);
    void DrawAssetRow(EditorState& state, const AssetDatabaseEntry& entry);

    void DrawAssetContextMenu(EditorState& state, const AssetDatabaseEntry& entry);
    void DrawBlankContextMenu(EditorState& state);

    void OpenCreatePopup(bool forFolder, AssetType type = AssetType::Invalid);
    void DrawCreatePopup(EditorState& state);
    void CommitCreate(EditorState& state);

    bool MatchesFilter(const AssetDatabaseEntry& entry) const;

    // Cached per Draw() call
    std::string ContentRoot;

    // Navigation
    std::string CurrentFolder; // relative to content root, forward-slash separated

    // Selection
    AssetID SelectedAsset;

    // View
    enum class ViewMode : uint8_t { Icons, List };
    ViewMode CurrentViewMode = ViewMode::Icons;
    float ThumbnailSize      = 72.0f;

    // Filter / search
    int  TypeFilter = 0;
    char SearchBuf[128] = {};

    // Rename modal
    bool    bShowRename  = false;
    AssetID RenamingID;
    char    RenameBuf[256] = {};

    // Create modal
    bool      bShowCreate     = false;
    bool      bCreateIsFolder = false;
    AssetType CreateType      = AssetType::Invalid;
    char      CreateNameBuf[256] = {};
};