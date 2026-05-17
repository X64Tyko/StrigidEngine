/* global React, Icon, Tier, Spark, CubeIso */

// ── 6. Slab Heatmap — the cache, as the engine sees it ───────────────────
// Per Debugging.md: a GPU-generated texture visualizing the entire Volatile
// + Temporal SoA slab. Field-major: X = entities, Y = fields stacked with
// history frames T..T-N per field. Macro view uses popcount on the 64-bit
// TemporalFlags bitplane to instantly reveal partition boundaries.
const IdeaSlabHeatmap = () => {
  // synthesize a heatmap texture
  const W = 320, H = 180;
  const cells = [];
  for (let y = 0; y < H; y++) {
    for (let x = 0; x < W; x++) {
      // 4 partition bands across X
      let band = "render";
      if (x > W * 0.18 && x < W * 0.42) band = "dual";
      else if (x >= W * 0.42 && x < W * 0.62) band = "phys";
      else if (x >= W * 0.62) band = "logic";

      // base density
      let v = 0.6 + Math.sin((x / 11)) * 0.18 + Math.sin((y / 7)) * 0.12 + (Math.random() - 0.5) * 0.2;
      // gaps in render band
      if (band === "render" && Math.floor(x / 12) % 7 === 3) v = 0;
      // sparse logic band
      if (band === "logic" && x > W * 0.78) v *= 0.45;
      // temporal streaks: movement leaves trails in lower rows
      if (y > H * 0.55 && Math.abs(((x - y) % 40)) < 3) v = Math.min(1, v + 0.4);
      cells.push({ x, y, v, band });
    }
  }
  return (
    <div className="tnx-root" style={{ height: "100%", background: "var(--bg-app)",
                                       padding: 28, display: "flex", flexDirection: "column", gap: 16 }}>
      <Header2
        kicker="06 · CACHE · SLAB HEATMAP"
        title="The slab, as the GPU draws it"
        sub="A compute-shader-rendered texture of the entire Volatile + Temporal slab. Field-major: X = entities, Y = fields with history frames. Macro view popcount's the 64-bit TemporalFlags — partition boundaries appear instantly."
      />

      <div style={{ flex: 1, display: "grid", gridTemplateColumns: "2.4fr 1fr", gap: 16, minHeight: 0 }}>
        {/* Heatmap canvas */}
        <div style={{ background: "var(--bg-deep)", border: "1px solid var(--border)",
                      borderRadius: 4, padding: 14, display: "flex", flexDirection: "column", gap: 8,
                      minHeight: 0 }}>
          <div style={{ display: "flex", alignItems: "center", gap: 8 }}>
            <span className="mono" style={{ fontSize: 10.5, color: "var(--fg-dim)",
                                              letterSpacing: "0.08em", textTransform: "uppercase" }}>
              slab_visualizer.slang
            </span>
            <span className="chip mono">Volatile + Temporal</span>
            <span className="chip mono">field-major</span>
            <span style={{ marginLeft: "auto", display: "flex", gap: 4 }}>
              <button className="btn btn-sm btn-primary">Macro</button>
              <button className="btn btn-sm">Micro</button>
              <button className="btn btn-sm btn-ghost">Chunk overlay</button>
            </span>
          </div>

          {/* Axes labels */}
          <div style={{ position: "relative", flex: 1, display: "flex", flexDirection: "column", minHeight: 0 }}>
            <div style={{ display: "flex", gap: 4, paddingLeft: 56, paddingRight: 12,
                          fontFamily: "var(--f-mono)", fontSize: 9.5, color: "var(--fg-dim)" }}>
              <span style={{ width: "18%", textAlign: "center" }}>RENDER</span>
              <span style={{ width: "24%", textAlign: "center", color: "var(--tier-volatile)" }}>DUAL</span>
              <span style={{ width: "20%", textAlign: "center", color: "var(--tier-temporal)" }}>PHYS</span>
              <span style={{ width: "38%", textAlign: "center" }}>LOGIC</span>
            </div>
            <div style={{ display: "flex", flex: 1, gap: 6, minHeight: 0 }}>
              {/* Y axis — field labels */}
              <div style={{ width: 50, display: "flex", flexDirection: "column",
                            fontFamily: "var(--f-mono)", fontSize: 9.5, color: "var(--fg-dim)",
                            justifyContent: "space-around", textAlign: "right", paddingRight: 4 }}>
                <span>Pos·T</span>
                <span>Pos·T-1</span>
                <span>Pos·T-2</span>
                <span style={{ color: "var(--tier-temporal)" }}>Vel·T</span>
                <span style={{ color: "var(--tier-temporal)" }}>Vel·T-1</span>
                <span>Flag</span>
                <span>Mesh</span>
                <span>Tint</span>
              </div>

              {/* SVG heatmap */}
              <div style={{ flex: 1, border: "1px solid var(--border)",
                            background: "oklch(0.10 0.005 285)", overflow: "hidden", position: "relative" }}>
                <svg viewBox={`0 0 ${W} ${H}`} preserveAspectRatio="none"
                     style={{ width: "100%", height: "100%", display: "block",
                              imageRendering: "pixelated", shapeRendering: "crispEdges" }}>
                  {cells.map((c, i) => {
                    let fill;
                    if (c.v <= 0.05) {
                      fill = "oklch(0.10 0.005 285)";   // black gap
                    } else if (c.band === "render") {
                      fill = `oklch(${0.32 + c.v * 0.4} 0.06 220 / ${0.5 + c.v * 0.5})`;
                    } else if (c.band === "dual") {
                      fill = `oklch(${0.36 + c.v * 0.45} ${0.10 + c.v * 0.08} 30 / ${0.5 + c.v * 0.5})`;
                    } else if (c.band === "phys") {
                      fill = `oklch(${0.34 + c.v * 0.42} ${0.10 + c.v * 0.10} 295 / ${0.5 + c.v * 0.5})`;
                    } else {
                      fill = `oklch(${0.32 + c.v * 0.38} 0.04 90 / ${0.45 + c.v * 0.55})`;
                    }
                    return <rect key={i} x={c.x} y={c.y} width="1" height="1" fill={fill}/>;
                  })}
                  {/* partition boundaries */}
                  <line x1={W * 0.18} y1="0" x2={W * 0.18} y2={H} stroke="var(--yellow)" strokeWidth="0.4" opacity="0.6"/>
                  <line x1={W * 0.42} y1="0" x2={W * 0.42} y2={H} stroke="var(--yellow)" strokeWidth="0.4" opacity="0.6"/>
                  <line x1={W * 0.62} y1="0" x2={W * 0.62} y2={H} stroke="var(--yellow)" strokeWidth="0.4" opacity="0.6"/>
                  {/* row separators between fields */}
                  {[0.125, 0.25, 0.375, 0.5, 0.625, 0.75, 0.875].map((f, i) => (
                    <line key={i} x1="0" y1={H * f} x2={W} y2={H * f}
                          stroke="oklch(0.30 0.01 285)" strokeWidth="0.3"/>
                  ))}
                  {/* hover target — illustrative tooltip rect */}
                  <rect x={W * 0.46} y={H * 0.55} width="6" height={H * 0.32}
                        fill="none" stroke="var(--yellow)" strokeWidth="0.6"/>
                </svg>

                {/* tooltip popping off the hover target */}
                <div style={{
                  position: "absolute", left: "50%", top: "62%",
                  background: "var(--bg-panel)", border: "1px solid var(--yellow-soft)",
                  padding: "6px 10px", borderRadius: 3, fontFamily: "var(--f-mono)",
                  fontSize: 10.5, color: "var(--fg)", boxShadow: "0 4px 12px oklch(0.05 0.04 295 / 0.7)"
                }}>
                  <div style={{ color: "var(--yellow)" }}>cell (e 0x00B1F4, Vel·T-1)</div>
                  <div>BarrelAssembly owned by Turret_3</div>
                  <div style={{ color: "var(--fg-dim)" }}>chunk 14 [62/64] · phys · DUAL</div>
                </div>
              </div>
            </div>

            {/* X axis — entity index */}
            <div style={{ display: "flex", paddingLeft: 56, paddingTop: 4,
                          fontFamily: "var(--f-mono)", fontSize: 9.5, color: "var(--fg-dim)",
                          justifyContent: "space-between" }}>
              <span>e 0</span>
              <span>e 8k</span>
              <span>e 16k</span>
              <span>e 24k</span>
              <span>e 32k</span>
            </div>
          </div>
        </div>

        {/* Right: legend + actions */}
        <div style={{ display: "flex", flexDirection: "column", gap: 10, minWidth: 0 }}>
          <div className="panel">
            <div className="panel-head"><span className="title">Legend — macro view</span></div>
            <div style={{ padding: "10px 12px", display: "flex", flexDirection: "column", gap: 6,
                          fontSize: 11.5 }}>
              <LegendSwatch color="oklch(0.10 0.005 285)" label="macro-gap (no entity)"/>
              <LegendSwatch color="oklch(0.45 0.06 220)"  label="render · cosmetic"/>
              <LegendSwatch color="oklch(0.50 0.14 30)"   label="dual · physics + render"/>
              <LegendSwatch color="oklch(0.48 0.14 295)"  label="phys · simulation only"/>
              <LegendSwatch color="oklch(0.42 0.04 90)"   label="logic · scripts / AI"/>
              <div style={{ height: 1, background: "var(--border-soft)", margin: "4px 0" }}/>
              <div style={{ color: "var(--fg-muted)", fontSize: 11 }}>
                Brightness = popcount of 64-bit TemporalFlags. Dim = fragmented chunk,
                bright = packed memory. Streaks = entity movement across history frames.
              </div>
            </div>
          </div>

          <div className="panel">
            <div className="panel-head"><span className="title">Slab health</span></div>
            <div style={{ padding: "10px 12px", display: "flex", flexDirection: "column", gap: 6,
                          fontFamily: "var(--f-mono)", fontSize: 11 }}>
              <KV k="Volatile"   v="262,144 cells · 84%"  c="var(--good)"/>
              <KV k="Temporal"   v="65,536 × 8f · 71%"   c="var(--good)"/>
              <KV k="Fragmented" v="3 chunks · 6% gap"   c="var(--warn)"/>
              <KV k="Defrag in"  v="≈ 42 frames"         c="var(--fg-muted)"/>
            </div>
          </div>

          <div style={{ display: "flex", flexDirection: "column", gap: 6 }}>
            <button className="btn btn-sm"><Icon name="refresh" size={11}/>Force defragment</button>
            <button className="btn btn-sm">Show Cold archetype chunks</button>
            <button className="btn btn-sm btn-yellow">Resimulate from T-4</button>
          </div>
        </div>
      </div>
    </div>
  );
};

