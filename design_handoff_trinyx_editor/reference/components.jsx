/* global React */
// Trinyx shared editor primitives — icons + small reusable bits used across
// the hi-fi screen and the novel-UX artboards. Everything that lives here
// is intentionally generic; layout-specific composition is in hifi.jsx /
// ideas.jsx so this stays small.

// ── Icon set ────────────────────────────────────────────────────────────
// Original line iconography, 1.5px strokes, 16px grid. No emoji.
const Icon = ({ name, size = 14, color = "currentColor", style }) => {
  const paths = {
    select:    "M3 2.5l6.5 11 1.7-4.5 4.5-1.7L3 2.5z",
    move:      "M8 1.5v13M1.5 8h13M8 1.5L5.5 4M8 1.5L10.5 4M1.5 8L4 5.5M1.5 8L4 10.5M8 14.5L5.5 12M8 14.5L10.5 12M14.5 8L12 5.5M14.5 8L12 10.5",
    rotate:    "M13.5 8a5.5 5.5 0 1 1-1.6-3.9M13.5 2v3.5H10",
    scale:     "M3 13l10-10M3 13V8M3 13h5M13 3v5M13 3H8",
    cube:      "M8 1.5l6 3.25v6.5L8 14.5l-6-3.25v-6.5L8 1.5zM8 1.5v13M2 4.75L8 8l6-3.25",
    light:     "M8 1.5v2M3 3l1.4 1.4M1.5 8h2M13 13l-1.4-1.4M14.5 8h-2M3 13l1.4-1.4M13 3l-1.4 1.4M5 8a3 3 0 1 1 6 0c0 1.5-1 2.2-1 3.5h-4c0-1.3-1-2-1-3.5z",
    camera:    "M2 5h2l1-1.5h6L12 5h2v8H2V5zM8 11.5a2.5 2.5 0 1 0 0-5 2.5 2.5 0 0 0 0 5z",
    play:      "M3.5 2.5l10 5.5-10 5.5z",
    pause:     "M4 3h3v10H4zM9 3h3v10H9z",
    stop:      "M3.5 3.5h9v9h-9z",
    step:      "M3 3v10M5 8l8 5V3l-8 5z",
    eye:       "M1.5 8s2.5-5 6.5-5 6.5 5 6.5 5-2.5 5-6.5 5-6.5-5-6.5-5zM8 10.5a2.5 2.5 0 1 0 0-5 2.5 2.5 0 0 0 0 5z",
    eyeOff:    "M2 2l12 12M6 4.5c.6-.2 1.3-.3 2-.3 4 0 6.5 4.3 6.5 4.3a13 13 0 0 1-2 2.4M3.5 5.5A11 11 0 0 0 1.5 8.5s2.5 4.3 6.5 4.3c1 0 1.9-.2 2.6-.5M6.5 7a2 2 0 0 0 2.8 2.8",
    lock:      "M4 7V5a3 3 0 0 1 6 0v2M3 7h8v6H3z",
    unlock:    "M4 7V5a3 3 0 0 1 5.7-1M3 7h8v6H3z",
    chevR:     "M5.5 3l5 5-5 5",
    chevD:     "M3 5.5l5 5 5-5",
    chevL:     "M10.5 3l-5 5 5 5",
    plus:      "M8 3v10M3 8h10",
    minus:     "M3 8h10",
    x:         "M3.5 3.5l9 9M12.5 3.5l-9 9",
    folder:    "M1.5 4h4.5l1.5 1.5h7v8h-13V4z",
    file:      "M3 1.5h7l3 3v10h-10v-13zM10 1.5v3h3",
    code:      "M5 5l-3 3 3 3M11 5l3 3-3 3M9 3l-2 10",
    search:    "M7 11.5a4.5 4.5 0 1 0 0-9 4.5 4.5 0 0 0 0 9zM10.5 10.5l3.5 3.5",
    cmd:       "M4 1.5a2.5 2.5 0 1 0 0 5h2.5V4M11.5 1.5a2.5 2.5 0 1 1 0 5H9V4M4 14.5a2.5 2.5 0 1 1 0-5h2.5V12M11.5 14.5a2.5 2.5 0 1 0 0-5H9V12M4 6.5h8v3H4z",
    bug:       "M5 4a3 3 0 0 1 6 0M3 7h10M3 7v3a5 5 0 0 0 10 0V7M1 8.5h2M13 8.5h2M2 12l2-1M14 12l-2-1M2 5l2 1M14 5l-2 1M8 7v8",
    layers:    "M8 1.5L1.5 5L8 8.5L14.5 5L8 1.5zM1.5 8l6.5 3.5L14.5 8M1.5 11l6.5 3.5L14.5 11",
    grid:      "M2 2h5v5H2zM9 2h5v5H9zM2 9h5v5H2zM9 9h5v5H9z",
    cpu:       "M4 4h8v8H4zM2 6h2M2 8h2M2 10h2M12 6h2M12 8h2M12 10h2M6 2v2M8 2v2M10 2v2M6 12v2M8 12v2M10 12v2M6 6h4v4H6z",
    network:   "M8 2v3M8 11v3M2 8h3M11 8h3M3 3l2 2M13 3l-2 2M3 13l2-2M13 13l-2-2M6 8a2 2 0 1 0 4 0 2 2 0 0 0-4 0z",
    save:      "M2.5 2.5h9l2 2v9h-11v-11zM4.5 2.5v4h6v-4M5 9h6v4H5z",
    refresh:   "M14 3v4h-4M2 13v-4h4M3.5 7a5 5 0 0 1 8.7-2.5L14 7M12.5 9a5 5 0 0 1-8.7 2.5L2 9",
    gear:      "M8 5.5a2.5 2.5 0 1 0 0 5 2.5 2.5 0 0 0 0-5zM8 1.5v2M8 12.5v2M3.5 3.5l1.4 1.4M11.1 11.1l1.4 1.4M1.5 8h2M12.5 8h2M3.5 12.5l1.4-1.4M11.1 4.9l1.4-1.4",
    timer:     "M8 4v4l2.5 2.5M8 1.5a6.5 6.5 0 1 0 0 13 6.5 6.5 0 0 0 0-13z",
    flask:     "M6 1.5v4l-3.5 7a1 1 0 0 0 .9 1.5h9.2a1 1 0 0 0 .9-1.5l-3.5-7v-4M5 1.5h6",
    dot:       "M8 8m-1.5 0a1.5 1.5 0 1 0 3 0 1.5 1.5 0 0 0-3 0",
    spark:     "M8 1.5L9.5 6 14 7.5 9.5 9 8 13.5 6.5 9 2 7.5 6.5 6z",
  };
  const d = paths[name] || paths.dot;
  return (
    <svg width={size} height={size} viewBox="0 0 16 16" fill="none" stroke={color}
         strokeWidth="1.5" strokeLinecap="round" strokeLinejoin="round" style={style}>
      <path d={d} />
    </svg>
  );
};

