/* global React, Icon, Tier, Spark, LogoMark */

// Novel UX ideas — each artboard is a focused Trinyx-specific idea that
// would be hard to find in other engines. Sized to display the idea
// clearly; they're meant to be discussed individually and merged into the
// chosen layout where they fit.

// ── 1. Command Palette — overlay on populated editor ────────────────────
// The palette isn't the primary surface — it sits ON TOP of the editor.
// New devs still see all the panels they expect; power users summon it
// without leaving where they are.
const IdeaCommandPalette = () => (
  <div className="tnx-root" style={{ height: "100%", background: "var(--bg-app)",
                                     padding: 28, display: "flex", flexDirection: "column", gap: 14 }}>
    <Header
      kicker="01 · NAVIGATION"
      title="Command palette, in context"
      sub="⌘K opens on top of the live editor — never leaves a beginner staring at a blank surface. Same affordance, same syntax, whether you're 1 hour or 1,000 hours in."
    />

    <div style={{ flex: 1, position: "relative", border: "1px solid var(--border)",
                  borderRadius: 4, overflow: "hidden", background: "var(--bg-deep)" }}>
      {/* Live editor in the background, slightly dimmed via overlay */}
      <div style={{ position: "absolute", inset: 0 }}>
        <window.HiFiEditor/>
      </div>
      {/* Dim layer */}
      <div style={{ position: "absolute", inset: 0,
                    background: "oklch(0.10 0.02 285 / 0.55)",
                    backdropFilter: "blur(2px)" }}/>

      {/* Palette */}
      <div style={{
        position: "absolute", left: "50%", top: "12%", transform: "translateX(-50%)",
        width: 620,
        background: "var(--bg-panel)", border: "1px solid var(--purple-soft)",
        borderRadius: 6, overflow: "hidden",
        boxShadow: "0 24px 60px oklch(0.05 0.04 295 / 0.7), 0 0 0 1px var(--purple-faint), 0 0 80px var(--purple-wash)"
      }}>
        <div style={{ display: "flex", alignItems: "center", gap: 10, padding: "12px 16px",
                      borderBottom: "1px solid var(--border-soft)" }}>
          <Icon name="cmd" size={14} color="var(--purple-hot)"/>
          <span style={{ fontFamily: "var(--f-mono)", fontSize: 14, color: "var(--fg)" }}>
            spawn pyramid 25 layers cold
          </span>
          <span style={{ width: 2, height: 16, background: "var(--purple-hot)",
                         animation: "blink 1s steps(2) infinite", marginLeft: 1 }}/>
          <span style={{ marginLeft: "auto", display: "flex", gap: 4 }}>
            <span className="kbd">esc</span>
          </span>
        </div>

        {/* Top match */}
        <div style={{ padding: "10px 16px", background: "var(--purple-wash)",
                      borderLeft: "2px solid var(--purple)" }}>
          <div style={{ display: "flex", alignItems: "center", gap: 8 }}>
            <Icon name="cube" size={13} color="var(--yellow)"/>
            <span style={{ fontWeight: 600 }}>Spawn 25-layer pyramid</span>
            <Tier kind="Cold"/>
            <span className="mono" style={{ fontSize: 10.5, color: "var(--fg-muted)" }}>archetype ECubeStatic · 5,525 ent · 2.1 MB</span>
            <span style={{ marginLeft: "auto" }}><span className="kbd">↵</span></span>
          </div>
          <div style={{ fontSize: 11, color: "var(--fg-muted)", marginTop: 4, marginLeft: 21 }}>
            scene · creates a stable test pyramid in the cold tier · benchmark fixture
          </div>
        </div>

        {[
          { i: "cube",     t: "Spawn pyramid (15 layers)",          tier: "Cold",     m: "scene · 1,240 ent · 480 KB" },
          { i: "spark",    t: "Spawn 100,000 ambient cubes",        tier: "Volatile", m: "scene · stress fixture · 38 MB" },
          { i: "flask",    t: "Run benchmark · 100k entities",      tier: null,       m: "→ Profile workspace · record 30s" },
          { i: "network",  t: "Switch to PIE loopback (2 owners)",  tier: null,       m: "→ Network workspace · 1 auth + 2 owner viewports" },
          { i: "save",     t: "Save scene as 'Pyramid_25.tnxscene'",tier: null,       m: "file · overwrite · ⌘S" },
          { i: "code",     t: "Open Player.cpp at PrePhysics()",    tier: null,       m: "external editor (clangd, ts-server, etc.)" },
        ].map((r, idx) => (
          <div key={idx} style={{ padding: "8px 16px", display: "flex", alignItems: "center", gap: 8,
                                  borderTop: "1px solid var(--border-soft)" }}>
            <Icon name={r.i} size={13} color="var(--fg-muted)"/>
            <span style={{ fontSize: 12.5 }}>{r.t}</span>
            {r.tier && <Tier kind={r.tier}/>}
            <span className="mono" style={{ marginLeft: "auto", fontSize: 10.5, color: "var(--fg-dim)" }}>{r.m}</span>
          </div>
        ))}

        <div style={{ display: "flex", padding: "8px 16px", borderTop: "1px solid var(--border)",
                      background: "var(--bg-deep)", fontSize: 10.5, color: "var(--fg-muted)",
                      fontFamily: "var(--f-mono)" }}>
          <span>parses natural args · "25 layers cold" → layers=25, tier=Cold</span>
          <span style={{ marginLeft: "auto" }}><span className="kbd">↑↓</span> nav · <span className="kbd">⇥</span> param edit</span>
        </div>
      </div>

      {/* Annotations pointing to editor chrome */}
      <div style={{
        position: "absolute", left: 16, bottom: 80,
        background: "var(--bg-deep)", border: "1px solid var(--yellow-soft)",
        padding: "8px 12px", borderRadius: 3, maxWidth: 280,
        fontSize: 11.5, color: "var(--fg)",
      }}>
        <div style={{ color: "var(--yellow)", fontWeight: 600, marginBottom: 4 }}>Editor stays visible</div>
        Beginners see the panels they're learning. Experts use the same shortcut from any panel.
      </div>
    </div>
  </div>
);