const LegendSwatch = ({ color, label }) => (
  <div style={{ display: "flex", alignItems: "center", gap: 8 }}>
    <div style={{ width: 16, height: 12, background: color, border: "1px solid var(--border-soft)", borderRadius: 1 }}/>
    <span style={{ color: "var(--fg)" }}>{label}</span>
  </div>
);

const KV = ({ k, v, c }) => (
  <div style={{ display: "flex" }}>
    <span style={{ color: "var(--fg-dim)" }}>{k}</span>
    <span style={{ marginLeft: "auto", color: c }}>{v}</span>
  </div>
);

// ── 7. Visual Script — Node Script workspace ─────────────────────────────
// Per Overview.md: 25 node types across 5 categories, codegen to C++.
// Determinism validator surfaces inline (Branch in pre/post-physics paths
// is rejected). The code preview is right there — you can read the C++ as
// you wire the graph.
const IdeaVisualScript = () => (
  <div className="tnx-root" style={{ height: "100%", background: "var(--bg-app)",
                                     padding: 28, display: "flex", flexDirection: "column", gap: 16 }}>
    <Header2
      kicker="07 · LOGIC WORKSPACE"
      title="Node script with live C++ codegen"
      sub="The Logic workspace is a node graph that emits real C++ to a real file. Determinism constraints are enforced inline: if you drop a Branch into a pre-physics path, the linter explains why before you compile."
    />

    <div style={{ flex: 1, display: "grid", gridTemplateColumns: "240px 1fr 360px", gap: 12, minHeight: 0 }}>
      {/* Node palette */}
      <div className="panel">
        <div className="panel-head"><span className="title">Nodes</span></div>
        <div style={{ padding: "6px 0", overflow: "auto", flex: 1 }}>
          {[
            ["EVENTS",     ["OnPrePhysics", "OnPostPhysics", "OnUpdate", "OnSpawn", "OnDestroy"], "var(--purple)"],
            ["FLOW",       ["Sequence", "Branch"], "var(--yellow)"],
            ["PROPERTIES", ["GetProperty", "SetProperty"], "var(--info)"],
            ["MATH",       ["Add", "Subtract", "Multiply", "Divide", "Clamp", "Lerp"], "var(--good)"],
            ["VECTORS",    ["MakeVec3", "BreakVec3", "Length", "Normalize", "Scale"], "var(--th-encoder)"],
            ["ENTITY",     ["GetPosition", "SetPosition", "GetVelocity", "SetVelocity", "ApplyImpulse"], "var(--tier-volatile)"],
          ].map(([h, items, c]) => (
            <div key={h} style={{ marginBottom: 4 }}>
              <div style={{ padding: "4px 10px", fontFamily: "var(--f-mono)", fontSize: 9.5,
                            letterSpacing: "0.08em", color: c, background: "var(--bg-app)" }}>{h}</div>
              {items.map(n => (
                <div key={n} style={{ padding: "3px 14px", fontSize: 11.5, color: "var(--fg-muted)",
                                       borderLeft: `2px solid ${c}`, marginLeft: 10, cursor: "grab" }}>{n}</div>
              ))}
            </div>
          ))}
        </div>
      </div>

      {/* Graph canvas */}
      <div style={{ position: "relative", background: "var(--bg-deep)",
                    border: "1px solid var(--border)", borderRadius: 3, overflow: "hidden" }}>
        {/* dot grid */}
        <div style={{ position: "absolute", inset: 0,
                      backgroundImage: "radial-gradient(oklch(0.30 0.02 285) 1px, transparent 1px)",
                      backgroundSize: "16px 16px", opacity: 0.45 }}/>

        <svg viewBox="0 0 800 400" preserveAspectRatio="xMidYMid meet"
             style={{ position: "absolute", inset: 0, width: "100%", height: "100%" }}>
          {/* Wires */}
          <Wire x1={148} y1={88}  x2={252} y2={88}/>
          <Wire x1={148} y1={108} x2={252} y2={108} color="var(--good)"/>
          <Wire x1={368} y1={88}  x2={472} y2={88}/>
          <Wire x1={368} y1={110} x2={472} y2={195}/>
          <Wire x1={368} y1={130} x2={472} y2={285} bad/>
          <Wire x1={580} y1={195} x2={680} y2={195}/>

          {/* Event node */}
          <ScriptNode x={28} y={60} w={120} title="OnPrePhysics" color="var(--purple)"
                      ports={[{ kind: "out", y: 28, label: "▶" }, { kind: "out", y: 48, label: "dt", green: true }]}/>
          {/* Get pos */}
          <ScriptNode x={252} y={60} w={116} title="GetPosition" color="var(--tier-volatile)"
                      ports={[
                        { kind: "in", y: 28, label: "▶" },
                        { kind: "in", y: 48, label: "Self" },
                        { kind: "out", y: 28, label: "▶" },
                        { kind: "out", y: 50, label: "Pos", green: true },
                        { kind: "out", y: 70, label: "·", green: true },
                      ]}/>
          {/* Add */}
          <ScriptNode x={472} y={60} w={108} title="Add (Vec3)" color="var(--good)"
                      ports={[
                        { kind: "in", y: 28, label: "A" },
                        { kind: "in", y: 48, label: "B" },
                        { kind: "out", y: 38, label: "Sum", green: true },
                      ]}/>
          {/* Branch — flagged invalid */}
          <ScriptNode x={472} y={170} w={108} title="Branch" color="var(--bad)" bad
                      ports={[
                        { kind: "in", y: 28, label: "▶" },
                        { kind: "in", y: 48, label: "cond" },
                        { kind: "out", y: 28, label: "True" },
                        { kind: "out", y: 50, label: "False" },
                      ]}/>
          {/* SetPosition */}
          <ScriptNode x={680} y={170} w={116} title="SetPosition" color="var(--tier-volatile)"
                      ports={[
                        { kind: "in", y: 28, label: "▶" },
                        { kind: "in", y: 48, label: "Self" },
                        { kind: "in", y: 68, label: "Pos" },
                      ]}/>
        </svg>

        {/* Lint error callout */}
        <div style={{
          position: "absolute", left: "50%", bottom: 14, transform: "translateX(-50%)",
          background: "oklch(0.20 0.10 25 / 0.95)", border: "1px solid var(--bad)",
          borderRadius: 3, padding: "8px 14px", display: "flex", alignItems: "center", gap: 10,
          maxWidth: 540
        }}>
          <Icon name="bug" size={14} color="var(--bad)"/>
          <div style={{ fontSize: 12 }}>
            <div style={{ color: "var(--bad)", fontWeight: 600 }}>Determinism violation</div>
            <div style={{ color: "var(--fg-muted)", fontSize: 11 }}>
              <span className="mono">Branch</span> in <span className="mono">OnPrePhysics</span> path
              breaks rollback. Use <span className="mono">Select</span> or move the conditional to <span className="mono">OnUpdate</span>.
            </div>
          </div>
          <button className="btn btn-sm" style={{ marginLeft: "auto" }}>Auto-fix</button>
        </div>
      </div>

      {/* C++ preview */}
      <div className="panel">
        <div className="panel-head">
          <Icon name="code" size={12} color="var(--fg-muted)"/>
          <span className="title">Codegen · Player_Move.gen.cpp</span>
          <span className="chip chip-yellow mono" style={{ marginLeft: "auto", fontSize: 9.5 }}>1 lint</span>
        </div>
        <div style={{ padding: "10px 12px", fontFamily: "var(--f-mono)", fontSize: 11,
                      lineHeight: 1.55, overflow: "auto", flex: 1 }}>
          <CodeLine n={1}><K>void</K> <I>Player</I>::<F>PrePhysics</F>(<K>SimFloat</K> dt) {`{`}</CodeLine>
          <CodeLine n={2}>{"  "}<C>// generated from Player_Move.tnxgraph</C></CodeLine>
          <CodeLine n={3}>{"  "}<K>auto</K> pos = Transform.<F>GetPos</F>();</CodeLine>
          <CodeLine n={4}>{"  "}<K>auto</K> sum = pos + Vel.<F>AsVec3</F>() * dt;</CodeLine>
          <CodeLine n={5} bad>{"  "}<K>if</K> (<I>SHOULD_DAMP</I>) {"{"}  <C>// ⚠ determinism</C></CodeLine>
          <CodeLine n={6}>{"    "}Transform.<F>SetPos</F>(sum);</CodeLine>
          <CodeLine n={7}>{"  "}{"}"}</CodeLine>
          <CodeLine n={8}>{"}"}</CodeLine>
          <div style={{ height: 10 }}/>
          <div style={{ color: "var(--fg-dim)" }}>// emitted by Node Script · {`{topological walk}`}</div>
          <div style={{ color: "var(--fg-dim)" }}>// schema v3 · 0.84 KB · 4 nodes</div>
        </div>
        <div style={{ padding: "8px 12px", borderTop: "1px solid var(--border-soft)",
                      display: "flex", gap: 6 }}>
          <button className="btn btn-sm">Emit</button>
          <button className="btn btn-sm btn-ghost">Validate</button>
          <button className="btn btn-sm btn-primary" style={{ marginLeft: "auto" }}>Compile</button>
        </div>
      </div>
    </div>
  </div>
);

