# PANELS — World Outliner + Inspector

These two panels carry the corrected data model. Get them right first;
everything else flows from there.

## World Outliner — TWO roots

The outliner has **two distinct rooted sections** because the engine has
two distinct kinds of object. They're sorted differently, opened
differently, right-clicked differently. Don't try to unify them.

```
CONSTRUCTS  (7)
  ▾ GameMode_Tnx                                ArenaMode
  ▾ Player                                      Player
    · Body              ConstructView<EPlayer>  idx 0x00A4F2   [selected]
    · JoltChar          JoltCharacter
  ▸ Camera_FP                                   CameraConstruct
  ▸ Camera_TP                                   CameraConstruct
    Sun                                         DirLightConstruct
    FillRim                                     DirLightConstruct
    Skybox                                      SkyConstruct

ARCHETYPES  (4)
  ▸ EPlayer                                     × 1        1 chunk
  ▾ ECubeStatic                                 × 5,525    87 chunks
    ▾ chunk 00   64/64   [COLD]
      e 0x00B100  Pyramid_25 · row 0
      e 0x00B101  Pyramid_25 · row 0
      e 0x00B102  Pyramid_25 · row 0
      + 61 more
    ▸ chunk 01   64/64   [COLD]
    ▸ chunk 86   27/64   [COLD]
  ▸ ECubeAmb                                    × 199,600  3,119 chunks
  ▸ EParticle                                   × 12,840   201 chunks
```

### Drawing pattern

```cpp
void DrawWorldOutliner()
{
    if (!ImGui::Begin("World Outliner")) { ImGui::End(); return; }
    TnxWidgets::PanelHeader("\xee\x80\x80", "World Outliner", []{
        // right side: count chips
        TnxWidgets::Chip("7 cnst");
        ImGui::SameLine();
        TnxWidgets::Chip("4 arch");
        ImGui::SameLine();
        TnxWidgets::Chip("217k ent");
    });

    // Filter row
    DrawFilterRow();

    // Root 1 — Constructs
    DrawOutlinerRoot("CONSTRUCTS", TnxStyle::Color::PurpleHot,
                     ConstructRegistry::Get().Roots());

    // Root 2 — Archetypes
    DrawOutlinerRoot("ARCHETYPES", TnxStyle::Color::TierVolatile,
                     EngineState.Registry.AllArchetypes());

    ImGui::End();
}

static void DrawOutlinerRoot(const char* label, ImVec4 color, auto&& children)
{
    ImGui::PushStyleColor(ImGuiCol_Header, TnxStyle::Color::BgApp);
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, TnxStyle::Color::BgElev);
    ImGui::PushStyleColor(ImGuiCol_HeaderActive,  TnxStyle::Color::BgElev);
    ImGui::PushStyleColor(ImGuiCol_Text, TnxStyle::Color::FgMuted);

    bool open = ImGui::CollapsingHeader(label, ImGuiTreeNodeFlags_DefaultOpen);
    ImGui::PopStyleColor(4);

    if (open) {
        for (auto& child : children) {
            // Construct: cube icon (purple-hot when selected)
            // Archetype: grid icon (volatile-tier color)
            // Chunk: layers icon (muted)
            // Entity leaf: dot icon (dim), name in JetBrains Mono, cache index hex
            DrawOutlinerItem(child);
        }
    }
}
```

### Selection

```cpp
struct OutlinerSelection {
    enum Kind { None, Construct, Archetype, Chunk, Entity } kind;
    union {
        ConstructHandle      cnst;
        ArchetypeID          arch;
        struct { ArchetypeID a; uint32_t chunk; }   chunk;
        struct { ArchetypeID a; EntityCacheIndex e; } entity;
    };
};
```

Selecting an entity drives the Inspector to entity mode. Selecting an
Archetype shows archetype-level info (DebugName, ClassID, entity count,
component list — same as today). Selecting a Construct shows the
construct's owned Views + bespoke logic toggles.

### Right-click menus

- **Construct** → Rename / Duplicate / Destroy / Reveal in Asset Browser
- **Archetype** → Inspect schema / Defragment / Dump to JSON
- **Chunk** → Show on Slab Heatmap / Dump bytes
- **Entity** → Inspect / Destroy / Track in viewport / Copy CacheIndex

## Inspector — three layers, three badge tiers

The big change from before. Reading the inspector top-down should
narrow your understanding of where you are:

```
┌──────────────────────────────────────────────────────────────┐
│  ☑ Player.Body                                          🔒   │
│  [EView<EPlayer>]  [idx 0x00A4F2]  [queue: Physics]          │  ← ENTITY level
│  [arch EPlayer · chunk 0/4]                                  │      queue tag here
├──────────────────────────────────────────────────────────────┤
│  ▾ CTransform   [TEMPORAL]  [net]                       ×    │  ← COMPONENT level
│       Position   X 12.483   Y 0.000   Z -4.220               │      tier + net here
│       Rotation   X 0.00     Y 182.50  Z 0.00                 │
│       Scale      X 1.00     Y 1.00    Z 1.00                 │
├──────────────────────────────────────────────────────────────┤
│  ▾ JoltCharacter   [VOLATILE]                          ×     │
│       Capsule R   0.40  m                                    │  ← FIELDS — no badges
│       Capsule H   1.80  m                                    │      just value
│       Move Spd    5.50  m/s                                  │
│       Grounded    true                                       │
├──────────────────────────────────────────────────────────────┤
│  ▾ CHealth   [TEMPORAL]  [net]                         ×     │
│       Value      100   / 100                                 │
│       Regen      2.50  /s                                    │
├──────────────────────────────────────────────────────────────┤
│  [+ Add Component …  Cmd+Shift+A]                            │
└──────────────────────────────────────────────────────────────┘
```