// ── 2. Partition-aware Inspector ─────────────────────────────────────────
// Corrected model: Tier + net live on components. Queue tag lives on the
// entity (entity header). Field rows stay clean — just label + value.
const IdeaPartitionInspector = () => (
  <div className="tnx-root" style={{ height: "100%", background: "var(--bg-app)",
                                     padding: 28, display: "flex", flexDirection: "column", gap: 16 }}>
    <Header
      kicker="02 · INSPECTOR"
      title="Three layers, three badges"
      sub="Entity owns the queue. Component owns the tier + net flag. Field is just a value. Read top-down: the badges narrow as you go, the meaning sharpens."
    />

    <div style={{ display: "grid", gridTemplateColumns: "1fr 1fr", gap: 24, alignItems: "start" }}>
      {/* Annotated mock — one selection, three badge tiers */}
      <div className="panel" style={{ width: "100%" }}>
        <div className="panel-head">
          <span className="title">Inspector — annotated</span>
        </div>

        {/* Entity-level header */}
        <div style={{ padding: "12px 14px", borderBottom: "1px solid var(--border-soft)",
                      background: "var(--bg-app)" }}>
          <div style={{ display: "flex", alignItems: "center", gap: 8, marginBottom: 6 }}>
            <div style={{ fontFamily: "var(--f-display)", fontSize: 16, fontWeight: 600 }}>Player.Body</div>
            <span style={{ fontFamily: "var(--f-mono)", fontSize: 10.5, color: "var(--yellow)",
                           marginLeft: 6 }}>↳ ENTITY</span>
          </div>
          <div style={{ display: "flex", gap: 6, alignItems: "center", flexWrap: "wrap" }}>
            <span className="chip mono">EView&lt;EPlayer&gt;</span>
            <span className="chip mono" style={{ color: "var(--fg-muted)" }}>idx 0x00A4F2</span>
            <span className="chip chip-purple mono">queue: Physics</span>
          </div>
          <div style={{ fontSize: 10.5, fontFamily: "var(--f-mono)", color: "var(--yellow)",
                        marginTop: 6, paddingLeft: 4, borderLeft: "2px solid var(--yellow)" }}>
            entity-level: queue tag (Physics / Logic / Render / General)
          </div>
        </div>

        {/* Component with tier + net */}
        <div style={{ borderBottom: "1px solid var(--border-soft)" }}>
          <div style={{ padding: "8px 14px 6px", background: "var(--bg-app)",
                        display: "flex", alignItems: "center", gap: 8 }}>
            <Icon name="chevD" size={9} color="var(--fg-muted)"/>
            <span style={{ fontFamily: "var(--f-display)", fontWeight: 600, fontSize: 13 }}>CTransform</span>
            <Tier kind="Temporal"/>
            <span className="chip chip-yellow mono" style={{ fontSize: 9 }}>net</span>
            <span style={{ fontFamily: "var(--f-mono)", fontSize: 10.5, color: "var(--purple-hot)",
                           marginLeft: "auto" }}>↳ COMPONENT</span>
          </div>
          <div style={{ padding: "4px 0 6px" }}>
            <div className="field-row">
              <div className="lbl">Position</div>
              <div className="vec3">
                <div className="field-input"><span className="axis axis-x">X</span><span className="val mono">12.483</span></div>
                <div className="field-input"><span className="axis axis-y">Y</span><span className="val mono">0.000</span></div>
                <div className="field-input"><span className="axis axis-z">Z</span><span className="val mono">−4.220</span></div>
              </div>
            </div>
            <div className="field-row">
              <div className="lbl">Velocity</div>
              <div className="vec3">
                <div className="field-input"><span className="axis axis-x">X</span><span className="val mono">0.000</span></div>
                <div className="field-input"><span className="axis axis-y">Y</span><span className="val mono">0.000</span></div>
                <div className="field-input"><span className="axis axis-z">Z</span><span className="val mono">0.000</span></div>
              </div>
            </div>
          </div>
          <div style={{ fontSize: 10.5, fontFamily: "var(--f-mono)", color: "var(--purple-hot)",
                        margin: "0 14px 10px", paddingLeft: 4, borderLeft: "2px solid var(--purple-hot)" }}>
            component-level: tier (Cold/Static/Volatile/Temporal) + net flag
          </div>
        </div>

        {/* Component — different tier, no net */}
        <div style={{ borderBottom: "1px solid var(--border-soft)" }}>
          <div style={{ padding: "8px 14px 6px", background: "var(--bg-app)",
                        display: "flex", alignItems: "center", gap: 8 }}>
            <Icon name="chevD" size={9} color="var(--fg-muted)"/>
            <span style={{ fontFamily: "var(--f-display)", fontWeight: 600, fontSize: 13 }}>CRender</span>
            <Tier kind="Volatile"/>
          </div>
          <div style={{ padding: "4px 0 8px" }}>
            <div className="field-row">
              <div className="lbl">MeshID</div>
              <div className="field-input"><span className="val mono">char_player</span></div>
            </div>
            <div className="field-row">
              <div className="lbl">Tint</div>
              <div className="field-input">
                <span style={{ width: 12, height: 12, background: "oklch(0.85 0.16 92)", borderRadius: 2, marginRight: 6 }}/>
                <span className="val mono">#F2C94C</span>
              </div>
            </div>
          </div>
        </div>

        {/* Field-level legend */}
        <div style={{ padding: "10px 14px" }}>
          <div style={{ fontSize: 10.5, fontFamily: "var(--f-mono)", color: "var(--fg-dim)",
                        paddingLeft: 4, borderLeft: "2px solid var(--fg-dim)" }}>
            field-level: just the value. no metadata on individual fields — they inherit.
          </div>
        </div>
      </div>

      {/* Legend */}
      <div style={{ display: "flex", flexDirection: "column", gap: 14 }}>
        <Legend>
          <div className="mono" style={{ fontSize: 10, letterSpacing: "0.1em",
                                          color: "var(--fg-dim)", marginBottom: 8 }}>ENTITY · QUEUE</div>
          <LegendRow swatch={<span className="chip chip-purple mono">queue: Physics</span>}
                     text="PrePhysics/PostPhysics — Brain dispatches per-chunk to workers"/>
          <LegendRow swatch={<span className="chip chip-purple mono">queue: Logic</span>}
                     text="ScalarUpdate — Brain thread, post physics step"/>
          <LegendRow swatch={<span className="chip chip-purple mono">queue: Render</span>}
                     text="Encoder thread — GPU upload + compute dispatch"/>
          <LegendRow swatch={<span className="chip chip-purple mono">queue: General</span>}
                     text="Catch-all + overflow when queues are full"/>
        </Legend>

        <Legend>
          <div className="mono" style={{ fontSize: 10, letterSpacing: "0.1em",
                                          color: "var(--fg-dim)", marginBottom: 8 }}>COMPONENT · TIER</div>
          <LegendRow swatch={<Tier kind="Cold"/>}     text="Archetype chunks, AoS, rarely updated"/>
          <LegendRow swatch={<Tier kind="Static"/>}   text="Read-only, never changes (geometry)"/>
          <LegendRow swatch={<Tier kind="Volatile"/>} text="SoA ring, 3 frames, no rollback"/>
          <LegendRow swatch={<Tier kind="Temporal"/>} text="SoA ring, max(8,X) frames, rollback"/>
        </Legend>

        <Legend>
          <div className="mono" style={{ fontSize: 10, letterSpacing: "0.1em",
                                          color: "var(--fg-dim)", marginBottom: 8 }}>COMPONENT · NET</div>
          <LegendRow swatch={<span className="chip chip-yellow mono">net</span>}
                     text="StateCorrection writes this component each net tick"/>
        </Legend>

        <div style={{ background: "var(--purple-wash)", border: "1px solid var(--purple-soft)",
                      padding: "10px 12px", borderRadius: 4, fontSize: 12, color: "var(--fg)" }}>
          <strong style={{ color: "var(--yellow)" }}>Why this matters →</strong> Selecting Player.Body
          you instantly read: "physics-queue entity, transform is rollback-able and replicated, render
          is cosmetic and local-only". Three glances, full picture.
        </div>
      </div>
    </div>
  </div>
);

