/* global React, Icon, Wordmark, LogoMark, Tier, Spark, Viewport */

// Hi-fi editor screen — full 1920×1080 polish. Synthesizes the strongest
// pieces of the wireframes: classic familiar 4-panel anchor + workspace
// switcher pills along the top + a Trinyx-specific bottom dock that
// surfaces the three threads, slab, and frame budget at all times.

// ── Top bar — global chrome ──────────────────────────────────────────────
const TopBar = () => (
  <div style={{
    height: 44, background: "var(--bg-deep)", borderBottom: "1px solid var(--border)",
    display: "flex", alignItems: "center", padding: "0 12px", gap: 14, flexShrink: 0,
  }}>
    <div style={{ display: "flex", alignItems: "center", gap: 10 }}>
      <LogoMark size={26}/>
      <div style={{ display: "flex", flexDirection: "column", lineHeight: 1.1 }}>
        <div style={{ fontFamily: "var(--f-display)", fontWeight: 600, fontSize: 13 }}>Testbed</div>
        <div className="mono" style={{ fontSize: 9.5, color: "var(--fg-dim)" }}>main · RelWithDebInfo</div>
      </div>
    </div>

    <div style={{ width: 1, height: 22, background: "var(--border)" }}/>

    {/* Workspace pills */}
    <div style={{ display: "flex", gap: 2, background: "var(--bg-panel)",
                  border: "1px solid var(--border)", borderRadius: 3, padding: 2 }}>
      {[
        ["Layout",   true],
        ["Logic",    false],
        ["Simulate", false],
        ["Network",  false],
        ["Profile",  false],
      ].map(([n, active]) => (
        <div key={n} style={{
          padding: "4px 10px", borderRadius: 2, fontSize: 11.5, fontWeight: 600,
          color: active ? "white" : "var(--fg-muted)",
          background: active ? "var(--purple)" : "transparent",
          cursor: "pointer", userSelect: "none",
        }}>{n}</div>
      ))}
    </div>

    {/* PIE controls — centered group */}
    <div style={{ display: "flex", gap: 4, marginLeft: 16 }}>
      <button className="btn btn-yellow" style={{ height: 28, padding: "0 14px" }}>
        <Icon name="play" size={12} color="oklch(0.20 0.02 90)"/>
        Play
      </button>
      <button className="btn" style={{ height: 28 }}>
        <Icon name="pause" size={12}/>
      </button>
      <button className="btn" style={{ height: 28 }}>
        <Icon name="step" size={12}/>
        Step
      </button>
      <button className="btn" style={{ height: 28 }}>
        <Icon name="stop" size={12}/>
      </button>
      <button className="btn btn-ghost" style={{ height: 28, marginLeft: 4 }}>
        <Icon name="network" size={12}/>
        <span className="mono" style={{ fontSize: 10.5 }}>STANDALONE</span>
        <Icon name="chevD" size={10}/>
      </button>
    </div>

    {/* Search */}
    <div style={{ marginLeft: "auto", display: "flex", alignItems: "center", gap: 8,
                  background: "var(--bg-input)", border: "1px solid var(--border)",
                  borderRadius: 3, padding: "0 10px", width: 280, height: 28 }}>
      <Icon name="search" size={12} color="var(--fg-dim)"/>
      <span style={{ color: "var(--fg-dim)", fontSize: 12 }}>Run command, find entity…</span>
      <span style={{ marginLeft: "auto", display: "flex", gap: 2 }}>
        <span className="kbd">⌘</span><span className="kbd">K</span>
      </span>
    </div>

    {/* User / settings */}
    <div style={{ display: "flex", gap: 4 }}>
      <button className="btn btn-icon btn-ghost"><Icon name="bug" size={14}/></button>
      <button className="btn btn-icon btn-ghost"><Icon name="gear" size={14}/></button>
    </div>
  </div>
);