const ScriptNode = ({ x, y, w, title, color, ports, bad }) => {
  const h = Math.max(60, 24 + ports.length * 12);
  return (
    <g>
      {bad && <rect x={x - 2} y={y - 2} width={w + 4} height={h + 4} rx="4"
                    fill="none" stroke="var(--bad)" strokeWidth="1.5" strokeDasharray="3 3"/>}
      <rect x={x} y={y} width={w} height={h} rx="3"
            fill="var(--bg-panel)" stroke={color} strokeWidth="1"/>
      <rect x={x} y={y} width={w} height="20" rx="3" fill={color} opacity="0.85"/>
      <text x={x + 8} y={y + 14} fontFamily="var(--f-display)" fontSize="11" fontWeight="600"
            fill={bad ? "white" : "white"}>{title}</text>
      {ports.map((p, i) => {
        const px = p.kind === "in" ? x : x + w;
        const py = y + p.y;
        return (
          <g key={i}>
            <circle cx={px} cy={py} r="3" fill={p.green ? "var(--good)" : "var(--yellow)"}
                    stroke="var(--bg-deep)" strokeWidth="1"/>
            <text x={p.kind === "in" ? px + 7 : px - 7} y={py + 3}
                  textAnchor={p.kind === "in" ? "start" : "end"}
                  fontFamily="var(--f-mono)" fontSize="9.5" fill="var(--fg-muted)">
              {p.label}
            </text>
          </g>
        );
      })}
    </g>
  );
};