const Legend = ({ children }) => (
  <div style={{ background: "var(--bg-panel)", border: "1px solid var(--border)",
                borderRadius: 4, padding: "10px 12px" }}>
    {children}
  </div>
);

const LegendRow = ({ swatch, text }) => (
  <div style={{ display: "flex", alignItems: "center", gap: 10, padding: "3px 0" }}>
    <div style={{ width: 110, display: "flex", justifyContent: "flex-start" }}>{swatch}</div>
    <div style={{ fontSize: 11.5, color: "var(--fg-muted)" }}>{text}</div>
  </div>
);

// ── 3. Rollback / Replay Scrubber ────────────────────────────────────────
const IdeaScrubber = () => {
  // synthesize a fake frame-budget histogram
  const frames = Array.from({ length: 90 }).map((_, i) => {
    const base = 0.7 + Math.sin(i * 0.4) * 0.1;
    const spike = (i === 32 || i === 33 || i === 67) ? 1.2 : 0;
    return base + Math.random() * 0.15 + spike;
  });
  const max = Math.max(...frames);
  const playhead = 70;
  return (
    <div className="tnx-root" style={{ height: "100%", background: "var(--bg-app)",
                                       padding: 28, display: "flex", flexDirection: "column", gap: 16 }}>
      <Header
        kicker="03 · REPLAY"
        title="Slab-based rollback scrubber"
        sub="The temporal ring buffer is already a free recording. Scrub backwards, scrub a single field, branch the timeline from frame N — the architecture supports it without extra cost."
      />

      <div style={{ background: "var(--bg-panel)", border: "1px solid var(--border)",
                    borderRadius: 4, padding: 16 }}>
        {/* header */}
        <div style={{ display: "flex", alignItems: "center", gap: 12, marginBottom: 14 }}>
          <button className="btn"><Icon name="chevL" size={12}/></button>
          <button className="btn btn-primary"><Icon name="play" size={12}/>Resume</button>
          <button className="btn"><Icon name="step" size={12}/></button>
          <button className="btn"><Icon name="chevR" size={12}/></button>
          <div style={{ marginLeft: 6, display: "flex", flexDirection: "column" }}>
            <div className="mono" style={{ fontSize: 18, color: "var(--fg)", lineHeight: 1 }}>
              f / <span style={{ color: "var(--yellow)" }}>18,442</span>
            </div>
            <div className="mono" style={{ fontSize: 10.5, color: "var(--fg-dim)" }}>
              512Hz · sim 36.02s · live 36.04s · drift −2 frames
            </div>
          </div>
          <div style={{ marginLeft: "auto", display: "flex", gap: 6 }}>
            <span className="chip chip-yellow">● recording</span>
            <span className="chip mono">ring 8,192 frames · 16.0s</span>
            <span className="chip mono">slab 24 MB / frame</span>
          </div>
        </div>

        {/* Track: frame budget bars + playhead */}
        <div style={{ position: "relative", height: 88, background: "var(--bg-deep)",
                      border: "1px solid var(--border-soft)", borderRadius: 3, overflow: "hidden" }}>
          {/* budget line at 1.95ms */}
          <div style={{ position: "absolute", left: 0, right: 0, top: `${100 - (1.95 / max) * 80}%`,
                        height: 1, borderTop: "1px dashed var(--yellow-soft)", opacity: 0.6 }}/>
          <div style={{ position: "absolute", right: 6, top: `${100 - (1.95 / max) * 80}%`,
                        transform: "translateY(-100%)", fontFamily: "var(--f-mono)",
                        fontSize: 9.5, color: "var(--yellow-soft)" }}>budget 1.95ms</div>

          {/* bars */}
          <div style={{ position: "absolute", inset: 0, display: "flex", alignItems: "flex-end" }}>
            {frames.map((v, i) => (
              <div key={i} style={{
                flex: 1, height: `${(v / max) * 85}%`,
                background: v > 1.95 ? "var(--bad)" : v > 1 ? "var(--warn)" : "var(--th-brain)",
                marginRight: 1, opacity: i === playhead ? 1 : 0.7
              }}/>
            ))}
          </div>
          {/* playhead */}
          <div style={{ position: "absolute", left: `${(playhead / frames.length) * 100}%`,
                        top: 0, bottom: 0, width: 2, background: "var(--yellow)",
                        boxShadow: "0 0 8px var(--yellow)" }}/>
          <div style={{ position: "absolute", left: `${(playhead / frames.length) * 100}%`,
                        top: -2, transform: "translate(-50%, -100%)",
                        background: "var(--yellow)", color: "oklch(0.20 0.02 90)",
                        fontFamily: "var(--f-mono)", fontSize: 9.5, fontWeight: 700,
                        padding: "1px 5px", borderRadius: 2 }}>
            f 18,372
          </div>

          {/* event markers */}
          <Marker x={22} kind="net" label="state correction"/>
          <Marker x={50} kind="bug" label="vY non-finite"/>
          <Marker x={70} kind="spawn" label="20 cubes spawned"/>
        </div>

        {/* Field-level mini timelines below */}
        <div style={{ marginTop: 14, display: "flex", flexDirection: "column", gap: 6 }}>
          {[
            ["Player.PosX",   "var(--purple-hot)", [12.5,12.5,12.5,12.5,12.4,12.3,12.2,12.1,12.0,12.0,12.0,12.1,12.3,12.5,12.6,12.8,13.0,13.2,13.4,13.5,13.6]],
            ["Player.vY",     "var(--bad)",         [0,0,0,0,0,0,0,1,3,5,2,0,0,0,0,0,0,0,0,0,0]],
            ["Health",        "var(--good)",        [100,100,100,100,100,100,100,100,90,90,90,90,90,90,90,90,90,90,90,90,90]],
          ].map(([n, c, d]) => (
            <div key={n} style={{ display: "flex", alignItems: "center", gap: 10,
                                  padding: "4px 8px", background: "var(--bg-app)",
                                  border: "1px solid var(--border-soft)", borderRadius: 2 }}>
              <span className="mono" style={{ width: 130, fontSize: 11, color: "var(--fg-muted)" }}>{n}</span>
              <div style={{ flex: 1, height: 22 }}>
                <Spark data={d} color={c} width={500} height={22} fill/>
              </div>
              <span className="mono" style={{ width: 70, fontSize: 11, textAlign: "right" }}>
                {d[d.length - 1]}
              </span>
            </div>
          ))}
        </div>

        {/* Actions */}
        <div style={{ display: "flex", gap: 8, marginTop: 14 }}>
          <button className="btn btn-sm"><Icon name="flask" size={11}/>Branch from f 18,372</button>
          <button className="btn btn-sm"><Icon name="save" size={11}/>Export 60 frames as .replay</button>
          <button className="btn btn-sm btn-ghost">Compare to server timeline →</button>
        </div>
      </div>
    </div>
  );
};