// ── Toolbar — gizmo modes + view options ─────────────────────────────────
const Toolbar = () => (
  <div style={{
    height: 34, background: "var(--bg-app)", borderBottom: "1px solid var(--border-soft)",
    display: "flex", alignItems: "center", padding: "0 10px", gap: 8, flexShrink: 0,
  }}>
    {/* Gizmo modes */}
    <div style={{ display: "flex", gap: 1, background: "var(--bg-panel)",
                  border: "1px solid var(--border)", borderRadius: 3, padding: 1 }}>
      {[
        ["select", true],
        ["move", false],
        ["rotate", false],
        ["scale", false],
      ].map(([n, active]) => (
        <div key={n} style={{
          width: 28, height: 24, display: "grid", placeItems: "center",
          background: active ? "var(--purple)" : "transparent",
          color: active ? "white" : "var(--fg-muted)",
          borderRadius: 2, cursor: "pointer"
        }}>
          <Icon name={n} size={13}/>
        </div>
      ))}
    </div>

    <div className="vdivider" style={{ height: 18 }}/>

    {/* Pivot + Space */}
    <button className="btn btn-sm btn-ghost">Pivot · <span className="mono" style={{ color: "var(--fg)" }}>Center</span></button>
    <button className="btn btn-sm btn-ghost">Space · <span className="mono" style={{ color: "var(--fg)" }}>World</span></button>

    <div className="vdivider" style={{ height: 18 }}/>

    {/* Snap */}
    <button className="btn btn-sm">
      <Icon name="grid" size={12}/>
      <span>Snap</span>
      <span className="mono" style={{ color: "var(--fg-muted)" }}>0.25</span>
    </button>

    <div className="vdivider" style={{ height: 18 }}/>

    {/* View toggles */}
    <button className="btn btn-sm btn-ghost"><Icon name="eye" size={12}/>Wireframe</button>
    <button className="btn btn-sm btn-ghost"><Icon name="layers" size={12}/>Partitions</button>
    <button className="btn btn-sm btn-ghost"><Icon name="cpu" size={12}/>Thread</button>

    <div style={{ marginLeft: "auto", display: "flex", gap: 10, alignItems: "center" }}>
      <span className="chip"><Icon name="cube" size={10}/>205,234 ent</span>
      <span className="chip chip-good"><Icon name="dot" size={8}/>Slab healthy</span>
      <span className="chip chip-yellow"><Icon name="spark" size={10}/>3 dirty bitplanes</span>
    </div>
  </div>
);

