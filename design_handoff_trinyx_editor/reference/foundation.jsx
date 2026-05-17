/* global React, Icon, Wordmark, LogoMark, Tier */
// Foundation artboards — brand, color, type, iconography, key components.
// These set up the visual vocabulary used in the wireframes and hi-fi.

const FoundationBrand = () => (
  <div className="tnx-root" style={{ height: "100%", padding: 40, display: "flex",
                                     flexDirection: "column", justifyContent: "space-between" }}>
    <div>
      <div style={{ fontFamily: "var(--f-mono)", fontSize: 11, letterSpacing: "0.18em",
                    textTransform: "uppercase", color: "var(--fg-dim)", marginBottom: 24 }}>
        editor · proposal · v0.1
      </div>
      <Wordmark size={92} />
      <div style={{ marginTop: 14, fontFamily: "var(--f-display)", fontSize: 18,
                    color: "var(--fg-muted)", maxWidth: 520, lineHeight: 1.5 }}>
        A data-oriented editor for a data-oriented engine. <span style={{ color: "var(--fg)" }}>Fast.
        Unambiguous. Modular.</span> Built around the three threads, the four tiers,
        and the developer who's reading the slab to debug a frame.
      </div>
    </div>

    <div style={{ display: "grid", gridTemplateColumns: "repeat(4, 1fr)", gap: 14, marginTop: 30 }}>
      {[
        ["Fast",          "Sub-frame feedback. The editor is part of the hot loop, not a tax on it."],
        ["Unambiguous",   "Every value has a unit, a tier, a thread. No magic."],
        ["Modular",       "Panels are real things. Detach, dock, save layouts per workflow."],
        ["Customizable",  "Themes, layouts, palettes. Power-users rebind everything."],
      ].map(([h, p]) => (
        <div key={h} style={{ borderTop: "1px solid var(--border)", paddingTop: 12 }}>
          <div style={{ fontFamily: "var(--f-display)", fontSize: 16, fontWeight: 600,
                        color: "var(--yellow)", marginBottom: 6 }}>{h}</div>
          <div style={{ fontSize: 12, color: "var(--fg-muted)", lineHeight: 1.5 }}>{p}</div>
        </div>
      ))}
    </div>
  </div>
);

// ── Color system ─────────────────────────────────────────────────────────
const Swatch = ({ name, color, token, light }) => (
  <div style={{ display: "flex", flexDirection: "column", gap: 6 }}>
    <div style={{ width: "100%", height: 64, background: color,
                  border: "1px solid var(--border)", borderRadius: 3 }}/>
    <div style={{ display: "flex", justifyContent: "space-between", alignItems: "baseline" }}>
      <div style={{ fontSize: 11, color: light ? "#222" : "var(--fg)", fontWeight: 600 }}>{name}</div>
      <div style={{ fontFamily: "var(--f-mono)", fontSize: 9.5,
                    color: light ? "#666" : "var(--fg-dim)" }}>{token}</div>
    </div>
  </div>
);

const FoundationColor = () => (
  <div className="tnx-root" style={{ height: "100%", padding: 28, display: "flex", flexDirection: "column", gap: 20 }}>
    <div>
      <div style={{ fontFamily: "var(--f-display)", fontSize: 22, fontWeight: 600 }}>Palette</div>
      <div style={{ fontSize: 12, color: "var(--fg-muted)", marginTop: 4 }}>
        Royal purple primary. Warm yellow accent for attention. Deep purple-ink neutrals.
      </div>
    </div>

    <div>
      <div className="mono" style={{ fontSize: 10, letterSpacing: "0.1em", color: "var(--fg-dim)", marginBottom: 8 }}>
        BRAND
      </div>
      <div style={{ display: "grid", gridTemplateColumns: "repeat(4, 1fr)", gap: 12 }}>
        <Swatch name="Purple"      color="oklch(0.62 0.18 295)" token="--purple"/>
        <Swatch name="Purple hot"  color="oklch(0.70 0.20 295)" token="--purple-hot"/>
        <Swatch name="Yellow"      color="oklch(0.86 0.16 92)"  token="--yellow"/>
        <Swatch name="Yellow soft" color="oklch(0.62 0.13 92)"  token="--yellow-soft"/>
      </div>
    </div>

    <div>
      <div className="mono" style={{ fontSize: 10, letterSpacing: "0.1em", color: "var(--fg-dim)", marginBottom: 8 }}>
        SURFACE — DARK
      </div>
      <div style={{ display: "grid", gridTemplateColumns: "repeat(5, 1fr)", gap: 12 }}>
        <Swatch name="Viewport" color="oklch(0.13 0.008 285)" token="--bg-viewport"/>
        <Swatch name="Deep"     color="oklch(0.16 0.010 285)" token="--bg-deep"/>
        <Swatch name="App"      color="oklch(0.19 0.012 285)" token="--bg-app"/>
        <Swatch name="Panel"    color="oklch(0.22 0.013 285)" token="--bg-panel"/>
        <Swatch name="Elevated" color="oklch(0.26 0.014 285)" token="--bg-elev"/>
      </div>
    </div>

    <div>
      <div className="mono" style={{ fontSize: 10, letterSpacing: "0.1em", color: "var(--fg-dim)", marginBottom: 8 }}>
        SEMANTIC + PARTITION TIERS
      </div>
      <div style={{ display: "grid", gridTemplateColumns: "repeat(8, 1fr)", gap: 10 }}>
        <Swatch name="Good"     color="oklch(0.74 0.16 145)" token="--good"/>
        <Swatch name="Warn"     color="oklch(0.80 0.16 75)"  token="--warn"/>
        <Swatch name="Bad"      color="oklch(0.66 0.20 25)"  token="--bad"/>
        <Swatch name="Info"     color="oklch(0.72 0.14 230)" token="--info"/>
        <Swatch name="Cold"     color="oklch(0.68 0.10 220)" token="--tier-cold"/>
        <Swatch name="Static"   color="oklch(0.72 0.06 95)"  token="--tier-static"/>
        <Swatch name="Volatile" color="oklch(0.74 0.15 30)"  token="--tier-volatile"/>
        <Swatch name="Temporal" color="oklch(0.68 0.16 295)" token="--tier-temporal"/>
      </div>
    </div>
  </div>
);