const Marker = ({ x, kind, label }) => {
  const color = kind === "bug" ? "var(--bad)" : kind === "net" ? "var(--info)" : "var(--yellow-soft)";
  return (
    <div style={{ position: "absolute", left: `${x}%`, top: 4, bottom: 4,
                  display: "flex", flexDirection: "column", alignItems: "center" }}>
      <div style={{ width: 1, height: "100%", background: color, opacity: 0.5 }}/>
      <div style={{ position: "absolute", top: 2, background: color, color: "white",
                    fontFamily: "var(--f-mono)", fontSize: 8.5, padding: "1px 5px",
                    borderRadius: 2, whiteSpace: "nowrap", transform: "translateX(4px)" }}>
        {label}
      </div>
    </div>
  );
};

// ── 4. PIE — Multi-window + Network Condition Simulator ─────────────────
// PIE is multi-window like Unreal: 1 Authority world + N Owner worlds,
// each with its own WorldViewport. The Network workspace tiles them with
// live stats + a Condition Simulator that lets you inject latency, loss,
// and jitter on the fly to stress-test ReplicationSystem / rollback.
const IdeaNetTopology = () => (
  <div className="tnx-root" style={{ height: "100%", background: "var(--bg-app)",
                                     padding: 28, display: "flex", flexDirection: "column", gap: 16 }}>
    <Header
      kicker="04 · NETWORK"
      title="Multi-window PIE, with a knob for chaos"
      sub="One Authority and up to four Owner worlds, each in their own viewport tile. Inject latency, packet loss, jitter live — see exactly when ReplicationSystem starts forcing corrections."
    />

    {/* Viewport tiles */}
    <div style={{ flex: 1, display: "grid", gridTemplateColumns: "2fr 1fr", gap: 16 }}>
      <div style={{ display: "grid", gridTemplateColumns: "1fr 1fr", gridTemplateRows: "1fr 1fr",
                    gap: 8 }}>
        <PIETile role="AUTHORITY" world="World #0" rtt="—"     fps="512Hz" big drops={0}/>
        <PIETile role="OWNER A"   world="P1 · loopback" rtt="24 ms" fps="144Hz" drops={0}/>
        <PIETile role="OWNER B"   world="P2 · loopback" rtt="31 ms" fps="144Hz" drops={2} warn/>
        <PIETile role="OWNER C"   world="P3 · loopback" rtt="18 ms" fps="144Hz" drops={0}/>
      </div>

      {/* Condition simulator + live stats */}
      <div style={{ display: "flex", flexDirection: "column", gap: 10, minWidth: 0 }}>
        <div className="panel">
          <div className="panel-head">
            <Icon name="flask" size={12} color="var(--yellow)"/>
            <span className="title">Condition simulator</span>
            <span style={{ marginLeft: "auto" }} className="chip chip-yellow">● active</span>
          </div>
          <div style={{ padding: "10px 14px", display: "flex", flexDirection: "column", gap: 12 }}>
            <NetSlider label="Latency"    value="120" unit="ms"   pct={0.3}  color="var(--purple-hot)"/>
            <NetSlider label="Jitter ±"   value="35"  unit="ms"   pct={0.25} color="var(--info)"/>
            <NetSlider label="Loss"       value="2.4" unit="%"    pct={0.12} color="var(--warn)"/>
            <NetSlider label="Duplicate"  value="0.1" unit="%"    pct={0.02} color="var(--fg-muted)"/>
            <NetSlider label="Reorder"    value="1.0" unit="%"    pct={0.05} color="var(--fg-muted)"/>

            <div style={{ display: "flex", gap: 6, marginTop: 6, flexWrap: "wrap" }}>
              <button className="btn btn-sm">Preset · LAN</button>
              <button className="btn btn-sm">Preset · 4G</button>
              <button className="btn btn-sm">Preset · Satellite</button>
              <button className="btn btn-sm btn-yellow">Drop next 3 corrections</button>
            </div>
          </div>
        </div>

        <Stat title="Replication queue"
              value="48"
              unit="entities"
              spark={[10,12,15,40,48,32,28,30,38,48,42,28]}
              color="var(--purple-hot)"/>
        <Stat title="Bandwidth"
              value="124"
              unit="KB/s · 64Hz tick"
              spark={[80,90,95,110,124,118,108,120,124,130,124]}
              color="var(--yellow)"/>
        <Stat title="StateCorrections"
              value="3.2 / s"
              unit="last 60s · ΔvsFull 18%"
              spark={[1,2,1,3,4,2,3,3,2,3,4,3]}
              color="var(--info)"/>
        <Stat title="Rollback events"
              value="0"
              unit="all owners converged"
              spark={[0,0,0,0,0,0,0,0,0,0]}
              color="var(--good)"/>
      </div>
    </div>
  </div>
);