const Wire = ({ x1, y1, x2, y2, color, bad }) => {
  const mx = (x1 + x2) / 2;
  const d = `M ${x1} ${y1} C ${mx} ${y1}, ${mx} ${y2}, ${x2} ${y2}`;
  return (
    <path d={d} fill="none"
          stroke={bad ? "var(--bad)" : color || "var(--yellow)"}
          strokeWidth="1.5"
          strokeDasharray={bad ? "4 3" : "none"}
          opacity={bad ? 0.85 : 0.9}/>
  );
};

const K = ({ children }) => <span style={{ color: "var(--purple-hot)" }}>{children}</span>;
const I = ({ children }) => <span style={{ color: "var(--yellow)" }}>{children}</span>;
const F = ({ children }) => <span style={{ color: "var(--th-encoder)" }}>{children}</span>;
const C = ({ children }) => <span style={{ color: "var(--fg-dim)", fontStyle: "italic" }}>{children}</span>;
const CodeLine = ({ n, bad, children }) => (
  <div style={{ display: "flex", background: bad ? "oklch(0.25 0.10 25 / 0.3)" : "transparent" }}>
    <span style={{ width: 24, color: "var(--fg-ghost)", textAlign: "right", paddingRight: 8,
                   borderRight: "1px solid var(--border-soft)" }}>{n}</span>
    <span style={{ paddingLeft: 8, whiteSpace: "pre" }}>{children}</span>
  </div>
);