// ── Typography ───────────────────────────────────────────────────────────
const FoundationType = () => (
  <div className="tnx-root" style={{ height: "100%", padding: 28, display: "flex", flexDirection: "column", gap: 22 }}>
    <div>
      <div style={{ fontFamily: "var(--f-display)", fontSize: 22, fontWeight: 600 }}>Type</div>
      <div style={{ fontSize: 12, color: "var(--fg-muted)", marginTop: 4 }}>
        Space Grotesk for display & marks. Manrope for UI. JetBrains Mono for every value, path, and hex.
      </div>
    </div>

    <div style={{ borderTop: "1px solid var(--border)", paddingTop: 14 }}>
      <div className="mono" style={{ fontSize: 10, letterSpacing: "0.1em", color: "var(--fg-dim)" }}>
        DISPLAY · Space Grotesk
      </div>
      <div className="display" style={{ fontSize: 56, fontWeight: 600, lineHeight: 1.05, marginTop: 8 }}>
        205,000 entities.<br/>One frame.
      </div>
    </div>

    <div style={{ display: "grid", gridTemplateColumns: "1.2fr 1fr", gap: 28 }}>
      <div style={{ borderTop: "1px solid var(--border)", paddingTop: 14 }}>
        <div className="mono" style={{ fontSize: 10, letterSpacing: "0.1em", color: "var(--fg-dim)" }}>
          UI · Manrope
        </div>
        <div style={{ marginTop: 8, display: "flex", flexDirection: "column", gap: 4 }}>
          <div style={{ fontSize: 20, fontWeight: 700 }}>Panel header — semibold 20/24</div>
          <div style={{ fontSize: 14, fontWeight: 600, color: "var(--fg)" }}>Section title — semibold 14/20</div>
          <div style={{ fontSize: 12, color: "var(--fg)" }}>Body — regular 12/17</div>
          <div style={{ fontSize: 11, color: "var(--fg-muted)", textTransform: "uppercase", letterSpacing: "0.08em" }}>
            Eyebrow — 11 caps 0.08em
          </div>
        </div>
      </div>

      <div style={{ borderTop: "1px solid var(--border)", paddingTop: 14 }}>
        <div className="mono" style={{ fontSize: 10, letterSpacing: "0.1em", color: "var(--fg-dim)" }}>
          MONO · JetBrains
        </div>
        <div className="mono" style={{ marginTop: 8, fontSize: 12, color: "var(--fg)", lineHeight: 1.6 }}>
          <div><span style={{ color: "var(--purple-hot)" }}>EntityCacheIndex</span> 0x00A4F2</div>
          <div>Transform.PosX <span style={{ color: "var(--yellow)" }}>= 12.4839</span></div>
          <div>brain   <span style={{ color: "var(--good)" }}>0.73 ms</span> / 1.95</div>
          <div>encoder <span style={{ color: "var(--good)" }}>0.88 ms</span> / 8.00</div>
        </div>
      </div>
    </div>
  </div>
);