const PIETile = ({ role, world, rtt, fps, big, drops, warn }) => (
  <div style={{
    background: "var(--bg-viewport)",
    border: `1px solid ${warn ? "var(--warn)" : "var(--border)"}`,
    borderRadius: 3, position: "relative", overflow: "hidden",
    display: "flex", flexDirection: "column",
  }}>
    {/* mini scene preview */}
    <div style={{
      position: "absolute", inset: 0,
      background: "radial-gradient(ellipse at 50% 60%, oklch(0.22 0.04 295) 0%, oklch(0.12 0.008 285) 70%)"
    }}/>
    <div style={{
      position: "absolute", left: "-25%", right: "-25%", bottom: 0, height: "55%",
      background:
        "linear-gradient(to bottom, transparent 0%, oklch(0.25 0.04 295 / 0.35) 100%)," +
        "repeating-linear-gradient(to right, oklch(0.45 0.06 295 / 0.45) 0 1px, transparent 1px 60px)," +
        "repeating-linear-gradient(to bottom, oklch(0.45 0.06 295 / 0.45) 0 1px, transparent 1px 50px)",
      transform: "perspective(600px) rotateX(58deg)",
      transformOrigin: "50% 100%",
      maskImage: "linear-gradient(to top, black 30%, transparent 100%)"
    }}/>
    <CubeIso style={{ left: "40%", top: "40%" }} size={big ? 50 : 32} hue={295}/>
    <CubeIso style={{ left: "55%", top: "48%" }} size={big ? 32 : 22} hue={92}/>

    {/* role banner */}
    <div style={{
      position: "relative", display: "flex", alignItems: "center", gap: 6,
      padding: "6px 10px", background: "oklch(0.12 0.02 285 / 0.65)",
      backdropFilter: "blur(4px)",
      borderBottom: warn ? "1px solid var(--warn)" : "1px solid var(--border-soft)"
    }}>
      <span style={{
        fontFamily: "var(--f-mono)", fontSize: 10, letterSpacing: "0.1em",
        color: role === "AUTHORITY" ? "var(--yellow)" : "var(--th-encoder)",
        fontWeight: 700
      }}>{role}</span>
      <span style={{ fontSize: 10.5, color: "var(--fg-muted)" }}>{world}</span>
      <span style={{ marginLeft: "auto", display: "flex", gap: 5 }}>
        {drops > 0 && (
          <span className="chip chip-yellow mono" style={{ fontSize: 9, height: 14, padding: "0 4px" }}>
            ⚠ {drops}
          </span>
        )}
      </span>
    </div>
    <div style={{ flex: 1 }}/>
    {/* footer stats */}
    <div style={{
      position: "relative", display: "flex", alignItems: "center", gap: 8,
      padding: "5px 10px", background: "oklch(0.10 0.02 285 / 0.7)",
      backdropFilter: "blur(4px)",
      fontFamily: "var(--f-mono)", fontSize: 9.5, color: "var(--fg-muted)"
    }}>
      <span>rtt {rtt}</span>
      <span>·</span>
      <span>{fps}</span>
      <span style={{ marginLeft: "auto", color: "var(--good)" }}>● live</span>
    </div>
  </div>
);