// ── World Outliner — Constructs and Archetypes are two distinct roots ──
// Constructs (OOP layer — Player, GameMode, Camera) are unique, named,
// addressable objects. Archetypes (ECS bulks) collapse to chunk and entity
// leaves; entities surface by CacheIndex. Matches the engine's mental model
// of "two kinds of object, one slab underneath".
const Hierarchy = () => {
  const items = [
    // ── Constructs root ────────────────────────────────────────────────
    { kind: "root-construct", name: "Constructs",   indent: 0, chev: "▾", count: 7 },
    { kind: "construct",      name: "GameMode_Tnx", indent: 1, chev: "▸", rtti: "ArenaMode" },
    { kind: "construct",      name: "Player",       indent: 1, chev: "▾", rtti: "Player" },
    { kind: "view",           name: "Body",         indent: 2, view: "ConstructView<EPlayer>", idx: "0x00A4F2", selected: true },
    { kind: "view",           name: "JoltChar",     indent: 2, view: "JoltCharacter" },
    { kind: "construct",      name: "Camera_FP",    indent: 2, chev: "▸", rtti: "CameraConstruct" },
    { kind: "construct",      name: "Camera_TP",    indent: 2, chev: "▸", rtti: "CameraConstruct" },
    { kind: "construct",      name: "Sun",          indent: 1, rtti: "DirLightConstruct" },
    { kind: "construct",      name: "FillRim",      indent: 1, rtti: "DirLightConstruct" },
    { kind: "construct",      name: "Skybox",       indent: 1, rtti: "SkyConstruct" },

    // ── Archetypes root ────────────────────────────────────────────────
    { kind: "root-arch", name: "Archetypes", indent: 0, chev: "▾", count: 4 },
    { kind: "arch",      name: "EPlayer",     indent: 1, chev: "▸", total: 1,       chunks: 1 },
    { kind: "arch",      name: "ECubeStatic", indent: 1, chev: "▾", total: 5525,    chunks: 87 },
    { kind: "chunk",     name: "chunk 00",    indent: 2, chev: "▾", fill: "64/64",  tag: "Cold" },
    { kind: "ent",       name: "e 0x00B100",  indent: 3, label: "Pyramid_25 · row 0" },
    { kind: "ent",       name: "e 0x00B101",  indent: 3, label: "Pyramid_25 · row 0" },
    { kind: "ent",       name: "e 0x00B102",  indent: 3, label: "Pyramid_25 · row 0" },
    { kind: "more",      name: "+ 61 more",   indent: 3 },
    { kind: "chunk",     name: "chunk 01",    indent: 2, chev: "▸", fill: "64/64",  tag: "Cold" },
    { kind: "chunk",     name: "chunk 86",    indent: 2, chev: "▸", fill: "27/64",  tag: "Cold" },
    { kind: "arch",      name: "ECubeAmb",    indent: 1, chev: "▸", total: 199600,  chunks: 3119 },
    { kind: "arch",      name: "EParticle",   indent: 1, chev: "▸", total: 12840,   chunks: 201 },
  ];
  return (
    <div className="panel" style={{ width: 268, height: "100%" }}>
      <div className="panel-head">
        <Icon name="layers" size={12} color="var(--fg-muted)"/>
        <span className="title">World Outliner</span>
        <span className="mono" style={{ marginLeft: "auto", fontSize: 10, color: "var(--fg-dim)" }}>
          7 cnst · 4 arch · 217k ent
        </span>
        <button className="btn btn-icon btn-ghost btn-sm"><Icon name="plus" size={11}/></button>
      </div>
      <div style={{ padding: "6px 6px 6px 6px",
                    display: "flex", alignItems: "center", gap: 6,
                    borderBottom: "1px solid var(--border-soft)" }}>
        <div style={{ display: "flex", alignItems: "center", gap: 6, flex: 1,
                      background: "var(--bg-input)", border: "1px solid var(--border-soft)",
                      borderRadius: 2, padding: "0 8px", height: 22 }}>
          <Icon name="search" size={10} color="var(--fg-dim)"/>
          <span style={{ color: "var(--fg-dim)", fontSize: 11 }}>filter Constructs/Archetypes…</span>
        </div>
      </div>
      <div className="panel-body" style={{ overflow: "auto" }}>
        <div style={{ padding: "2px 0", fontSize: 12 }}>
          {items.map((it, i) => {
            const isRoot = it.kind.startsWith("root-");
            const iconName =
              it.kind === "root-construct" ? "cube" :
              it.kind === "root-arch"      ? "grid" :
              it.kind === "construct"      ? "cube" :
              it.kind === "view"           ? "flask" :
              it.kind === "arch"           ? "grid" :
              it.kind === "chunk"          ? "layers" :
              it.kind === "ent"            ? "dot" :
              "chevR";
            const iconColor =
              it.selected               ? "var(--yellow)" :
              it.kind === "root-construct" ? "var(--purple-hot)" :
              it.kind === "root-arch"      ? "var(--tier-volatile)" :
              it.kind === "construct"      ? "var(--purple-hot)" :
              it.kind === "view"           ? "var(--yellow-soft)" :
              it.kind === "arch"           ? "var(--tier-volatile)" :
              it.kind === "chunk"          ? "var(--fg-muted)" :
              it.kind === "ent"            ? "var(--fg-dim)" :
              "var(--fg-dim)";

            if (it.kind === "more") {
              return (
                <div key={i} style={{
                  padding: `2px 8px 2px ${8 + it.indent * 14}px`, height: 20,
                  color: "var(--fg-dim)", fontSize: 11, fontStyle: "italic",
                }}>{it.name}</div>
              );
            }

            return (
              <div key={i} style={{
                display: "flex", alignItems: "center", gap: 4,
                padding: `2px 8px 2px ${8 + it.indent * 14}px`, height: 22,
                background: it.selected ? "var(--purple-wash)" :
                            isRoot ? "var(--bg-app)" : "transparent",
                borderLeft: it.selected ? "2px solid var(--purple)" : "2px solid transparent",
                borderBottom: isRoot ? "1px solid var(--border-soft)" : "none",
                cursor: "default",
                fontFamily: it.kind === "ent" || it.kind === "chunk" ? "var(--f-mono)" : "var(--f-ui)",
                fontSize: it.kind === "ent" ? 11 : 12,
              }}>
                <span style={{ color: "var(--fg-dim)", width: 10, fontSize: 9 }}>{it.chev || ""}</span>
                <Icon name={iconName} size={it.kind === "ent" ? 8 : 12} color={iconColor}/>
                <span style={{
                  color: isRoot ? "var(--fg-muted)" : "var(--fg)",
                  fontWeight: isRoot ? 600 : it.selected ? 600 : 400,
                  textTransform: isRoot ? "uppercase" : "none",
                  letterSpacing: isRoot ? "0.08em" : 0,
                  fontSize: isRoot ? 10.5 : "inherit",
                  flex: 1, overflow: "hidden", textOverflow: "ellipsis", whiteSpace: "nowrap"
                }}>{it.name}</span>
                {it.rtti && <span className="mono" style={{ fontSize: 10, color: "var(--fg-dim)" }}>{it.rtti}</span>}
                {it.view && <span className="mono" style={{ fontSize: 9.5, color: "var(--fg-dim)" }}>{it.view}</span>}
                {it.label && <span style={{ fontSize: 10, color: "var(--fg-dim)" }}>{it.label}</span>}
                {it.tag && <Tier kind={it.tag}/>}
                {it.fill && <span className="mono" style={{ fontSize: 10, color: "var(--fg-dim)" }}>{it.fill}</span>}
                {it.total !== undefined && (
                  <span className="mono" style={{ fontSize: 10, color: "var(--fg-dim)" }}>
                    ×{it.total.toLocaleString()}
                  </span>
                )}
                {it.count !== undefined && (
                  <span className="mono" style={{ fontSize: 10, color: "var(--fg-dim)" }}>
                    {it.count}
                  </span>
                )}
              </div>
            );
          })}
        </div>
      </div>
    </div>
  );
};