// ── Iconography ──────────────────────────────────────────────────────────
const FoundationIcons = () => {
  const icons = [
    "select","move","rotate","scale","cube","light","camera",
    "play","pause","stop","step",
    "eye","eyeOff","lock","unlock",
    "chevR","chevD","plus","minus","x",
    "folder","file","code","search","cmd",
    "bug","layers","grid","cpu","network",
    "save","refresh","gear","timer","flask","spark",
  ];
  return (
    <div className="tnx-root" style={{ height: "100%", padding: 28, display: "flex", flexDirection: "column", gap: 14 }}>
      <div>
        <div style={{ fontFamily: "var(--f-display)", fontSize: 22, fontWeight: 600 }}>Iconography</div>
        <div style={{ fontSize: 12, color: "var(--fg-muted)", marginTop: 4 }}>
          1.5px strokes, 16px grid, rounded caps. Single-color, takes accent on active.
        </div>
      </div>
      <div style={{ display: "grid", gridTemplateColumns: "repeat(9, 1fr)", gap: 0,
                    border: "1px solid var(--border)", borderRadius: 4, overflow: "hidden" }}>
        {icons.map((n, i) => (
          <div key={n} style={{
            display: "flex", flexDirection: "column", alignItems: "center", gap: 6,
            padding: "14px 6px",
            borderRight: (i + 1) % 9 ? "1px solid var(--border-soft)" : "none",
            borderTop: i >= 9 ? "1px solid var(--border-soft)" : "none",
            background: "var(--bg-panel)"
          }}>
            <Icon name={n} size={16} color="var(--fg-muted)"/>
            <div className="mono" style={{ fontSize: 9, color: "var(--fg-dim)" }}>{n}</div>
          </div>
        ))}
      </div>

      <div style={{ display: "flex", gap: 14, marginTop: 6 }}>
        <div style={{ display: "flex", alignItems: "center", gap: 8, color: "var(--purple-hot)" }}>
          <Icon name="play" size={20}/><span className="mono" style={{ fontSize: 11 }}>20px active</span>
        </div>
        <div style={{ display: "flex", alignItems: "center", gap: 8, color: "var(--yellow)" }}>
          <Icon name="spark" size={20}/><span className="mono" style={{ fontSize: 11 }}>20px callout</span>
        </div>
      </div>
    </div>
  );
};

// ── Key components — buttons / chips / partition badges / field rows ─────
const FoundationComponents = () => (
  <div className="tnx-root" style={{ height: "100%", padding: 28, display: "flex", flexDirection: "column", gap: 20 }}>
    <div>
      <div style={{ fontFamily: "var(--f-display)", fontSize: 22, fontWeight: 600 }}>Components</div>
      <div style={{ fontSize: 12, color: "var(--fg-muted)", marginTop: 4 }}>
        Atoms used everywhere — buttons, chips, kbds, partition tier badges, value fields.
      </div>
    </div>

    <div>
      <div className="mono" style={{ fontSize: 10, letterSpacing: "0.1em", color: "var(--fg-dim)", marginBottom: 10 }}>BUTTONS</div>
      <div style={{ display: "flex", gap: 10, alignItems: "center", flexWrap: "wrap" }}>
        <button className="btn btn-primary"><Icon name="play"/>Play in Editor</button>
        <button className="btn"><Icon name="save"/>Save</button>
        <button className="btn btn-yellow"><Icon name="spark"/>Rebuild slab</button>
        <button className="btn btn-ghost"><Icon name="refresh"/>Refresh</button>
        <button className="btn btn-icon"><Icon name="gear"/></button>
        <button className="btn btn-sm">Add component…</button>
      </div>
    </div>

    <div>
      <div className="mono" style={{ fontSize: 10, letterSpacing: "0.1em", color: "var(--fg-dim)", marginBottom: 10 }}>CHIPS / KBDS / TIERS</div>
      <div style={{ display: "flex", gap: 8, alignItems: "center", flexWrap: "wrap" }}>
        <span className="chip">DUAL</span>
        <span className="chip chip-purple">selected</span>
        <span className="chip chip-yellow">dirty</span>
        <span className="chip chip-good">net synced</span>
        <Tier kind="Cold"/>
        <Tier kind="Static"/>
        <Tier kind="Volatile"/>
        <Tier kind="Temporal"/>
        <span className="kbd">⌘</span><span className="kbd">K</span>
        <span className="kbd">⇧</span><span className="kbd">F5</span>
      </div>
    </div>

    <div>
      <div className="mono" style={{ fontSize: 10, letterSpacing: "0.1em", color: "var(--fg-dim)", marginBottom: 10 }}>INSPECTOR FIELD ROW</div>
      <div style={{ background: "var(--bg-panel)", border: "1px solid var(--border)",
                    borderRadius: 4, padding: "6px 0", width: 380 }}>
        <div className="field-row">
          <div className="lbl">Position</div>
          <div className="vec3">
            <div className="field-input"><span className="axis axis-x">X</span><span className="val mono">12.483</span></div>
            <div className="field-input"><span className="axis axis-y">Y</span><span className="val mono">0.000</span></div>
            <div className="field-input"><span className="axis axis-z">Z</span><span className="val mono">−4.220</span></div>
          </div>
        </div>
        <div className="field-row">
          <div className="lbl">Mass</div>
          <div className="field-input"><span className="val mono">1.000</span>
            <span style={{ marginLeft: "auto", color: "var(--fg-dim)", fontSize: 10 }}>kg</span></div>
        </div>
        <div className="field-row">
          <div className="lbl" style={{ display: "flex", justifyContent: "flex-end", gap: 4, alignItems: "center" }}>
            <Tier kind="Temporal"/>Health
          </div>
          <div className="field-input"><span className="val mono">100</span>
            <span style={{ marginLeft: "auto", color: "var(--yellow)", fontSize: 10 }} className="mono">net</span></div>
        </div>
      </div>
    </div>
  </div>
);

Object.assign(window, { FoundationBrand, FoundationColor, FoundationType, FoundationIcons, FoundationComponents });