const NetSlider = ({ label, value, unit, pct, color }) => (
  <div>
    <div style={{ display: "flex", alignItems: "baseline", gap: 6, marginBottom: 4 }}>
      <span style={{ fontSize: 11, color: "var(--fg-muted)" }}>{label}</span>
      <span className="mono" style={{ marginLeft: "auto", fontSize: 11.5, color: "var(--fg)" }}>
        {value} <span style={{ color: "var(--fg-dim)" }}>{unit}</span>
      </span>
    </div>
    <div style={{ height: 6, background: "var(--bg-input)", borderRadius: 1,
                  position: "relative", border: "1px solid var(--border-soft)" }}>
      <div style={{ position: "absolute", left: 0, top: 0, bottom: 0,
                    width: `${pct * 100}%`, background: color, borderRadius: "1px 0 0 1px" }}/>
      <div style={{ position: "absolute", left: `${pct * 100}%`, top: -3, bottom: -3,
                    width: 8, marginLeft: -4, background: color, borderRadius: 1,
                    boxShadow: `0 0 6px ${color}` }}/>
    </div>
  </div>
);

const Stat = ({ title, value, unit, spark, color }) => (
  <div style={{ background: "var(--bg-panel)", border: "1px solid var(--border)",
                borderRadius: 3, padding: "10px 12px" }}>
    <div style={{ display: "flex", alignItems: "baseline", gap: 6, marginBottom: 4 }}>
      <span className="mono" style={{ fontSize: 10, color: "var(--fg-dim)",
                                       textTransform: "uppercase", letterSpacing: "0.08em" }}>{title}</span>
    </div>
    <div style={{ display: "flex", alignItems: "baseline", gap: 6 }}>
      <span style={{ fontFamily: "var(--f-display)", fontSize: 24, fontWeight: 600, color }}>{value}</span>
      <span className="mono" style={{ fontSize: 10.5, color: "var(--fg-dim)" }}>{unit}</span>
      <span style={{ marginLeft: "auto" }}><Spark data={spark} color={color} width={70} height={20} fill/></span>
    </div>
  </div>
);