// ── Wordmark ─────────────────────────────────────────────────────────────
// "trinyx" set in Space Grotesk; the lowercase "y" is replaced by a
// custom triple-prong glyph — three vectors meeting at a node, riffing on
// the engine's Sentinel/Brain/Encoder triplet without being literal.
const Wordmark = ({ size = 36, color = "var(--fg)", accent = "var(--yellow)" }) => {
  const h = size;
  return (
    <div style={{ display: "inline-flex", alignItems: "baseline", gap: 0,
                  fontFamily: "var(--f-display)", fontWeight: 600,
                  fontSize: h, color, lineHeight: 1, letterSpacing: "-0.04em" }}>
      <span>trin</span>
      <span style={{ position: "relative", display: "inline-block",
                     width: h * 0.55, height: h, verticalAlign: "baseline" }}>
        <svg viewBox="0 0 22 36" width={h * 0.55} height={h}
             style={{ position: "absolute", left: 0, top: 0, overflow: "visible" }}>
          {/* triangular y — three vectors converging at a node ~⅔ down */}
          <path d="M 2 4 L 11 22 M 20 4 L 11 22 M 11 22 L 11 34"
                stroke={color} strokeWidth="3.4" strokeLinecap="round" fill="none"/>
          <circle cx="11" cy="22" r="2.2" fill={accent}/>
        </svg>
      </span>
      <span>x</span>
    </div>
  );
};

