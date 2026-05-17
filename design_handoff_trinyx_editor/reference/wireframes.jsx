/* global React */
// Wireframes — three IA explorations for the editor.
// Intentionally low-fi: grayscale, hatched fills, simple boxes, real type
// hierarchy but no color decisions. This is about WHERE things go.

const WfTag = ({ children, fill }) => (
  <span className="wf-tag" style={{ background: fill || "transparent",
                                    color: fill ? "#fff" : "#2a2a2a" }}>{children}</span>
);

// Stylized "scene viewport" placeholder used in all three wireframes
const WfViewport = ({ children, style }) => (
  <div className="wf-box wf-soft" style={{
    position: "relative", display: "flex", alignItems: "center",
    justifyContent: "center", flexDirection: "column", ...style
  }}>
    <div style={{
      position: "absolute", inset: 12,
      border: "1px dashed #999",
      background: "repeating-linear-gradient(135deg, transparent 0 14px, rgba(0,0,0,0.04) 14px 15px)"
    }}/>
    <div style={{ position: "relative", display: "flex", flexDirection: "column",
                  alignItems: "center", gap: 4, color: "#666" }}>
      <div style={{ fontSize: 9, letterSpacing: "0.16em", textTransform: "uppercase" }}>VIEWPORT</div>
      <div style={{ fontSize: 11 }}>3D scene · gizmo · PIE camera</div>
    </div>
    {children}
  </div>
);

const WfList = ({ items, indent = [] }) => (
  <div style={{ padding: "6px 0", flex: 1, overflow: "hidden" }}>
    {items.map((t, i) => (
      <div key={i} style={{
        padding: `3px 8px 3px ${10 + (indent[i] || 0) * 12}px`,
        background: i === 1 ? "#d4d2cc" : "transparent",
        fontSize: 11, color: "#222", display: "flex", justifyContent: "space-between"
      }}>
        <span>{t}</span>
        {i === 1 && <span style={{ fontFamily: "var(--f-mono)", fontSize: 9, color: "#666" }}>0x00A4</span>}
      </div>
    ))}
  </div>
);