// ── 5. Slab Inspector — see your data as a spreadsheet ───────────────────
const IdeaSlabView = () => (
  <div className="tnx-root" style={{ height: "100%", background: "var(--bg-app)",
                                     padding: 28, display: "flex", flexDirection: "column", gap: 16 }}>
    <Header
      kicker="05 · SLAB"
      title="The cache as a spreadsheet"
      sub="The engine's mental model is 'global cache + EntityCacheIndex'. The editor visualizes it directly — fields are rows, entities are columns, cells are values. Drag to multi-edit. The DoD's reality, not its abstraction."
    />

    <div style={{ background: "var(--bg-panel)", border: "1px solid var(--border)",
                  borderRadius: 4, padding: 0, overflow: "hidden", flex: 1, display: "flex", flexDirection: "column" }}>
      {/* Filter row */}
      <div style={{ display: "flex", alignItems: "center", gap: 8, padding: "8px 12px",
                    borderBottom: "1px solid var(--border-soft)" }}>
        <Icon name="grid" size={12} color="var(--fg-muted)"/>
        <span className="mono" style={{ fontSize: 11 }}>Slab</span>
        <span style={{ color: "var(--fg-dim)" }}>·</span>
        <span className="chip chip-purple">Pyramid_25</span>
        <span className="chip mono">5,525 entities</span>
        <span className="chip mono">Cold tier</span>
        <span style={{ marginLeft: "auto" }} className="mono"><span style={{ color: "var(--fg-dim)" }}>show columns</span> 0–24 / 5524</span>
      </div>

      {/* Spreadsheet */}
      <div style={{ flex: 1, overflow: "auto" }}>
        <table style={{ borderCollapse: "collapse", fontFamily: "var(--f-mono)",
                        fontSize: 10.5, width: "100%" }}>
          <thead>
            <tr style={{ background: "var(--bg-deep)" }}>
              <th style={cellStyle({ head: true, sticky: true })}>field</th>
              {Array.from({ length: 16 }).map((_, i) => (
                <th key={i} style={cellStyle({ head: true })}>
                  <div style={{ color: "var(--fg-dim)" }}>e{i.toString(16).padStart(2,"0").toUpperCase()}</div>
                </th>
              ))}
            </tr>
          </thead>
          <tbody>
            {[
              { f: "Transform.PosX", tier: "Cold", values: (i) => (i * 1.25 - 10).toFixed(2) },
              { f: "Transform.PosY", tier: "Cold", values: (i) => (Math.floor(i / 4) * 1.0).toFixed(2) },
              { f: "Transform.PosZ", tier: "Cold", values: (i) => (-Math.floor(i / 4) * 1.25).toFixed(2) },
              { f: "Mesh.ID",         tier: "Static", values: () => "cube_1m" },
              { f: "Tint.r",          tier: "Volatile", values: (i) => (0.5 + (i % 4) * 0.12).toFixed(2), hot: [4, 5] },
              { f: "Tint.g",          tier: "Volatile", values: (i) => (0.3 + (i % 4) * 0.10).toFixed(2), hot: [4, 5] },
              { f: "Tint.b",          tier: "Volatile", values: (i) => (0.1 + (i % 4) * 0.08).toFixed(2), hot: [4, 5] },
              { f: "Health",          tier: "Temporal", values: (i) => (i === 3 ? "0" : "100"), bad: (i) => i === 3, net: true },
              { f: "Active.flags",    tier: "Temporal", values: (i) => i === 3 ? "0x0000" : "0x8001", bad: (i) => i === 3 },
            ].map((row, ri) => (
              <tr key={ri}>
                <td style={cellStyle({ sticky: true, lbl: true })}>
                  <div style={{ display: "flex", alignItems: "center", gap: 5 }}>
                    <Tier kind={row.tier}/>
                    <span style={{ color: "var(--fg)" }}>{row.f}</span>
                    {row.net && <span className="chip chip-yellow mono" style={{ fontSize: 8.5, height: 14, padding: "0 4px" }}>net</span>}
                  </div>
                </td>
                {Array.from({ length: 16 }).map((_, i) => {
                  const v = row.values(i);
                  const isBad = row.bad && row.bad(i);
                  const isHot = row.hot && row.hot.includes(i);
                  const selected = i === 3;
                  return (
                    <td key={i} style={cellStyle({
                      val: v,
                      bad: isBad,
                      hot: isHot,
                      selected: selected && ri === 0,
                      selectedCol: selected,
                    })}>{v}</td>
                  );
                })}
              </tr>
            ))}
          </tbody>
        </table>
      </div>

      <div style={{ display: "flex", padding: "6px 12px", borderTop: "1px solid var(--border-soft)",
                    background: "var(--bg-deep)", fontFamily: "var(--f-mono)", fontSize: 10,
                    color: "var(--fg-muted)" }}>
        <span>entity 0x03 selected · Health = 0 (highlighted in red)</span>
        <span style={{ marginLeft: "auto" }}>⌥ drag column to multi-edit · ⌘F to find non-finite</span>
      </div>
    </div>
  </div>
);