### Drawing pattern

```cpp
void DrawInspector(const OutlinerSelection& sel)
{
    if (!ImGui::Begin("Inspector")) { ImGui::End(); return; }
    TnxWidgets::PanelHeader("\xee\x80\x82", "Inspector");

    if (sel.kind != OutlinerSelection::Entity) {
        DrawNonEntityInspector(sel);  // archetype / construct mode
        ImGui::End();
        return;
    }

    Entity e = ResolveEntity(sel.entity.a, sel.entity.e);

    DrawEntityHeader(e);

    for (const ComponentRef& comp : e.Components()) {
        if (DrawComponentHeader(comp)) {
            DrawComponentFields(comp);
        }
    }
    DrawAddComponentBar();
    ImGui::End();
}

void DrawEntityHeader(const Entity& e)
{
    ImGui::Checkbox("##active", &e.active);  // visibility
    ImGui::SameLine();
    ImGui::PushFont(TnxStyle::Font::DisplaySemibold);
    ImGui::Text("%s", e.DisplayName());  // e.g. "Player.Body"
    ImGui::PopFont();

    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 22);
    if (TnxWidgets::ButtonIconGhost("\xee\x80\x90")) e.locked = !e.locked;  // lock

    // Badge row — entity-level
    TnxWidgets::Chip(Format("%s<%s>", e.ViewKind(), e.SchemaName()));
    ImGui::SameLine();
    TnxWidgets::Chip(Format("idx 0x%06X", e.CacheIndex()));
    ImGui::SameLine();
    TnxWidgets::Chip(Format("queue: %s", QueueName(e.PartitionQueue())),
                     TnxWidgets::ChipStyle::Purple);
    ImGui::SameLine();
    TnxWidgets::Chip(Format("arch %s · chunk %d/%d",
                            e.ArchetypeName(), e.ChunkIndex(), e.ArchetypeChunks()));
}

bool DrawComponentHeader(const ComponentRef& c)
{
    using namespace TnxStyle::Color;
    ImGui::PushStyleColor(ImGuiCol_Header, BgApp);
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, BgElev);
    bool open = ImGui::CollapsingHeader(c.Name(), ImGuiTreeNodeFlags_DefaultOpen);
    ImGui::PopStyleColor(2);

    // Header decorations — drawn AFTER the CollapsingHeader, on the same line.
    ImGui::SameLine();
    TnxWidgets::TierBadge(c.Tier());        // [TEMPORAL]
    if (c.IsReplicated()) {
        ImGui::SameLine();
        TnxWidgets::Chip("net", TnxWidgets::ChipStyle::Yellow);
    }
    return open;
}
```

### Field rows — clean

No badges on fields. The component header tells you everything about
how this whole block of values is stored.

```cpp
void DrawComponentFields(const ComponentRef& c)
{
    ImGui::Indent(8.0f);
    for (const FieldRef& f : c.Fields()) {
        switch (f.Kind()) {
            case FieldKind::Vec3:   TnxWidgets::FieldVec3 (f.Label(), f.Vec3Ptr());           break;
            case FieldKind::Float:  TnxWidgets::FieldFloat(f.Label(), f.FloatPtr(), f.Unit()); break;
            case FieldKind::Bool:   TnxWidgets::FieldBool (f.Label(), f.BoolPtr());            break;
            case FieldKind::AssetRef: TnxWidgets::FieldAssetRef(f.Label(), f.AssetPtr(), f.AssetType()); break;
        }
    }
    ImGui::Unindent(8.0f);
}
```

## Mode switches

The Inspector has three modes — wire them via your `EditorState::SelectionType`:

| `SelectionType` | What renders |
|---|---|
| `None` | "Nothing selected" empty state with a `⌘K` hint |
| `Construct` | Owned views list, bespoke fields, RPC test buttons |
| `Archetype` | DebugName, ClassID, total entity count, chunks, component list (read-only) |
| `Entity` | The full three-layer inspector above |

## Empty states matter

Don't ship a panel that's blank when nothing's selected. The empty state
is a teaching surface. Inspector empty state should say:

```
 ⌘  Nothing selected.
    Click an entity in the World Outliner, drag in the viewport,
    or press ⌘K to find one by name.
```

Same treatment for Content Browser when project is empty, Node Script
when no graph is open, etc.

## What stays the same

The existing **Engine Stats**, **Log**, **Content Browser**, **Node
Script**, **Component Generator**, and **Debugger** panels keep their
current data sources and most of their content. They only need:

1. The new color tokens applied (free via `Apply()`).
2. Headers swapped to `TnxWidgets::PanelHeader`.
3. Any tier / queue references rebadged using the corrected model.