// ── 8. Component Generator — form to disk in 30 seconds ──────────────────
// Per Overview.md: form-based ECS component header generator.
// Tier + SystemGroup + dynamic field list → emits the right macro
// (TNX_TEMPORAL_FIELDS / TNX_VOLATILE_FIELDS / TNX_REGISTER_FIELDS).
const IdeaComponentGen = () => (
  <div className="tnx-root" style={{ height: "100%", background: "var(--bg-app)",
                                     padding: 28, display: "flex", flexDirection: "column", gap: 16 }}>
    <Header2
      kicker="08 · COMPONENT GENERATOR"
      title="From form to .hpp, live"
      sub="The form on the left writes the macro on the right. New devs don't memorise which macro emits which storage tier — they pick from a dropdown and the right thing happens."
    />

    <div style={{ flex: 1, display: "grid", gridTemplateColumns: "420px 1fr", gap: 16, minHeight: 0 }}>
      {/* Form */}
      <div className="panel">
        <div className="panel-head"><span className="title">New component</span></div>
        <div style={{ padding: "14px 16px", display: "flex", flexDirection: "column", gap: 14 }}>
          <FormRow label="Name">
            <div className="field-input" style={{ height: 28, fontSize: 13 }}>
              <span className="val mono">CVelocity</span>
            </div>
          </FormRow>
          <FormRow label="Storage tier">
            <div style={{ display: "flex", gap: 4 }}>
              {[
                ["Cold",     false],
                ["Static",   false],
                ["Volatile", false],
                ["Temporal", true],
              ].map(([t, on]) => (
                <button key={t} className={on ? "btn btn-primary" : "btn"} style={{ flex: 1, height: 26 }}>
                  <span className="mono" style={{ fontSize: 11 }}>{t}</span>
                </button>
              ))}
            </div>
          </FormRow>
          <FormRow label="SystemGroup">
            <div style={{ display: "flex", gap: 4 }}>
              {[
                ["Physics", true],
                ["Logic", false],
                ["Render", false],
                ["General", false],
              ].map(([t, on]) => (
                <button key={t} className={on ? "btn btn-primary" : "btn"} style={{ flex: 1, height: 26, fontSize: 11 }}>{t}</button>
              ))}
            </div>
          </FormRow>
          <FormRow label="Replicated">
            <label style={{ display: "flex", alignItems: "center", gap: 8, fontSize: 12, color: "var(--fg-muted)" }}>
              <input type="checkbox" defaultChecked style={{ accentColor: "var(--yellow)" }}/>
              StateCorrection writes this component
            </label>
          </FormRow>

          <div style={{ borderTop: "1px solid var(--border-soft)", paddingTop: 12 }}>
            <div className="mono" style={{ fontSize: 10, letterSpacing: "0.08em",
                                            color: "var(--fg-dim)", marginBottom: 8, textTransform: "uppercase" }}>
              Fields
            </div>
            <FieldEditor name="vX" type="Float32"/>
            <FieldEditor name="vY" type="Float32"/>
            <FieldEditor name="vZ" type="Float32"/>
            <FieldEditor name="speedSq" type="Float32" deriv/>
            <button className="btn btn-sm" style={{ marginTop: 6 }}>
              <Icon name="plus" size={11}/>Add field
            </button>
          </div>

          <div style={{ borderTop: "1px solid var(--border-soft)", paddingTop: 12,
                        display: "flex", gap: 6 }}>
            <button className="btn">Preview</button>
            <button className="btn btn-primary" style={{ marginLeft: "auto" }}>
              <Icon name="save" size={12}/>Emit CVelocity.hpp
            </button>
          </div>
        </div>
      </div>

      {/* Code preview */}
      <div className="panel">
        <div className="panel-head">
          <Icon name="code" size={12} color="var(--fg-muted)"/>
          <span className="title">CVelocity.hpp · live preview</span>
          <span className="chip chip-good" style={{ marginLeft: "auto" }}>schema valid</span>
        </div>
        <div style={{ padding: "12px 14px", fontFamily: "var(--f-mono)", fontSize: 11.5,
                      lineHeight: 1.6, overflow: "auto", flex: 1 }}>
          <CodeLine n={1}><K>#pragma once</K></CodeLine>
          <CodeLine n={2}><K>#include</K> {`"`}<I>tnx/component_view.hpp</I>{`"`}</CodeLine>
          <CodeLine n={3}>{" "}</CodeLine>
          <CodeLine n={4}><K>template</K> &lt;<K>FieldWidth</K> WIDTH = FieldWidth::Scalar&gt;</CodeLine>
          <CodeLine n={5}><K>struct</K> <I>CVelocity</I> : <F>ComponentView</F>&lt;<I>CVelocity</I>, WIDTH&gt;</CodeLine>
          <CodeLine n={6}>{`{`}</CodeLine>
          <CodeLine n={7}>{"  "}<C>// tier=Temporal → emits the Temporal macro</C></CodeLine>
          <CodeLine n={8} hl>{"  "}<F>TNX_TEMPORAL_FIELDS</F>(<I>CVelocity</I>, <K>Physics</K>, vX, vY, vZ)</CodeLine>
          <CodeLine n={9}>{" "}</CodeLine>
          <CodeLine n={10}>{"  "}<F>FloatProxy</F>&lt;WIDTH&gt; vX, vY, vZ;</CodeLine>
          <CodeLine n={11}>{" "}</CodeLine>
          <CodeLine n={12}>{"  "}<C>// derived field — not in slab, computed on read</C></CodeLine>
          <CodeLine n={13}>{"  "}<K>auto</K> <F>speedSq</F>() <K>const</K> {`{`} <K>return</K> vX*vX + vY*vY + vZ*vZ; {`}`}</CodeLine>
          <CodeLine n={14}>{"};"}</CodeLine>
          <CodeLine n={15}><F>TNX_REGISTER_COMPONENT</F>(<I>CVelocity</I>)</CodeLine>
          <CodeLine n={16}><F>TNX_NET_REPLICATED</F>(<I>CVelocity</I>)  <C>// → ReplicationSystem walks this</C></CodeLine>
        </div>
        <div style={{ padding: "8px 14px", borderTop: "1px solid var(--border-soft)",
                      fontFamily: "var(--f-mono)", fontSize: 10.5, color: "var(--fg-dim)",
                      display: "flex", gap: 10 }}>
          <span>writes <span style={{ color: "var(--yellow)" }}>src/components/CVelocity.hpp</span></span>
          <span>·</span>
          <span>registers in <span style={{ color: "var(--yellow)" }}>component_registry.cpp</span></span>
          <span style={{ marginLeft: "auto" }}>schema v4</span>
        </div>
      </div>
    </div>
  </div>
);