// ── Inspector ────────────────────────────────────────────────────────────
const Inspector = () => (
  <div className="panel" style={{ width: 340, height: "100%" }}>
    <div className="panel-head">
      <Icon name="cube" size={12} color="var(--yellow)"/>
      <span className="title">Inspector</span>
    </div>

    {/* Selection header — entity-level: queue tag lives here */}
    <div style={{ padding: "10px 12px", borderBottom: "1px solid var(--border-soft)" }}>
      <div style={{ display: "flex", alignItems: "center", gap: 8, marginBottom: 4 }}>
        <input type="checkbox" defaultChecked style={{ accentColor: "var(--purple)" }}/>
        <div style={{ fontFamily: "var(--f-display)", fontSize: 16, fontWeight: 600 }}>Player.Body</div>
        <button className="btn btn-icon btn-sm btn-ghost" style={{ marginLeft: "auto" }}><Icon name="lock" size={11}/></button>
      </div>
      <div style={{ display: "flex", gap: 6, alignItems: "center", flexWrap: "wrap" }}>
        <span className="chip mono">EView&lt;EPlayer&gt;</span>
        <span className="chip mono" style={{ color: "var(--fg-muted)" }}>idx 0x00A4F2</span>
        <span className="chip chip-purple mono">queue: Physics</span>
        <span className="chip mono" style={{ color: "var(--fg-muted)" }}>arch EPlayer · chunk 0/4</span>
      </div>
    </div>

    <div className="panel-body" style={{ overflow: "auto" }}>
      {/* Transform — tier + net live on the component, fields are clean */}
      <ComponentBlock name="CTransform" tier="Temporal" net>
        <div className="field-row">
          <div className="lbl">Position</div>
          <div className="vec3">
            <div className="field-input"><span className="axis axis-x">X</span><span className="val mono">12.483</span></div>
            <div className="field-input"><span className="axis axis-y">Y</span><span className="val mono">0.000</span></div>
            <div className="field-input"><span className="axis axis-z">Z</span><span className="val mono">−4.220</span></div>
          </div>
        </div>
        <div className="field-row">
          <div className="lbl">Rotation</div>
          <div className="vec3">
            <div className="field-input"><span className="axis axis-x">X</span><span className="val mono">0.00</span></div>
            <div className="field-input"><span className="axis axis-y">Y</span><span className="val mono">182.50</span></div>
            <div className="field-input"><span className="axis axis-z">Z</span><span className="val mono">0.00</span></div>
          </div>
        </div>
        <div className="field-row">
          <div className="lbl">Scale</div>
          <div className="vec3">
            <div className="field-input"><span className="axis axis-x">X</span><span className="val mono">1.00</span></div>
            <div className="field-input"><span className="axis axis-y">Y</span><span className="val mono">1.00</span></div>
            <div className="field-input"><span className="axis axis-z">Z</span><span className="val mono">1.00</span></div>
          </div>
        </div>
      </ComponentBlock>

      {/* JoltCharacter — Volatile (no rollback), not networked */}
      <ComponentBlock name="JoltCharacter" tier="Volatile">
        <div className="field-row">
          <div className="lbl">Capsule R</div>
          <div className="field-input"><span className="val mono">0.40</span>
            <span className="mono" style={{ marginLeft: "auto", color: "var(--fg-dim)", fontSize: 10 }}>m</span></div>
        </div>
        <div className="field-row">
          <div className="lbl">Capsule H</div>
          <div className="field-input"><span className="val mono">1.80</span>
            <span className="mono" style={{ marginLeft: "auto", color: "var(--fg-dim)", fontSize: 10 }}>m</span></div>
        </div>
        <div className="field-row">
          <div className="lbl">Move Spd</div>
          <div className="field-input"><span className="val mono">5.50</span>
            <span className="mono" style={{ marginLeft: "auto", color: "var(--fg-dim)", fontSize: 10 }}>m/s</span></div>
        </div>
        <div className="field-row">
          <div className="lbl">Grounded</div>
          <div className="field-input"><span className="val mono" style={{ color: "var(--good)" }}>true</span></div>
        </div>
      </ComponentBlock>

      {/* CHealth — Temporal + networked at the component level */}
      <ComponentBlock name="CHealth" tier="Temporal" net>
        <div className="field-row">
          <div className="lbl">Value</div>
          <div className="field-input"><span className="val mono">100</span>
            <span className="mono" style={{ marginLeft: "auto", color: "var(--fg-dim)", fontSize: 10 }}>/ 100</span></div>
        </div>
        <div className="field-row">
          <div className="lbl">Regen</div>
          <div className="field-input"><span className="val mono">2.50</span>
            <span className="mono" style={{ marginLeft: "auto", color: "var(--fg-dim)", fontSize: 10 }}>/s</span></div>
        </div>
      </ComponentBlock>

      {/* Add component bar */}
      <div style={{ padding: 12 }}>
        <div style={{
          border: "1.5px dashed var(--border)", borderRadius: 3, padding: "10px",
          display: "flex", alignItems: "center", justifyContent: "center", gap: 8,
          color: "var(--fg-muted)", fontSize: 12, cursor: "pointer"
        }}>
          <Icon name="plus" size={12}/>
          Add Component  <span className="kbd">⌘⇧A</span>
        </div>
      </div>
    </div>
  </div>
);