// ── Logo mark (just the triangular y, square) ────────────────────────────
const LogoMark = ({ size = 40, bg = "var(--purple)", fg = "white", dot = "var(--yellow)" }) => (
  <div style={{ width: size, height: size, background: bg, borderRadius: 6,
                display: "grid", placeItems: "center" }}>
    <svg viewBox="0 0 22 36" width={size * 0.5} height={size * 0.8}>
      <path d="M 2 4 L 11 22 M 20 4 L 11 22 M 11 22 L 11 34"
            stroke={fg} strokeWidth="3.4" strokeLinecap="round" fill="none"/>
      <circle cx="11" cy="22" r="2.4" fill={dot}/>
    </svg>
  </div>
);

// ── Tier badge ───────────────────────────────────────────────────────────
const Tier = ({ kind, children }) => (
  <span className={`tier tier-${kind.toLowerCase()}`}>{children || kind}</span>
);

// ── Sparkline (for thread budget) ────────────────────────────────────────
const Spark = ({ data, color = "var(--purple)", width = 80, height = 18, fill = false }) => {
  const max = Math.max(...data, 0.01);
  const step = width / (data.length - 1);
  const pts = data.map((v, i) => [i * step, height - (v / max) * height]);
  const dPath = pts.map((p, i) => `${i ? "L" : "M"} ${p[0].toFixed(1)} ${p[1].toFixed(1)}`).join(" ");
  const fillPath = fill ? `${dPath} L ${width} ${height} L 0 ${height} Z` : "";
  return (
    <svg width={width} height={height} style={{ display: "block" }}>
      {fill && <path d={fillPath} fill={color} opacity="0.18"/>}
      <path d={dPath} fill="none" stroke={color} strokeWidth="1.2"/>
    </svg>
  );
};

// ── Viewport placeholder ─────────────────────────────────────────────────
// Stylized 3D scene preview using only CSS — a level grid in perspective +
// a few floating cubes. No SVG-drawn imagery beyond simple primitives.
const Viewport = ({ showGizmo = true, showStats = true, style }) => (
  <div style={{
    position: "relative", flex: "1 1 auto", background: "var(--bg-viewport)",
    overflow: "hidden", ...style
  }}>
    {/* radial vignette */}
    <div style={{
      position: "absolute", inset: 0,
      background: "radial-gradient(ellipse at 50% 60%, oklch(0.22 0.04 295) 0%, oklch(0.12 0.008 285) 70%)"
    }}/>
    {/* horizon grid */}
    <div style={{
      position: "absolute", left: "-25%", right: "-25%", bottom: 0, height: "55%",
      background:
        "linear-gradient(to bottom, transparent 0%, oklch(0.25 0.04 295 / 0.35) 100%)," +
        "repeating-linear-gradient(to right, oklch(0.45 0.06 295 / 0.45) 0 1px, transparent 1px 80px)," +
        "repeating-linear-gradient(to bottom, oklch(0.45 0.06 295 / 0.45) 0 1px, transparent 1px 60px)",
      transform: "perspective(700px) rotateX(58deg)",
      transformOrigin: "50% 100%",
      maskImage: "linear-gradient(to top, black 30%, transparent 100%)"
    }}/>
    {/* cubes */}
    <CubeIso style={{ left: "32%", top: "38%" }} size={70} hue={295} />
    <CubeIso style={{ left: "52%", top: "42%" }} size={56} hue={92} />
    <CubeIso style={{ left: "44%", top: "54%" }} size={44} hue={230} />
    {/* selection ring on cube 1 */}
    <div style={{
      position: "absolute", left: "32%", top: "38%", width: 86, height: 86,
      marginLeft: -8, marginTop: -8, border: "1.5px solid var(--yellow)",
      borderRadius: 2, pointerEvents: "none",
      boxShadow: "0 0 0 1px oklch(0.20 0.02 90), 0 0 20px oklch(0.85 0.16 92 / 0.3)"
    }}/>
    {showGizmo && (
      <div style={{ position: "absolute", right: 12, top: 10, width: 60, height: 60 }}>
        <ViewGizmo />
      </div>
    )}
    {showStats && (
      <div style={{
        position: "absolute", left: 10, top: 10,
        fontFamily: "var(--f-mono)", fontSize: 10.5,
        color: "oklch(0.85 0.04 285 / 0.7)",
        background: "oklch(0.12 0.008 285 / 0.6)",
        padding: "5px 8px", borderRadius: 3, lineHeight: 1.55,
        backdropFilter: "blur(4px)",
      }}>
        <div><span style={{ color: "var(--th-brain)"}}>brain</span>  0.73 ms · 512Hz</div>
        <div><span style={{ color: "var(--th-encoder)"}}>render</span> 0.88 ms · 144Hz</div>
        <div><span style={{ color: "var(--th-sentinel)"}}>input</span>  i→photon 7.4ms</div>
        <div style={{ color: "var(--fg-dim)"}}>205k ent · 8 wrk · avx2</div>
      </div>
    )}
  </div>
);