const FormRow = ({ label, children }) => (
  <div>
    <div className="mono" style={{ fontSize: 10, letterSpacing: "0.08em", color: "var(--fg-dim)",
                                    textTransform: "uppercase", marginBottom: 6 }}>{label}</div>
    {children}
  </div>
);

const FieldEditor = ({ name, type, deriv }) => (
  <div style={{ display: "flex", gap: 6, alignItems: "center", padding: "3px 0" }}>
    <div className="field-input" style={{ flex: 1.4 }}>
      <span className="val mono">{name}</span>
    </div>
    <div className="field-input" style={{ flex: 1 }}>
      <span className="val mono" style={{ color: "var(--purple-hot)" }}>{type}</span>
      <Icon name="chevD" size={9} color="var(--fg-dim)" style={{ marginLeft: "auto" }}/>
    </div>
    {deriv && <span className="chip mono" style={{ fontSize: 9 }}>derived</span>}
    <button className="btn btn-icon btn-sm btn-ghost"><Icon name="x" size={10}/></button>
  </div>
);

// ── 9. Job Graph Visualizer — Profile workspace centerpiece ──────────────
const IdeaJobGraph = () => (
  <div className="tnx-root" style={{ height: "100%", background: "var(--bg-app)",
                                     padding: 28, display: "flex", flexDirection: "column", gap: 16 }}>
    <Header2
      kicker="09 · PROFILE WORKSPACE"
      title="Job graph — see the critical path"
      sub="The Brain's per-chunk job dispatch as a dependency DAG. The critical path glows. If a Construct's tick registration is forcing a thread sync, the bubble is visible."
    />

    <div style={{ flex: 1, display: "grid", gridTemplateColumns: "1fr 320px", gap: 16, minHeight: 0 }}>
      {/* Graph */}
      <div style={{ background: "var(--bg-deep)", border: "1px solid var(--border)",
                    borderRadius: 3, padding: 16, position: "relative", overflow: "hidden" }}>
        <svg viewBox="0 0 700 380" preserveAspectRatio="xMidYMid meet"
             style={{ width: "100%", height: "100%" }}>
          {/* Lane labels */}
          <text x="10" y="40"  fontFamily="var(--f-mono)" fontSize="11" fill="var(--th-brain)" fontWeight="700">BRAIN</text>
          <text x="10" y="140" fontFamily="var(--f-mono)" fontSize="11" fill="var(--tier-volatile)" fontWeight="700">PHYS WORKERS</text>
          <text x="10" y="240" fontFamily="var(--f-mono)" fontSize="11" fill="var(--good)" fontWeight="700">LOGIC WORKERS</text>
          <text x="10" y="340" fontFamily="var(--f-mono)" fontSize="11" fill="var(--th-encoder)" fontWeight="700">ENCODER</text>

          <line x1="0" y1="85" x2="700" y2="85" stroke="var(--border-soft)"/>
          <line x1="0" y1="185" x2="700" y2="185" stroke="var(--border-soft)"/>
          <line x1="0" y1="285" x2="700" y2="285" stroke="var(--border-soft)"/>

          {/* Brain — coordinator */}
          <JobBox x={90} y={20} w={70} title="dispatch" sub="prePhys" color="var(--th-brain)" critical/>
          <JobBox x={290} y={20} w={90} title="WaitForCounter" sub="steal" color="var(--th-brain)" />
          <JobBox x={460} y={20} w={70} title="dispatch" sub="postPhys" color="var(--th-brain)" critical/>

          {/* Phys workers (per-chunk) */}
          {Array.from({ length: 6 }).map((_, i) => (
            <JobBox key={i} x={170 + i * 22} y={110 + (i % 3) * 12} w={20}
                    title="" sub="" color="var(--tier-volatile)" tiny critical={i < 3}/>
          ))}
          <JobBox x={310} y={110} w={140} title="Jolt step" sub="105µs" color="var(--tier-volatile)" critical/>

          {/* Logic workers */}
          {Array.from({ length: 8 }).map((_, i) => (
            <JobBox key={i} x={170 + i * 18} y={210 + (i % 4) * 9} w={16}
                    title="" sub="" color="var(--good)" tiny/>
          ))}

          {/* Encoder */}
          <JobBox x={540} y={310} w={120} title="GPU upload" sub="dirty bitplanes" color="var(--th-encoder)" critical/>

          {/* Critical path overlay */}
          <path d="M 160 30 L 200 110 L 310 130 L 460 130 L 530 30 L 600 320"
                fill="none" stroke="var(--yellow)" strokeWidth="2" strokeDasharray="4 3" opacity="0.85"/>
          <text x="600" y="305" fontFamily="var(--f-mono)" fontSize="10.5" fill="var(--yellow)" fontWeight="700">
            critical path · 1.42ms
          </text>

          {/* Bubble warning */}
          <g>
            <rect x="385" y="60" width="60" height="22" rx="3"
                  fill="oklch(0.25 0.12 75 / 0.6)" stroke="var(--warn)" strokeWidth="1"/>
            <text x="415" y="75" fontFamily="var(--f-mono)" fontSize="9.5" fill="var(--warn)"
                  textAnchor="middle" fontWeight="700">BUBBLE 84µs</text>
          </g>
        </svg>
      </div>

      {/* Side: explanations + actions */}
      <div style={{ display: "flex", flexDirection: "column", gap: 10 }}>
        <div className="panel">
          <div className="panel-head"><span className="title">This frame</span></div>
          <div style={{ padding: "10px 12px", display: "flex", flexDirection: "column", gap: 6,
                        fontFamily: "var(--f-mono)", fontSize: 11 }}>
            <KV k="brain"        v="0.73 ms"  c="var(--good)"/>
            <KV k="phys workers" v="0.51 ms"  c="var(--good)"/>
            <KV k="logic workers" v="0.32 ms" c="var(--good)"/>
            <KV k="encoder"      v="0.88 ms"  c="var(--good)"/>
            <div style={{ height: 1, background: "var(--border-soft)", margin: "4px 0" }}/>
            <KV k="critical path" v="1.42 ms · ok" c="var(--good)"/>
            <KV k="bubble"        v="84 µs · phys idle" c="var(--warn)"/>
          </div>
        </div>

        <div style={{ background: "var(--purple-wash)", border: "1px solid var(--purple-soft)",
                      padding: "10px 12px", borderRadius: 3, fontSize: 12 }}>
          <div style={{ color: "var(--yellow)", fontWeight: 600, marginBottom: 4 }}>
            Brain dispatch ↦ Jolt step
          </div>
          <span style={{ color: "var(--fg-muted)" }}>
            Most phys workers ran out of chunks 84µs before Jolt. Looks like a Construct
            with bespoke PrePhysics is forcing a sync. Open <span className="mono" style={{ color: "var(--fg)" }}>tick registrations →</span>
          </span>
        </div>

        <div style={{ display: "flex", flexDirection: "column", gap: 6 }}>
          <button className="btn btn-sm"><Icon name="flask" size={11}/>Open in Tracy</button>
          <button className="btn btn-sm">Record 60 frames</button>
          <button className="btn btn-sm">Filter to bubble path</button>
        </div>
      </div>
    </div>
  </div>
);