const ComponentBlock = ({ name, tier, net, children }) => (
  <div style={{ borderBottom: "1px solid var(--border-soft)" }}>
    <div style={{
      display: "flex", alignItems: "center", gap: 8,
      padding: "8px 12px 6px", background: "var(--bg-app)"
    }}>
      <Icon name="chevD" size={9} color="var(--fg-muted)"/>
      <span style={{ fontFamily: "var(--f-display)", fontWeight: 600, fontSize: 12.5 }}>{name}</span>
      {tier && <Tier kind={tier}/>}
      {net && <span className="chip chip-yellow mono" style={{ fontSize: 9, height: 16 }}>net</span>}
      <button className="btn btn-icon btn-sm btn-ghost" style={{ marginLeft: "auto", width: 18, height: 18 }}>
        <Icon name="x" size={10}/>
      </button>
    </div>
    <div style={{ padding: "4px 0 8px" }}>{children}</div>
  </div>
);

// ── Bottom dock — Assets / Console / Thread Budget tabs + always-visible
//    thread monitor strip on the right ─────────────────────────────────
const BottomDock = () => {
  const tabs = ["Assets", "Console", "Network", "Profiler"];
  const [active] = [0];
  return (
    <div style={{ height: 240, borderTop: "1px solid var(--border)",
                  display: "flex", flexShrink: 0, background: "var(--bg-app)" }}>
      {/* Left: tabbed area */}
      <div style={{ flex: 1, display: "flex", flexDirection: "column", minWidth: 0 }}>
        <div style={{ display: "flex", borderBottom: "1px solid var(--border)",
                      background: "var(--bg-app)", paddingLeft: 4 }}>
          {tabs.map((t, i) => (
            <div key={t} style={{
              padding: "8px 14px", fontSize: 11.5, fontWeight: 600,
              color: i === active ? "var(--fg)" : "var(--fg-muted)",
              borderBottom: i === active ? "2px solid var(--purple)" : "2px solid transparent",
              cursor: "pointer", display: "flex", alignItems: "center", gap: 6,
            }}>
              {t}
              {t === "Console" && (
                <span style={{
                  background: "var(--bad)", color: "white", fontFamily: "var(--f-mono)",
                  fontSize: 9, padding: "0 5px", borderRadius: 8, lineHeight: 1.4
                }}>2</span>
              )}
            </div>
          ))}
          <div style={{ marginLeft: "auto", display: "flex", gap: 6, padding: "0 8px", alignItems: "center" }}>
            <button className="btn btn-sm btn-ghost"><Icon name="plus" size={11}/>Import</button>
            <button className="btn btn-sm btn-ghost"><Icon name="refresh" size={11}/></button>
          </div>
        </div>

        {/* Assets content */}
        <div style={{ flex: 1, display: "flex", minHeight: 0 }}>
          {/* tree */}
          <div style={{ width: 200, borderRight: "1px solid var(--border-soft)",
                        padding: "8px 0", fontSize: 11.5 }}>
            {[
              { n: "▾ Project",          ind: 0 },
              { n: "  ▾ src",             ind: 1 },
              { n: "    GameMode.cpp",   ind: 2 },
              { n: "    Player.cpp",     ind: 2, active: true },
              { n: "  ▾ assets",         ind: 1 },
              { n: "    meshes",          ind: 2 },
              { n: "    materials",       ind: 2 },
              { n: "    textures",        ind: 2 },
              { n: "  ▸ shaders",         ind: 1 },
              { n: "  ▸ scenes",          ind: 1 },
              { n: "  ▸ configs",         ind: 1 },
            ].map((it, i) => (
              <div key={i} style={{
                padding: `3px 8px 3px ${8 + it.ind * 12}px`,
                background: it.active ? "var(--purple-wash)" : "transparent",
                color: it.active ? "var(--fg)" : "var(--fg-muted)",
                display: "flex", alignItems: "center", gap: 5
              }}>
                <Icon name={it.n.includes("▾") || it.n.includes("▸") ? "folder" : "file"}
                      size={11} color={it.active ? "var(--yellow)" : "var(--fg-dim)"}/>
                <span>{it.n.replace(/^[ ▾▸]+/, "")}</span>
              </div>
            ))}
          </div>

          {/* asset grid */}
          <div style={{ flex: 1, padding: "8px 12px", display: "flex", flexDirection: "column", gap: 8 }}>
            <div style={{ display: "flex", alignItems: "center", gap: 6 }}>
              <span className="mono" style={{ fontSize: 10.5, color: "var(--fg-dim)" }}>
                Project / assets / meshes
              </span>
              <span style={{ marginLeft: "auto", display: "flex", gap: 6 }}>
                <span className="chip mono">28 items</span>
                <span className="chip mono">12.4 MB</span>
              </span>
            </div>
            <div style={{ display: "grid", gridTemplateColumns: "repeat(10, 1fr)", gap: 8 }}>
              {[
                ["cube_1m",        "var(--tier-volatile)"],
                ["cube_2m",        "var(--tier-volatile)"],
                ["pyramid",        "var(--purple)"],
                ["sphere_lo",      "var(--tier-cold)"],
                ["sphere_hi",      "var(--tier-cold)"],
                ["plane",          "var(--tier-static)"],
                ["char_player",    "var(--yellow)"],
                ["char_npc_a",     "var(--purple)"],
                ["tree_pine",      "var(--good)"],
                ["rock_l",         "var(--fg-muted)"],
                ["rock_m",         "var(--fg-muted)"],
                ["barrel",         "var(--tier-volatile)"],
                ["crate_wood",     "var(--tier-volatile)"],
                ["gun_blaster",    "var(--yellow)"],
              ].map(([n, c], i) => (
                <div key={n} style={{ display: "flex", flexDirection: "column", gap: 4 }}>
                  <div style={{
                    height: 56, background: "var(--bg-panel)",
                    border: `1px solid ${i === 2 ? "var(--purple)" : "var(--border-soft)"}`,
                    borderRadius: 2, position: "relative", overflow: "hidden",
                    boxShadow: i === 2 ? "0 0 0 1px var(--purple)" : "none"
                  }}>
                    <div style={{
                      position: "absolute", inset: 6,
                      background: `linear-gradient(135deg, ${c} 0%, transparent 100%)`,
                      opacity: 0.35
                    }}/>
                    <div style={{
                      position: "absolute", left: "50%", top: "50%",
                      transform: "translate(-50%, -50%) rotate(45deg)",
                      width: 22, height: 22,
                      background: c, opacity: 0.7,
                      clipPath: "polygon(50% 0%, 100% 50%, 50% 100%, 0% 50%)"
                    }}/>
                    <div style={{
                      position: "absolute", left: 4, top: 4,
                      fontFamily: "var(--f-mono)", fontSize: 8.5, color: "var(--fg-dim)"
                    }}>{i + 1}</div>
                  </div>
                  <div style={{ fontSize: 10.5, color: "var(--fg-muted)",
                                textAlign: "center", overflow: "hidden", textOverflow: "ellipsis",
                                whiteSpace: "nowrap" }}>{n}</div>
                </div>
              ))}
            </div>
          </div>
        </div>
      </div>

      {/* Right: always-on thread budget strip */}
      <div style={{ width: 360, borderLeft: "1px solid var(--border)",
                    background: "var(--bg-panel)", display: "flex", flexDirection: "column" }}>
        <div className="panel-head">
          <Icon name="cpu" size={12} color="var(--fg-muted)"/>
          <span className="title">Frame budget</span>
          <span className="mono" style={{ marginLeft: "auto", fontSize: 10, color: "var(--fg-dim)" }}>1024 fps · 1.0 ms</span>
        </div>
        <div style={{ padding: "10px 14px", display: "flex", flexDirection: "column", gap: 12 }}>
          <ThreadBar label="sentinel" color="var(--th-sentinel)" hz="1000Hz" budget="1.00" used="1.00" pct={1} note="input poll"/>
          <ThreadBar label="brain"    color="var(--th-brain)"    hz="512Hz"  budget="1.95" used="0.73" pct={0.73 / 1.95} note="logic + phys 105µs"/>
          <ThreadBar label="encoder"  color="var(--th-encoder)"  hz="144Hz"  budget="6.94" used="0.88" pct={0.88 / 6.94} note="render · 3 dirty"/>
          <div style={{ display: "flex", gap: 8, alignItems: "center", marginTop: 4 }}>
            <span className="mono" style={{ fontSize: 10, color: "var(--fg-dim)", flex: 1 }}>i → photon</span>
            <span className="mono" style={{ fontSize: 12, color: "var(--good)" }}>7.37 ms</span>
            <Spark data={[6,7,7,8,7,7,8,9,7,7,8,7,6,7,7,8,8,7,7,8]} color="var(--good)" width={80}/>
          </div>
        </div>
      </div>
    </div>
  );
};