// ─────────────────────────────────────────────────────────────────────────
// Layout A — Classic 4-panel
// The proven engine-editor pattern: top menu + toolbar, left hierarchy,
// center viewport, right inspector, bottom asset browser + console.
// Baseline option — what game devs will recognize on day one.
// ─────────────────────────────────────────────────────────────────────────
const WireframeClassic = () => (
  <div className="wf" data-screen-label="wf-classic">
    {/* Menu */}
    <div style={{ display: "flex", height: 24, borderBottom: "1px solid #2a2a2a", alignItems: "center", padding: "0 10px", gap: 14, fontSize: 11 }}>
      <strong style={{ fontFamily: "var(--f-display)", fontSize: 13 }}>trinyx</strong>
      <span>File</span><span>Edit</span><span>Scene</span><span>Build</span><span>Window</span><span>Help</span>
      <span style={{ marginLeft: "auto", fontFamily: "var(--f-mono)", fontSize: 10, color: "#666" }}>⌘K  search anything</span>
    </div>
    {/* Toolbar */}
    <div style={{ display: "flex", height: 34, borderBottom: "1px solid #888", alignItems: "center",
                  padding: "0 8px", gap: 6, background: "#e6e4dd" }}>
      {["select","move","rot","scale"].map(t => (
        <div key={t} className="wf-box" style={{ width: 28, height: 22, fontSize: 9, display: "grid", placeItems: "center" }}>{t}</div>
      ))}
      <div style={{ width: 1, height: 18, background: "#888", margin: "0 4px" }}/>
      <div className="wf-box wf-fill" style={{ height: 22, padding: "0 10px", display: "grid", placeItems: "center", fontSize: 10 }}>▶ Play</div>
      <div className="wf-box" style={{ height: 22, padding: "0 10px", display: "grid", placeItems: "center", fontSize: 10 }}>‖ Pause</div>
      <div className="wf-box" style={{ height: 22, padding: "0 10px", display: "grid", placeItems: "center", fontSize: 10 }}>Step</div>
      <div style={{ marginLeft: "auto", display: "flex", gap: 6, fontSize: 10 }}>
        <WfTag>net: standalone</WfTag>
        <WfTag>brain 0.73ms</WfTag>
      </div>
    </div>

    {/* Body */}
    <div style={{ flex: 1, display: "flex", minHeight: 0 }}>
      {/* Left — Hierarchy */}
      <div style={{ width: 220, borderRight: "1px solid #888", display: "flex", flexDirection: "column" }}>
        <div style={{ padding: "5px 8px", borderBottom: "1px solid #888", background: "#e6e4dd",
                      display: "flex", justifyContent: "space-between", fontSize: 10, letterSpacing: "0.08em", textTransform: "uppercase" }}>
          <span>Hierarchy</span><span>＋</span>
        </div>
        <WfList
          items={["▾ World","  ▾ Player","    Body (View)","    JoltCharacter","    Camera FP","  ▸ Pyramid_25 (5,525)","  ▸ Particles","  Lights × 3","  Skybox"]}
          indent={[0,1,2,2,2,1,1,1,1]}
        />
      </div>

      {/* Center — Viewport */}
      <div style={{ flex: 1, display: "flex", flexDirection: "column" }}>
        <WfViewport style={{ flex: 1 }}/>
      </div>

      {/* Right — Inspector */}
      <div style={{ width: 260, borderLeft: "1px solid #888", display: "flex", flexDirection: "column" }}>
        <div style={{ padding: "5px 8px", borderBottom: "1px solid #888", background: "#e6e4dd",
                      fontSize: 10, letterSpacing: "0.08em", textTransform: "uppercase" }}>
          Inspector  ·  Player
        </div>
        <div style={{ padding: 8, display: "flex", flexDirection: "column", gap: 8, fontSize: 10.5 }}>
          <div className="wf-box" style={{ padding: 6 }}>
            <div className="wf-label" style={{ marginBottom: 4 }}>Transform · Temporal</div>
            <div style={{ display: "grid", gridTemplateColumns: "1fr 1fr 1fr", gap: 3, fontFamily: "var(--f-mono)" }}>
              <div className="wf-box" style={{ padding: "2px 5px" }}>X 12.4</div>
              <div className="wf-box" style={{ padding: "2px 5px" }}>Y 0.0</div>
              <div className="wf-box" style={{ padding: "2px 5px" }}>Z −4.2</div>
            </div>
          </div>
          <div className="wf-box" style={{ padding: 6 }}>
            <div className="wf-label" style={{ marginBottom: 4 }}>JoltCharacter · Phys</div>
            <div className="wf-soft" style={{ height: 14, marginBottom: 3 }}/>
            <div className="wf-soft" style={{ height: 14 }}/>
          </div>
          <div className="wf-box" style={{ padding: 6 }}>
            <div className="wf-label" style={{ marginBottom: 4 }}>Camera · Logic</div>
            <div className="wf-soft" style={{ height: 14, marginBottom: 3 }}/>
            <div className="wf-soft" style={{ height: 14 }}/>
          </div>
          <div className="wf-box wf-dash" style={{ padding: 6, textAlign: "center", color: "#666" }}>+ Add Component</div>
        </div>
      </div>
    </div>

    {/* Bottom — Asset browser + console */}
    <div style={{ height: 150, borderTop: "1px solid #888", display: "flex" }}>
      <div style={{ flex: 1.5, borderRight: "1px solid #888", display: "flex", flexDirection: "column" }}>
        <div style={{ display: "flex", borderBottom: "1px solid #888", background: "#e6e4dd", fontSize: 10 }}>
          <div style={{ padding: "4px 10px", background: "#fff", borderRight: "1px solid #888" }}>Assets</div>
          <div style={{ padding: "4px 10px", color: "#666" }}>Materials</div>
          <div style={{ padding: "4px 10px", color: "#666" }}>Shaders</div>
        </div>
        <div style={{ display: "grid", gridTemplateColumns: "repeat(7, 1fr)", gap: 6, padding: 8 }}>
          {Array.from({ length: 14 }).map((_, i) => (
            <div key={i} style={{ display: "flex", flexDirection: "column", gap: 3 }}>
              <div className="wf-box wf-hatch" style={{ height: 36 }}/>
              <div style={{ fontSize: 9, color: "#444", textAlign: "center" }}>{["cube","tri","plane","mat_red","mat_blue","light","fx_dust","gun","tree","char","sky","grid","ui","tex"][i]}</div>
            </div>
          ))}
        </div>
      </div>
      <div style={{ flex: 1, display: "flex", flexDirection: "column" }}>
        <div style={{ display: "flex", borderBottom: "1px solid #888", background: "#e6e4dd", fontSize: 10 }}>
          <div style={{ padding: "4px 10px", background: "#fff", borderRight: "1px solid #888" }}>Console</div>
          <div style={{ padding: "4px 10px", color: "#666" }}>Profiler</div>
          <div style={{ padding: "4px 10px", color: "#666" }}>Net</div>
        </div>
        <div style={{ padding: 6, fontFamily: "var(--f-mono)", fontSize: 9.5, color: "#222", lineHeight: 1.5 }}>
          <div>[ok] Schema registered · 47 entity types</div>
          <div>[ok] Slab partition resized → 262144</div>
          <div>[warn] CTransform vY had non-finite at chunk 102</div>
          <div>[info] PIE loopback · 2 owner worlds</div>
          <div style={{ color: "#666" }}>──────────────────────</div>
          <div>›  <span style={{ background: "#d4d2cc" }}>spawn 1000 cubes pyramid_</span></div>
        </div>
      </div>
    </div>

    <div style={{ height: 18, borderTop: "1px solid #888", background: "#e6e4dd",
                  display: "flex", alignItems: "center", padding: "0 10px", gap: 14, fontSize: 9.5, color: "#444" }}>
      <span>SCENE: Testbed.tnx</span><span>205k ent</span><span>brain 0.73ms</span><span>encoder 0.88ms</span>
      <span style={{ marginLeft: "auto" }}>SCALE — predictable, expected, baseline</span>
    </div>
  </div>
);

