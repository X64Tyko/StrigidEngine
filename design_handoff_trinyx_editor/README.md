# Trinyx Editor — Design Handoff

> Implementation target: Dear ImGui (docking branch), C++20, MSVC/GCC/Clang.
> Engine: TrinyxEngine. Editor runs on the Encoder thread, operates directly
> on live ECS slab data.

## What this bundle is

A spec for rebuilding the existing ImGui editor's **look + structure** to match
the design mockups produced in chat. The mockups themselves are in `reference/`
as plain HTML so you can open them locally and pixel-match. **They are
references, not code to ship** — recreate them with ImGui widgets.

## Fidelity

**High-fidelity** for v1. Pixel-match the dark theme, the royal-purple primary
and warm-yellow accent, the partition tier badges, and the panel chrome from
the mockups. Lower-fi for the "novel ideas" — those should follow the spirit
without obsessing over the exact pixels.

## Read order

1. **`IMGUI_FEASIBILITY.md`** — what's doable, what addons you need, what's
   not worth fighting
2. **`STYLE.md`** — drop-in `ApplyTrinyxStyle()` plus the color/font tables
3. **`WIDGETS.md`** — implementation patterns for the custom atoms (tier
   badge, chip, kbd, frame-budget bar, command palette, etc.)
4. **`PANELS.md`** — the World Outliner and Inspector — corrected data model
   (Constructs + Archetypes/Chunks/Entities; tier+net on components; queue on
   entity)
5. **`NOVEL.md`** — workspace switcher, multi-window PIE + condition
   simulator, slab heatmap, node script, job graph
6. **`ROADMAP.md`** — phased rollout — what to ship before / after PIE work

## Important model corrections from prior round

Three things changed since the engine's current bare-bones editor:

1. **World Outliner has two roots, not one.** Constructs (OOP, named,
   addressable) are one root; Archetypes → Chunks → Entities (cache index) are
   another. Different icons, different sort behavior, different right-click
   menus. See `PANELS.md`.

2. **Badges live at the layer they describe:**
   - **Entity** owns the **queue** tag (`queue: Physics / Logic / Render /
     General`) — appears in the inspector's selection header.
   - **Component** owns the **tier** badge (Cold / Static / Volatile /
     Temporal) and the **net** flag — appears in each component's collapsing
     header.
   - **Field** is just a value. No badges, no metadata. They inherit from the
     component.

3. **PIE is multi-window** — Authority + N Owner viewports each in their own
   `WorldViewport`. The Network workspace tiles them; the Condition Simulator
   panel sits alongside. ImGui docking branch supports this natively.

## Files in this bundle

```
README.md                 ← this file
IMGUI_FEASIBILITY.md      ← capability matrix + required addons
STYLE.md                  ← style code, color table, fonts
WIDGETS.md                ← custom widget patterns
PANELS.md                 ← World Outliner + Inspector specs
NOVEL.md                  ← workspace switcher + new features
ROADMAP.md                ← build order

reference/
  Trinyx Editor.html      ← open this in a browser, fullscreen the artboards
  styles.css              ← design tokens (oklch values) — port to ImVec4 via STYLE.md
  components.jsx          ← icon set, viewport, wordmark — informational
  foundation.jsx
  wireframes.jsx
  hifi.jsx
  ideas.jsx
  ideas2.jsx
  design-canvas.jsx
```

## Asks for the developer

- **Match the design tokens exactly.** Colors, font sizes, spacing values
  are all spec'd in `STYLE.md`. Don't eyeball — the oklch → sRGB conversions
  are pre-computed.
- **Reuse existing panels.** The engine already has `World Outliner`,
  `Details`, `Content Browser`, `Engine Stats`, `Log`, `Node Script`,
  `Component Generator`, `Debugger`. Restyle and reorganize, don't rebuild
  unless the IA forces it.
- **Workspace switcher comes first.** Per the design lock-in, the top-bar
  workspace pills (Layout / Logic / Simulate / Network / Profile) are how
  the user navigates between dock layouts. Build that, save dock state per
  workspace, ship it before the rest.
- **Ask before adding content.** Don't invent stats, badges, or affordances
  that aren't in the mocks. If a panel needs something not spec'd, surface
  the question.

## Build verification checklist

A successful v1 lands when:

- [ ] `ApplyTrinyxStyle()` matches the dark-mode color table in `STYLE.md` ±2/255 on every channel
- [ ] Manrope + Space Grotesk + JetBrains Mono load via FreeType, anti-aliased
- [ ] World Outliner shows Constructs and Archetypes as two distinct rooted sections
- [ ] Inspector shows entity-level queue tag + component-level tier/net + clean fields
- [ ] Workspace pills swap dock layouts; layout per workspace is persisted to `EditorSettings.ini`
- [ ] Persistent thread-budget strip is visible from every workspace (anchored to a status dock)
- [ ] ⌘K opens the command palette overlay; ESC dismisses; ↑↓ navigates; ↵ executes top result