const ThreadBar = ({ label, color, hz, budget, used, pct, note }) => (
  <div>
    <div style={{ display: "flex", alignItems: "baseline", gap: 8, marginBottom: 4 }}>
      <span style={{ width: 8, height: 8, borderRadius: 2, background: color }}/>
      <span className="mono" style={{ fontSize: 11, color: "var(--fg)", fontWeight: 600 }}>{label}</span>
      <span className="mono" style={{ fontSize: 10, color: "var(--fg-dim)" }}>{hz}</span>
      <span className="mono" style={{ marginLeft: "auto", fontSize: 11 }}>
        <span style={{ color: "var(--fg)" }}>{used}</span>
        <span style={{ color: "var(--fg-dim)" }}> / {budget} ms</span>
      </span>
    </div>
    <div style={{ height: 8, background: "var(--bg-input)", borderRadius: 1, overflow: "hidden", position: "relative" }}>
      <div style={{
        position: "absolute", left: 0, top: 0, bottom: 0,
        width: `${Math.min(100, pct * 100)}%`,
        background: color,
        boxShadow: `0 0 8px ${color}`
      }}/>
      {/* budget tick at 100% — not the bar width */}
      <div style={{
        position: "absolute", right: 0, top: -2, bottom: -2, width: 1,
        background: "var(--fg-dim)"
      }}/>
    </div>
    <div style={{ fontSize: 10, color: "var(--fg-dim)", marginTop: 2 }}>{note}</div>
  </div>
);