// ─────────────────────────────────────────────────────────────────────────
// Layout B — Modal Workspaces
// Workspaces along the top (Layout / Logic / Simulate / Network / Profile)
// swap the whole panel arrangement to fit one task at a time. Reduces the
// number of visible panels at any moment — you only see what the current
// task needs.
// ─────────────────────────────────────────────────────────────────────────
const WireframeWorkspaces = () => (
  <div className="wf" data-screen-label="wf-workspaces">
    {/* Workspace switcher dominates the top */}
    <div style={{ display: "flex", height: 44, borderBottom: "1px solid #2a2a2a", background: "#fff" }}>
      <div style={{ width: 60, display: "grid", placeItems: "center", borderRight: "1px solid #888" }}>
        <strong style={{ fontFamily: "var(--f-display)", fontSize: 14 }}>tnx</strong>
      </div>
      {[
        ["Layout",   "place + light",  true],
        ["Logic",    "scripts + flow"],
        ["Simulate", "PIE + replay"],
        ["Network",  "PIE loopback"],
        ["Profile",  "frames + threads"],
      ].map(([n, sub, active]) => (
        <div key={n} style={{
          flex: 1, borderRight: "1px solid #888", padding: "5px 12px",
          display: "flex", flexDirection: "column", justifyContent: "center",
          background: active ? "#2a2a2a" : "transparent",
          color: active ? "#fff" : "#222",
        }}>
          <div style={{ fontSize: 12, fontWeight: 700 }}>{n}</div>
          <div style={{ fontSize: 9.5, opacity: 0.7 }}>{sub}</div>
        </div>
      ))}
      <div style={{ width: 200, padding: "0 12px", display: "flex", alignItems: "center", gap: 8 }}>
        <WfTag fill="#2a2a2a">⌘K</WfTag>
        <span style={{ fontSize: 10, color: "#666" }}>search anything</span>
      </div>
    </div>

    {/* Layout-workspace specific arrangement: hierarchy + viewport + inspector
        (no bottom dock — assets float as a slide-up palette). */}
    <div style={{ flex: 1, display: "flex", minHeight: 0 }}>
      <div style={{ width: 200, borderRight: "1px solid #888", display: "flex", flexDirection: "column" }}>
        <div style={{ padding: "5px 8px", borderBottom: "1px solid #888", background: "#e6e4dd",
                      fontSize: 10, letterSpacing: "0.08em", textTransform: "uppercase" }}>Outline</div>
        <WfList
          items={["▾ World","  ▾ Player","    Body","    Camera FP","  ▸ Pyramid_25","  Lights × 3"]}
          indent={[0,1,2,2,1,1]}
        />
        <div style={{ padding: "5px 8px", borderTop: "1px solid #888", background: "#e6e4dd",
                      fontSize: 10, letterSpacing: "0.08em", textTransform: "uppercase" }}>Layers</div>
        <div style={{ padding: 8, fontSize: 11, display: "flex", flexDirection: "column", gap: 4 }}>
          <div>● Geometry</div>
          <div>● Lights</div>
          <div>● Volumes</div>
          <div style={{ color: "#888" }}>○ Debug overlays</div>
        </div>
      </div>

      <div style={{ flex: 1, display: "flex", flexDirection: "column", position: "relative" }}>
        <WfViewport style={{ flex: 1 }}/>
        {/* Floating assets palette specific to LAYOUT workspace */}
        <div style={{ position: "absolute", left: 16, bottom: 16, width: 460,
                      background: "#fff", border: "1px solid #2a2a2a", boxShadow: "4px 4px 0 #2a2a2a" }}>
          <div style={{ padding: "5px 10px", borderBottom: "1px solid #888", background: "#e6e4dd",
                        fontSize: 10, letterSpacing: "0.08em", textTransform: "uppercase",
                        display: "flex", justifyContent: "space-between" }}>
            <span>Place · drag into scene</span><span style={{ fontFamily: "var(--f-mono)" }}>cube / plane / light…</span>
          </div>
          <div style={{ display: "grid", gridTemplateColumns: "repeat(8, 1fr)", gap: 4, padding: 8 }}>
            {Array.from({ length: 8 }).map((_, i) => (
              <div key={i} className="wf-box wf-hatch" style={{ height: 40 }}/>
            ))}
          </div>
        </div>
      </div>

      <div style={{ width: 280, borderLeft: "1px solid #888", display: "flex", flexDirection: "column" }}>
        <div style={{ padding: "5px 8px", borderBottom: "1px solid #888", background: "#e6e4dd",
                      fontSize: 10, letterSpacing: "0.08em", textTransform: "uppercase" }}>Inspector</div>
        <div style={{ padding: 8, display: "flex", flexDirection: "column", gap: 6 }}>
          {["Transform", "Mesh", "Material", "Light"].map(s => (
            <div key={s} className="wf-box" style={{ padding: 6 }}>
              <div className="wf-label" style={{ marginBottom: 4 }}>{s}</div>
              <div className="wf-soft" style={{ height: 14, marginBottom: 3 }}/>
              <div className="wf-soft" style={{ height: 14 }}/>
            </div>
          ))}
        </div>
      </div>
    </div>

    <div style={{ height: 18, borderTop: "1px solid #888", background: "#e6e4dd",
                  display: "flex", alignItems: "center", padding: "0 10px", gap: 14, fontSize: 9.5, color: "#444" }}>
      <span>LAYOUT workspace</span><span>·</span><span>1 / 5</span>
      <span style={{ marginLeft: "auto" }}>SCALE — task-focused, fewer panels per moment</span>
    </div>
  </div>
);