function cellStyle({ head, sticky, lbl, val, bad, hot, selected, selectedCol }) {
  let bg = "transparent";
  let color = "var(--fg)";
  if (head) bg = "var(--bg-deep)";
  if (sticky && !lbl) bg = "var(--bg-deep)";
  if (lbl) { bg = "var(--bg-elev)"; }
  if (selected) { bg = "var(--purple)"; color = "white"; }
  else if (selectedCol) { bg = "var(--purple-wash)"; }
  if (bad && !selected) { color = "var(--bad)"; bg = "oklch(0.30 0.10 25 / 0.4)"; }
  if (hot && !selected) { color = "var(--yellow)"; }
  return {
    padding: "5px 10px",
    minWidth: head ? 64 : 64,
    textAlign: head ? "center" : "right",
    borderRight: "1px solid var(--border-soft)",
    borderBottom: "1px solid var(--border-soft)",
    background: bg,
    color,
    fontWeight: lbl ? 600 : 400,
    position: sticky ? "sticky" : "static",
    left: sticky ? 0 : "auto",
    zIndex: sticky ? 1 : 0,
    whiteSpace: "nowrap",
  };
}

// ── shared header ────────────────────────────────────────────────────────
const Header = ({ kicker, title, sub }) => (
  <div>
    <div className="mono" style={{ fontSize: 10.5, letterSpacing: "0.16em",
                                    color: "var(--yellow)", marginBottom: 8 }}>{kicker}</div>
    <div style={{ fontFamily: "var(--f-display)", fontSize: 28, fontWeight: 600,
                  letterSpacing: "-0.02em", lineHeight: 1.1 }}>{title}</div>
    <div style={{ fontSize: 13, color: "var(--fg-muted)", marginTop: 6, maxWidth: 800, lineHeight: 1.5 }}>{sub}</div>
  </div>
);

Object.assign(window, {
  IdeaCommandPalette, IdeaPartitionInspector, IdeaScrubber, IdeaNetTopology, IdeaSlabView
});