// ── Status bar ───────────────────────────────────────────────────────────
const StatusBar = () => (
  <div style={{
    height: 22, background: "var(--bg-deep)", borderTop: "1px solid var(--border)",
    display: "flex", alignItems: "center", padding: "0 12px", gap: 14, flexShrink: 0,
    fontSize: 10.5, color: "var(--fg-muted)", fontFamily: "var(--f-mono)"
  }}>
    <span><Icon name="dot" size={6} style={{ verticalAlign: "middle", marginRight: 4 }} color="var(--good)"/>Slab healthy</span>
    <span>Δ-bitplanes 3/5</span>
    <span>workers 8 · phys 2</span>
    <span>GPU 1.4ms</span>
    <span>vk::raii 1.4.304</span>
    <span style={{ marginLeft: "auto", color: "var(--yellow-soft)" }}>● recording replay · frame 18,442</span>
    <span style={{ color: "var(--fg-dim)" }}>Trinyx 0.7.2 · Dev-Main · c891af3</span>
  </div>
);

// ── Compose ──────────────────────────────────────────────────────────────
const HiFiEditor = () => (
  <div className="tnx-root" style={{ display: "flex", flexDirection: "column", height: "100%" }}>
    <TopBar/>
    <Toolbar/>
    <div style={{ flex: 1, display: "flex", minHeight: 0, padding: 8, gap: 8,
                  background: "var(--bg-app)" }}>
      <Hierarchy/>
      <div className="panel" style={{ flex: 1, height: "100%" }}>
        <div className="panel-head">
          <Icon name="cube" size={12} color="var(--fg-muted)"/>
          <span className="title">Scene · Testbed</span>
          <span style={{ display: "flex", gap: 6, marginLeft: "auto" }}>
            <span className="chip mono"><Icon name="camera" size={9}/>Pers · 50°</span>
            <span className="chip mono">f / 18,442</span>
          </span>
        </div>
        <Viewport/>
      </div>
      <Inspector/>
    </div>
    <BottomDock/>
    <StatusBar/>
  </div>
);

Object.assign(window, { HiFiEditor });