// ─────────────────────────────────────────────────────────────────────────
// Layout C — Zen / Command-First
// Viewport-dominant. Persistent slim left rail (hierarchy as collapsing
// breadcrumb) and right rail (current selection summary). All deep tools
// summoned via ⌘K palette or pinned to a customizable floating "tool tray".
// Bet: experienced devs spend 90% of time in the viewport; surface the
// rest on demand.
// ─────────────────────────────────────────────────────────────────────────
const WireframeZen = () => (
  <div className="wf" data-screen-label="wf-zen" style={{ background: "#ececea" }}>
    <div style={{ flex: 1, display: "flex", minHeight: 0, position: "relative" }}>
      {/* Slim left rail — breadcrumb hierarchy */}
      <div style={{ width: 56, borderRight: "1px solid #aaa", background: "#f4f3ef",
                    display: "flex", flexDirection: "column", alignItems: "center",
                    padding: "10px 0", gap: 8 }}>
        <div className="wf-box wf-fill" style={{ width: 32, height: 32, display: "grid", placeItems: "center", fontSize: 11 }}>tnx</div>
        <div style={{ width: 32, height: 1, background: "#aaa" }}/>
        {["S","H","I","A","C","N","P","■","■"].map((k, i) => (
          <div key={i} className="wf-box" style={{ width: 32, height: 32, display: "grid", placeItems: "center",
            fontSize: 10, background: i === 0 ? "#2a2a2a" : "#fff", color: i === 0 ? "#fff" : "#222" }}>{k}</div>
        ))}
      </div>

      {/* Viewport is the canvas */}
      <div style={{ flex: 1, position: "relative", display: "flex", flexDirection: "column" }}>
        <WfViewport style={{ flex: 1 }}/>

        {/* Floating breadcrumb at top — collapsing hierarchy */}
        <div style={{ position: "absolute", top: 16, left: 16, display: "flex",
                      background: "#fff", border: "1px solid #2a2a2a", padding: "5px 10px",
                      fontSize: 11, gap: 6, alignItems: "center", boxShadow: "3px 3px 0 #2a2a2a" }}>
          <span>World</span><span style={{ color: "#888" }}>/</span>
          <span>Player</span><span style={{ color: "#888" }}>/</span>
          <strong>Body</strong>
          <span style={{ fontFamily: "var(--f-mono)", fontSize: 9, marginLeft: 8,
                         background: "#2a2a2a", color: "#fff", padding: "1px 4px" }}>TEMPORAL · 0x00A4F2</span>
        </div>

        {/* Floating quick-tools tray — user customizable */}
        <div style={{ position: "absolute", left: "50%", top: 16, transform: "translateX(-50%)",
                      display: "flex", background: "#fff", border: "1px solid #2a2a2a",
                      gap: 0, boxShadow: "3px 3px 0 #2a2a2a" }}>
          {["select","move","rot","scale","play","step","stop"].map((t, i) => (
            <div key={t} style={{
              width: 34, height: 30, display: "grid", placeItems: "center",
              borderRight: i < 6 ? "1px solid #888" : "none",
              background: t === "play" ? "#2a2a2a" : t === "select" ? "#d4d2cc" : "#fff",
              color: t === "play" ? "#fff" : "#222", fontSize: 9.5
            }}>{t}</div>
          ))}
        </div>

        {/* Command palette open — central */}
        <div style={{ position: "absolute", left: "50%", top: "32%", transform: "translateX(-50%)",
                      width: 480, background: "#fff", border: "2px solid #2a2a2a",
                      boxShadow: "6px 6px 0 #2a2a2a" }}>
          <div style={{ padding: "8px 12px", borderBottom: "1px solid #2a2a2a", display: "flex", alignItems: "center", gap: 8 }}>
            <WfTag fill="#2a2a2a">⌘K</WfTag>
            <span style={{ fontFamily: "var(--f-mono)", fontSize: 12 }}>spawn pyramid</span>
            <span style={{ width: 2, height: 14, background: "#2a2a2a", animation: "blink 1s steps(2) infinite" }}/>
          </div>
          {[
            ["▶ spawn pyramid 25",         "scene · creates 5,525 cubes",         "↵"],
            ["▶ spawn pyramid 15",         "scene · creates 1,240 cubes",         ""],
            ["⏱ rebuild slab partition",   "engine · dual arena resize",          ""],
            ["⊕ open Profile workspace",   "navigation · frame budget",           "⇧↵"],
          ].map(([t, sub, kb], i) => (
            <div key={i} style={{ padding: "6px 12px", display: "flex", justifyContent: "space-between",
                                  background: i === 0 ? "#d4d2cc" : "transparent", fontSize: 11 }}>
              <div>
                <div>{t}</div>
                <div style={{ fontSize: 9.5, color: "#666" }}>{sub}</div>
              </div>
              {kb && <WfTag>{kb}</WfTag>}
            </div>
          ))}
        </div>
      </div>

      {/* Slim right rail — current selection summary */}
      <div style={{ width: 220, borderLeft: "1px solid #aaa", background: "#f4f3ef",
                    display: "flex", flexDirection: "column" }}>
        <div style={{ padding: "8px 10px", borderBottom: "1px solid #aaa" }}>
          <div className="wf-label">SELECTED</div>
          <div style={{ fontFamily: "var(--f-display)", fontSize: 16, fontWeight: 700 }}>Player.Body</div>
          <div style={{ fontFamily: "var(--f-mono)", fontSize: 9.5, color: "#666" }}>EView&lt;EPlayer&gt;</div>
        </div>
        <div style={{ padding: 10, fontFamily: "var(--f-mono)", fontSize: 10.5, lineHeight: 1.7 }}>
          <div>PosX  <span style={{ float: "right" }}>12.483</span></div>
          <div>PosY  <span style={{ float: "right" }}>0.000</span></div>
          <div>PosZ  <span style={{ float: "right" }}>−4.220</span></div>
          <div>vX    <span style={{ float: "right" }}>0.000</span></div>
          <div>Health<span style={{ float: "right" }}>100</span></div>
          <div style={{ marginTop: 8, color: "#888" }}>━━━ depth ⌥ + click ━━━</div>
        </div>
      </div>
    </div>

    <div style={{ height: 18, background: "#e6e4dd", borderTop: "1px solid #aaa",
                  display: "flex", alignItems: "center", padding: "0 10px", gap: 14, fontSize: 9.5, color: "#444" }}>
      <span>ZEN</span><span>·</span><span>palette open</span>
      <span style={{ marginLeft: "auto" }}>SCALE — sparse default, everything-via-palette</span>
    </div>
  </div>
);

Object.assign(window, { WireframeClassic, WireframeWorkspaces, WireframeZen });