const JobBox = ({ x, y, w, title, sub, color, critical, tiny }) => {
  const h = tiny ? 8 : 30;
  return (
    <g>
      <rect x={x} y={y} width={w} height={h} rx="2"
            fill={color} opacity={critical ? 0.95 : 0.6}
            stroke={critical ? "var(--yellow)" : "transparent"} strokeWidth={critical ? 1 : 0}/>
      {!tiny && (
        <>
          <text x={x + 5} y={y + 12} fontFamily="var(--f-mono)" fontSize="10" fill="white" fontWeight="600">{title}</text>
          <text x={x + 5} y={y + 25} fontFamily="var(--f-mono)" fontSize="9" fill="oklch(0.15 0.02 285 / 0.85)">{sub}</text>
        </>
      )}
    </g>
  );
};

// ── Header (local — avoid name collision with foundation Header) ─────────
const Header2 = ({ kicker, title, sub }) => (
  <div>
    <div className="mono" style={{ fontSize: 10.5, letterSpacing: "0.16em",
                                    color: "var(--yellow)", marginBottom: 8 }}>{kicker}</div>
    <div style={{ fontFamily: "var(--f-display)", fontSize: 28, fontWeight: 600,
                  letterSpacing: "-0.02em", lineHeight: 1.1 }}>{title}</div>
    <div style={{ fontSize: 13, color: "var(--fg-muted)", marginTop: 6, maxWidth: 800, lineHeight: 1.5 }}>{sub}</div>
  </div>
);

Object.assign(window, { IdeaSlabHeatmap, IdeaVisualScript, IdeaComponentGen, IdeaJobGraph });