// Faux isometric cube using 3 CSS gradients
const CubeIso = ({ style, size = 64, hue = 295 }) => {
  const top = `oklch(0.78 0.10 ${hue})`;
  const left = `oklch(0.56 0.12 ${hue})`;
  const right = `oklch(0.42 0.10 ${hue})`;
  return (
    <div style={{ position: "absolute", width: size, height: size, ...style }}>
      <div style={{
        position: "absolute", inset: 0,
        clipPath: "polygon(50% 0%, 100% 25%, 50% 50%, 0% 25%)",
        background: top
      }}/>
      <div style={{
        position: "absolute", inset: 0,
        clipPath: "polygon(0% 25%, 50% 50%, 50% 100%, 0% 75%)",
        background: left
      }}/>
      <div style={{
        position: "absolute", inset: 0,
        clipPath: "polygon(50% 50%, 100% 25%, 100% 75%, 50% 100%)",
        background: right
      }}/>
    </div>
  );
};

const ViewGizmo = () => (
  <svg viewBox="0 0 60 60" width="60" height="60">
    <g transform="translate(30 30)">
      {/* X axis */}
      <line x1="0" y1="0" x2="22" y2="-6" stroke="oklch(0.70 0.18 25)" strokeWidth="1.5"/>
      <circle cx="22" cy="-6" r="6.5" fill="oklch(0.70 0.18 25)"/>
      <text x="22" y="-3" textAnchor="middle" fontSize="8" fontFamily="var(--f-mono)" fontWeight="700" fill="white">X</text>
      {/* Y axis */}
      <line x1="0" y1="0" x2="0" y2="-22" stroke="oklch(0.72 0.18 145)" strokeWidth="1.5"/>
      <circle cx="0" cy="-22" r="6.5" fill="oklch(0.72 0.18 145)"/>
      <text x="0" y="-19" textAnchor="middle" fontSize="8" fontFamily="var(--f-mono)" fontWeight="700" fill="white">Y</text>
      {/* Z axis (toward viewer) */}
      <line x1="0" y1="0" x2="-18" y2="10" stroke="oklch(0.70 0.18 250)" strokeWidth="1.5"/>
      <circle cx="-18" cy="10" r="6.5" fill="oklch(0.70 0.18 250)"/>
      <text x="-18" y="13" textAnchor="middle" fontSize="8" fontFamily="var(--f-mono)" fontWeight="700" fill="white">Z</text>
    </g>
  </svg>
);

Object.assign(window, { Icon, Wordmark, LogoMark, Tier, Spark, Viewport, CubeIso, ViewGizmo });
