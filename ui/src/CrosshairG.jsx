import { useState, useEffect, useCallback } from "react";

const sendToApp = (type, payload) => {
  if (window.chrome?.webview) {
    window.chrome.webview.postMessage(JSON.stringify({ type, ...payload }));
  }
};

const styles = `
  @import url('https://fonts.googleapis.com/css2?family=Share+Tech+Mono&family=Rajdhani:wght@300;400;500;600;700&display=swap');

  :root {
    --bg: #080a08;
    --bg2: #0d110d;
    --bg3: #111611;
    --surface: #0f140f;
    --surface2: #161d16;
    --border: #1e2d1e;
    --border2: #2a3d2a;
    --accent: #39ff14;
    --accent2: #2acc0f;
    --accent3: #1a7a09;
    --accent-dim: rgba(57,255,20,0.15);
    --accent-glow: rgba(57,255,20,0.4);
    --text: #c8e6c8;
    --text-dim: #5a7a5a;
    --text-muted: #2d3d2d;
    --danger: #ff3333;
    --warn: #ffaa00;
    --mono: 'Share Tech Mono', monospace;
    --display: 'Rajdhani', sans-serif;
  }

  * { box-sizing: border-box; margin: 0; padding: 0; }

  html, body, #root {
    width: 100%; height: 100%;
    overflow: hidden;
    background: var(--bg);
  }

  body {
    color: var(--text);
    font-family: var(--display);
  }

  body::before {
    content: '';
    position: fixed; inset: 0;
    background: repeating-linear-gradient(0deg, transparent, transparent 2px, rgba(0,0,0,0.06) 2px, rgba(0,0,0,0.06) 4px);
    pointer-events: none;
    z-index: 9999;
  }

  @keyframes pulse-glow {
    0%,100%{box-shadow:0 0 8px var(--accent-glow),inset 0 0 8px rgba(57,255,20,.05)}
    50%{box-shadow:0 0 16px var(--accent-glow),inset 0 0 16px rgba(57,255,20,.1)}
  }
  @keyframes flicker {
    0%,94%,100%{opacity:1} 95%{opacity:.8} 97%{opacity:.95}
  }
  @keyframes slide-in {
    from{opacity:0;transform:translateY(-6px)} to{opacity:1;transform:translateY(0)}
  }
  @keyframes scan-line {
    0%{top:-10%} 100%{top:110%}
  }

  .app {
    display: grid;
    grid-template-columns: 270px 1fr;
    grid-template-rows: 50px 1fr;
    height: 100vh;
    animation: slide-in 0.25s ease;
  }

  /* HEADER */
  .header {
    grid-column: 1/-1;
    display: flex;
    align-items: center;
    justify-content: space-between;
    padding: 0 16px;
    background: var(--surface);
    border-bottom: 1px solid var(--border);
    position: relative;
    overflow: hidden;
    
  }

  .header::after {
    content:'';
    position:absolute; bottom:0; left:0;
    width:100%; height:1px;
    background:linear-gradient(90deg,transparent,var(--accent),transparent);
    animation: flicker 8s infinite;
  }

  .header-left { display:flex; align-items:center; gap:12px;  }

  .logo { display:flex; align-items:center; gap:8px; }

  .logo-icon { width:24px; height:24px; }
  .logo-icon svg { width:100%; height:100%; }

  .logo-text {
    font-family:var(--display);
    font-weight:700;
    font-size:16px;
    letter-spacing:3px;
    color:var(--accent);
    text-shadow:0 0 12px var(--accent-glow);
    animation: flicker 12s infinite;
  }

  .logo-version {
    font-family:var(--mono);
    font-size:9px;
    color:var(--text-muted);
    letter-spacing:1px;
    align-self:flex-end;
    margin-bottom:2px;
  }

  .header-right {
    display:flex; align-items:center; gap:12px;
    
  }

  .status-pill {
    display:flex; align-items:center; gap:5px;
    font-family:var(--mono); font-size:10px;
    color:var(--text-dim); letter-spacing:1px;
  }

  .status-dot {
    width:6px; height:6px; border-radius:50%;
    background:var(--accent);
    box-shadow:0 0 6px var(--accent);
    animation: pulse-glow 2s infinite;
  }
  .status-dot.off { background:var(--text-muted); box-shadow:none; animation:none; }

  .hotkey-badge {
    font-family:var(--mono); font-size:11px;
    color:var(--text);
    background:var(--surface2);
    border:1px solid var(--border2);
    padding:3px 8px; letter-spacing:1px;
  }

  /* Window controls */
  .win-controls { display:flex; gap:0; }
  .win-btn {
    width:42px; height:50px;
    background:transparent; border:none;
    color:var(--text-dim);
    font-size:14px; cursor:pointer;
    display:flex; align-items:center; justify-content:center;
    transition:background 0.1s, color 0.1s;
    font-family:var(--mono);
  }
  .win-btn:hover { background:var(--surface2); color:var(--text); }
  .win-btn.close:hover { background:var(--danger); color:white; }

  /* SIDEBAR */
  .sidebar {
    background:var(--surface);
    border-right:1px solid var(--border);
    overflow-y:auto; overflow-x:hidden;
  }
  .sidebar::-webkit-scrollbar{width:3px}
  .sidebar::-webkit-scrollbar-thumb{background:var(--border2)}

  .section { border-bottom:1px solid var(--border); }

  .section-header {
    display:flex; align-items:center; gap:8px;
    padding:9px 14px;
    font-family:var(--mono); font-size:11px;
    letter-spacing:2px; color:var(--accent);
    background:var(--bg2);
    border-bottom:1px solid var(--border);
    position:relative;
  }
  .section-header::before {
    content:''; position:absolute;
    left:0; top:0; bottom:0; width:2px;
    background:var(--accent);
    box-shadow:0 0 6px var(--accent-glow);
  }

  .section-body { padding:12px 14px; display:flex; flex-direction:column; gap:12px; }

  /* SHAPE GRID */
  .style-grid { display:grid; grid-template-columns:1fr 1fr; gap:5px; }

  .style-btn {
    background:var(--bg3);
    border:1px solid var(--border);
    color:var(--text-dim);
    font-family:var(--display);
    font-size:11px; font-weight:600;
    letter-spacing:1px; padding:7px 4px;
    cursor:pointer; transition:all 0.12s;
    text-align:center; position:relative; overflow:hidden;
  }
  .style-btn::after {
    content:''; position:absolute; inset:0;
    background:var(--accent-dim); opacity:0; transition:opacity 0.12s;
  }
  .style-btn:hover { border-color:var(--accent3); color:var(--text); }
  .style-btn:hover::after { opacity:1; }
  .style-btn.active {
    border-color:var(--accent); color:var(--accent);
    background:var(--surface2);
    box-shadow:0 0 8px rgba(57,255,20,.2),inset 0 0 12px rgba(57,255,20,.05);
  }
  .style-btn.active::after { opacity:1; }

  /* SLIDERS */
  .slider-row { display:flex; flex-direction:column; gap:4px; }
  .slider-meta { display:flex; justify-content:space-between; align-items:baseline; }
  .slider-label { font-family:var(--mono); font-size:11px; letter-spacing:1.5px; color:var(--text-dim); }
  .slider-value { font-family:var(--mono); font-size:13px; color:var(--accent); text-shadow:0 0 8px var(--accent-glow); min-width:24px; text-align:right; }

  .slider-track { position:relative; height:20px; display:flex; align-items:center; cursor:pointer; }
  .slider-bg { position:absolute; left:0; right:0; height:3px; background:var(--bg3); border:1px solid var(--border); }
  .slider-fill { position:absolute; left:0; height:3px; background:linear-gradient(90deg,var(--accent3),var(--accent)); box-shadow:0 0 6px var(--accent-glow); }
  .slider-thumb { position:absolute; width:13px; height:13px; background:var(--bg); border:2px solid var(--accent); box-shadow:0 0 8px var(--accent-glow); transform:translateX(-50%); pointer-events:none; }
  .slider-input { position:absolute; inset:0; width:100%; opacity:0; cursor:pointer; height:100%; margin:0; }

  /* COLOR PICKER */
  .color-picker { display:flex; flex-direction:column; gap:6px; }
  .color-picker-header { display:flex; align-items:center; gap:8px; cursor:pointer; padding:4px 0; }
  .color-picker-swatch { width:20px; height:20px; border:1px solid rgba(255,255,255,.2); flex-shrink:0; }
  .color-picker-label { font-family:var(--mono); font-size:11px; letter-spacing:1px; color:var(--text-dim); flex:1; }
  .color-picker-hex { font-family:var(--mono); font-size:11px; color:var(--accent); letter-spacing:1px; }
  .color-picker-body { display:flex; flex-direction:column; gap:6px; padding:8px; background:var(--bg3); border:1px solid var(--border); }
  .color-picker-body.hidden { display:none; }
  .hue-track { position:relative; height:14px; cursor:pointer; border:1px solid var(--border);
    background:linear-gradient(90deg,#f00,#ff0,#0f0,#0ff,#00f,#f0f,#f00); }
  .sat-track { position:relative; height:14px; cursor:pointer; border:1px solid var(--border); }
  .lit-track { position:relative; height:14px; cursor:pointer; border:1px solid var(--border);
    background:linear-gradient(90deg,#000,#fff); }
  .picker-thumb { position:absolute; top:-2px; width:4px; height:18px; background:white; border:1px solid #000; pointer-events:none; transform:translateX(-50%); }
  .color-presets { display:grid; grid-template-columns:repeat(8,1fr); gap:4px; }
  .color-preset { height:16px; cursor:pointer; border:1px solid transparent; transition:border-color .1s; }
  .color-preset:hover { border-color:var(--accent); }



  /* TOGGLES */
  .toggle-row { display:flex; align-items:center; justify-content:space-between; padding:2px 0; }
  .toggle-label { font-family:var(--display); font-size:13px; font-weight:500; color:var(--text-dim); }
  .toggle-label.on { color:var(--text); }
  .toggle-switch { position:relative; width:36px; height:19px; flex-shrink:0; cursor:pointer; }
  .toggle-switch input { display:none; }
  .toggle-track { position:absolute; inset:0; background:var(--bg3); border:1px solid var(--border); transition:all .2s; }
  .toggle-knob { position:absolute; top:3px; left:3px; width:11px; height:11px; background:var(--text-muted); transition:all .2s; }
  .toggle-switch input:checked~.toggle-track { background:var(--surface2); border-color:var(--accent); box-shadow:0 0 8px rgba(57,255,20,.3); }
  .toggle-switch input:checked~.toggle-knob { left:20px; background:var(--accent); box-shadow:0 0 6px var(--accent-glow); }

  /* ACTION BTN */
  .action-btn {
    width:100%; background:transparent; border:1px solid var(--border2);
    color:var(--accent); font-family:var(--mono); font-size:10px;
    letter-spacing:2px; padding:9px; cursor:pointer; transition:all .15s;
    text-transform:uppercase; position:relative; overflow:hidden;
  }
  .action-btn::before { content:''; position:absolute; left:-100%; top:0; bottom:0; width:100%; background:var(--accent-dim); transition:left .3s; }
  .action-btn:hover { border-color:var(--accent); box-shadow:0 0 12px rgba(57,255,20,.2); }
  .action-btn:hover::before { left:0; }
  .action-btn.success { border-color:var(--accent); box-shadow:0 0 16px rgba(57,255,20,.4); }

  /* PREVIEW */
  .preview-panel { background:var(--bg); display:flex; flex-direction:column; overflow:hidden; }

  .preview-header { display:flex; align-items:center; justify-content:space-between; padding:9px 18px; border-bottom:1px solid var(--border); background:var(--surface); flex-shrink:0; }
  .preview-title { font-family:var(--mono); font-size:11px; letter-spacing:2px; color:var(--text); }
  .preview-meta { font-family:var(--mono); font-size:11px; color:var(--text-dim); letter-spacing:1px; }

  .preview-canvas { flex:1; position:relative; overflow:hidden; }
  .preview-canvas::before {
    content:''; position:absolute; inset:0;
    background-image:linear-gradient(var(--border) 1px,transparent 1px),linear-gradient(90deg,var(--border) 1px,transparent 1px);
    background-size:40px 40px; opacity:0.3;
  }
  .preview-canvas::after {
    content:''; position:absolute; top:50%; left:50%;
    transform:translate(-50%,-50%);
    width:100px; height:100px; border:1px solid var(--text-muted); border-radius:50%; opacity:0.1;
  }

  .crosshair-preview { position:absolute; top:50%; left:50%; transform:translate(-50%,-50%); }

  .scan-overlay {
    position:absolute; left:0; right:0; height:50px;
    background:linear-gradient(180deg,transparent,rgba(57,255,20,.04),transparent);
    animation:scan-line 5s linear infinite; pointer-events:none;
  }

  .preview-info {
    padding:10px 18px; border-top:1px solid var(--border);
    background:var(--surface);
    display:grid; grid-template-columns:repeat(4,1fr); gap:10px;
    flex-shrink:0;
  }
  .info-cell { display:flex; flex-direction:column; gap:2px; }
  .info-key { font-family:var(--mono); font-size:10px; color:var(--text-dim); letter-spacing:1px; text-transform:uppercase; }
  .info-val { font-family:var(--mono); font-size:12px; color:var(--accent); text-shadow:0 0 6px var(--accent-glow); }

  /* WARNING */
  .fs-warning {
    margin:0 14px 12px;
    padding:9px 10px;
    background:rgba(255,170,0,.06);
    border:1px solid rgba(255,170,0,.25);
    border-left:2px solid var(--warn);
    display:flex; gap:7px; align-items:flex-start;
  }
  .fs-warning-icon { color:var(--warn); font-size:11px; font-family:var(--mono); flex-shrink:0; margin-top:1px; }
  .fs-warning-text { font-family:var(--display); font-size:11px; color:rgba(255,170,0,.75); line-height:1.5; font-weight:500; }

  .app { cursor: default; }
`;

const SHAPES = ["CROSS","DOT","CROSS+DOT","CIRCLE","CIRCLE+DOT","CIRCLE+CROSS+DOT"];

function CrosshairSVG({ config }) {
  const { shape, size, thickness, gap, dotSize, color, outlineColor, outlineSize } = config;
  const cx = 100, cy = 100;
  const el = [];
  const hasCross  = [0,2,5].includes(shape);
  const hasDot    = [1,2,4,5].includes(shape);
  const hasCircle = [3,4,5].includes(shape);
  const sw = Math.max(1, thickness);
  const ol = outlineSize;

  if (ol > 0) {
    if (hasCross) el.push(
      <line key="ol-l" x1={cx-size} y1={cy} x2={cx-gap} y2={cy} stroke={outlineColor} strokeWidth={sw+ol*2}/>,
      <line key="ol-r" x1={cx+gap} y1={cy} x2={cx+size} y2={cy} stroke={outlineColor} strokeWidth={sw+ol*2}/>,
      <line key="ol-t" x1={cx} y1={cy-size} x2={cx} y2={cy-gap} stroke={outlineColor} strokeWidth={sw+ol*2}/>,
      <line key="ol-b" x1={cx} y1={cy+gap} x2={cx} y2={cy+size} stroke={outlineColor} strokeWidth={sw+ol*2}/>
    );
    if (hasCircle) el.push(<circle key="ol-c" cx={cx} cy={cy} r={size} fill="none" stroke={outlineColor} strokeWidth={sw+ol*2}/>);
    if (hasDot)    el.push(<circle key="ol-d" cx={cx} cy={cy} r={dotSize+ol} fill={outlineColor}/>);
  }
  if (hasCross) el.push(
    <line key="l" x1={cx-size} y1={cy} x2={cx-gap} y2={cy} stroke={color} strokeWidth={sw}/>,
    <line key="r" x1={cx+gap}  y1={cy} x2={cx+size} y2={cy} stroke={color} strokeWidth={sw}/>,
    <line key="t" x1={cx} y1={cy-size} x2={cx} y2={cy-gap}  stroke={color} strokeWidth={sw}/>,
    <line key="b" x1={cx} y1={cy+gap}  x2={cx} y2={cy+size} stroke={color} strokeWidth={sw}/>
  );
  if (hasCircle) el.push(<circle key="c" cx={cx} cy={cy} r={size} fill="none" stroke={color} strokeWidth={sw}/>);
  if (hasDot)    el.push(<circle key="d" cx={cx} cy={cy} r={dotSize} fill={color}/>);

  return <svg width="200" height="200" viewBox="0 0 200 200" className="crosshair-preview">{el}</svg>;
}

function Slider({ label, value, min, max, onChange }) {
  const pct = ((value - min) / (max - min)) * 100;
  return (
    <div className="slider-row">
      <div className="slider-meta">
        <span className="slider-label">{label}</span>
        <span className="slider-value">{value}</span>
      </div>
      <div className="slider-track">
        <div className="slider-bg"/>
        <div className="slider-fill" style={{width:`${pct}%`}}/>
        <div className="slider-thumb" style={{left:`${pct}%`}}/>
        <input type="range" min={min} max={max} value={value}
          onChange={e=>onChange(+e.target.value)} className="slider-input"/>
      </div>
    </div>
  );
}

function Toggle({ label, value, onChange }) {
  return (
    <div className="toggle-row">
      <span className={`toggle-label ${value?'on':''}`}>{label}</span>
      <label className="toggle-switch">
        <input type="checkbox" checked={value} onChange={e=>onChange(e.target.checked)}/>
        <div className="toggle-track"/>
        <div className="toggle-knob"/>
      </label>
    </div>
  );
}

function hexToHsl(hex) {
  let r=parseInt(hex.slice(1,3),16)/255, g=parseInt(hex.slice(3,5),16)/255, b=parseInt(hex.slice(5,7),16)/255;
  const max=Math.max(r,g,b), min=Math.min(r,g,b), l=(max+min)/2;
  if(max===min) return [0,0,Math.round(l*100)];
  const d=max-min, s=l>0.5?d/(2-max-min):d/(max+min);
  let h = max===r ? (g-b)/d+(g<b?6:0) : max===g ? (b-r)/d+2 : (r-g)/d+4;
  return [Math.round(h/6*360), Math.round(s*100), Math.round(l*100)];
}
function hslToHex(h,s,l) {
  s/=100; l/=100;
  const a=s*Math.min(l,1-l);
  const f=n=>{ const k=(n+h/30)%12; const c=l-a*Math.max(-1,Math.min(k-3,9-k,1)); return Math.round(255*c).toString(16).padStart(2,'0'); };
  return `#${f(0)}${f(8)}${f(4)}`;
}

const PRESETS = ['#39ff14','#ff3333','#ffaa00','#00aaff','#ff00ff','#ffffff','#000000','#ff6600'];

function ColorPicker({ label, value, onChange }) {
  const [open, setOpen] = useState(false);
  const [hsl, setHsl] = useState(() => hexToHsl(value||'#00ff14'));

  const update = (newHsl) => { setHsl(newHsl); onChange(hslToHex(...newHsl)); };

  const makeTrackProps = (idx, max) => ({
    onMouseDown: (e) => {
      e.preventDefault();
      const el = e.currentTarget;
      const apply = (ev) => {
        const rect = el.getBoundingClientRect();
        const pct = Math.max(0, Math.min(1, (ev.clientX - rect.left) / rect.width));
        const n = [...hsl]; n[idx] = Math.round(pct * max); update(n);
      };
      apply(e);
      const onMove = (ev) => apply(ev);
      const onUp = () => { window.removeEventListener('mousemove',onMove); window.removeEventListener('mouseup',onUp); };
      window.addEventListener('mousemove', onMove);
      window.addEventListener('mouseup', onUp);
    }
  });

  const satGrad = `linear-gradient(90deg, hsl(${hsl[0]},0%,50%), hsl(${hsl[0]},100%,50%))`;

  return (
    <div className="color-picker">
      <div className="color-picker-header" onClick={()=>setOpen(o=>!o)}>
        <div className="color-picker-swatch" style={{background:value}}/>
        <span className="color-picker-label">{label}</span>
        <span className="color-picker-hex">{(value||'').toUpperCase()}</span>
        <span style={{color:'var(--text-dim)',fontFamily:'var(--mono)',fontSize:'10px'}}>{open?'▲':'▼'}</span>
      </div>
      <div className={`color-picker-body ${open?'':'hidden'}`}>
        <div style={{display:'flex',alignItems:'center',gap:6}}>
          <span style={{fontFamily:'var(--mono)',fontSize:'9px',color:'var(--text-dim)',width:14}}>H</span>
          <div className="hue-track" style={{flex:1,cursor:'ew-resize'}} {...makeTrackProps(0,360)}>
            <div className="picker-thumb" style={{left:`${(hsl[0]/360)*100}%`}}/>
          </div>
        </div>
        <div style={{display:'flex',alignItems:'center',gap:6}}>
          <span style={{fontFamily:'var(--mono)',fontSize:'9px',color:'var(--text-dim)',width:14}}>S</span>
          <div className="sat-track" style={{flex:1,background:satGrad,cursor:'ew-resize'}} {...makeTrackProps(1,100)}>
            <div className="picker-thumb" style={{left:`${hsl[1]}%`}}/>
          </div>
        </div>
        <div style={{display:'flex',alignItems:'center',gap:6}}>
          <span style={{fontFamily:'var(--mono)',fontSize:'9px',color:'var(--text-dim)',width:14}}>L</span>
          <div className="lit-track" style={{flex:1,cursor:'ew-resize'}} {...makeTrackProps(2,100)}>
            <div className="picker-thumb" style={{left:`${hsl[2]}%`}}/>
          </div>
        </div>
        <div className="color-presets">
          {PRESETS.map(p=>(
            <div key={p} className="color-preset" style={{background:p}}
              onClick={()=>{ onChange(p); setHsl(hexToHsl(p)); }}/>
          ))}
        </div>
      </div>
    </div>
  );
}

export default function CrosshairG() {
  const [cfg, setCfg] = useState({
    shape:0, size:12, thickness:2, gap:4,
    outlineSize:1, dotSize:3,
    color:"#00ff14", outlineColor:"#000000",
    visible:true, lockMouse:false, secondMonitor:false,
  });
  const [centered, setCentered] = useState(false);
  const [time, setTime] = useState('');

  useEffect(() => {
    const update = () => setTime(new Date().toLocaleTimeString('en-US',{hour12:false}));
    update();
    const t = setInterval(update, 1000);
    return () => clearInterval(t);
  }, []);

  useEffect(() => {
    const handler = (e) => {
      try {
        const data = JSON.parse(e.data);
        if (data.type === 'init' || data.type === 'state') {
          setCfg(p => ({ ...p, ...data.config }));
        }
      } catch { /* ignore malformed messages */ }
    };
    window.chrome?.webview?.addEventListener('message', handler);
    sendToApp('ready', {});
    return () => window.chrome?.webview?.removeEventListener('message', handler);
  }, []);

  const set = useCallback((k, v) => {
    setCfg(p => {
      const next = { ...p, [k]: v };
      sendToApp('config', { key: k, value: v });
      return next;
    });
  }, []);

  const handleCenter = () => {
    sendToApp('center', {});
    setCentered(true);
    setTimeout(() => setCentered(false), 1500);
  };

  return (
    <>
      <style>{styles}</style>
      <div className="app">

        {/* HEADER */}
        <header className="header">
          <div className="header-left">
            <div className="logo">
              <div className="logo-icon">
                <svg viewBox="0 0 24 24" fill="none">
                  <line x1="12" y1="2"  x2="12" y2="8"  stroke="#39ff14" strokeWidth="2"/>
                  <line x1="12" y1="16" x2="12" y2="22" stroke="#39ff14" strokeWidth="2"/>
                  <line x1="2"  y1="12" x2="8"  y2="12" stroke="#39ff14" strokeWidth="2"/>
                  <line x1="16" y1="12" x2="22" y2="12" stroke="#39ff14" strokeWidth="2"/>
                  <circle cx="12" cy="12" r="2" fill="#39ff14"/>
                  <circle cx="12" cy="12" r="6" stroke="#39ff14" strokeWidth="1" opacity="0.35"/>
                </svg>
              </div>
              <span className="logo-text">CROSSHAIRG</span>
              <span className="logo-version">v1.3</span>
            </div>
            <div className="status-pill">
              <div className={`status-dot ${cfg.visible?'':'off'}`}/>
              {cfg.visible ? 'ACTIVE' : 'HIDDEN'}
            </div>
          </div>

          <div className="header-right">
            <span className="hotkey-badge">MENU · CTRL+F5</span>
            <span className="hotkey-badge">TOGGLE · CTRL+F6</span>
            <span className="status-pill" style={{color:'var(--text-muted)'}}>{time}</span>
          </div>
        </header>

        {/* SIDEBAR */}
        <aside className="sidebar">

          <div className="section">
            <div className="section-header">▸ SHAPE</div>
            <div className="section-body">
              <div className="style-grid">
                {SHAPES.map((s,i)=>(
                  <button key={i} className={`style-btn ${cfg.shape===i?'active':''}`}
                    onClick={()=>set('shape',i)}>{s}</button>
                ))}
              </div>
            </div>
          </div>

          <div className="section">
            <div className="section-header">▸ DIMENSIONS</div>
            <div className="section-body">
              <Slider label="SIZE"    value={cfg.size}        min={0}  max={50} onChange={v=>set('size',v)}/>
              <Slider label="WIDTH"   value={cfg.thickness}   min={1}  max={50} onChange={v=>set('thickness',v)}/>
              <Slider label="GAP"     value={cfg.gap}         min={0}  max={50} onChange={v=>set('gap',v)}/>
              <Slider label="OUTLINE" value={cfg.outlineSize} min={0}  max={10} onChange={v=>set('outlineSize',v)}/>
              <Slider label="DOT"     value={cfg.dotSize}     min={1}  max={20} onChange={v=>set('dotSize',v)}/>
            </div>
          </div>

          <div className="section">
            <div className="section-header">▸ COLOR</div>
            <div className="section-body">
              <ColorPicker label="CROSSHAIR" value={cfg.color} onChange={v=>set('color',v)}/>
              <ColorPicker label="OUTLINE"   value={cfg.outlineColor} onChange={v=>set('outlineColor',v)}/>
            </div>
          </div>

          <div className="section">
            <div className="section-header">▸ OPTIONS</div>
            <div className="section-body">
              <Toggle label="Toggle Crosshair"        value={cfg.visible}       onChange={v=>set('visible',v)}/>
              <Toggle label="Lock mouse to monitor" value={cfg.lockMouse}     onChange={v=>set('lockMouse',v)}/>
              <Toggle label="Use second monitor"    value={cfg.secondMonitor} onChange={v=>set('secondMonitor',v)}/>
              <button className={`action-btn ${centered?'success':''}`} onClick={handleCenter}>
                {centered ? '✓  CENTERED' : '⊕  CENTER / CALIBRATE'}
              </button>
            </div>
            <div className="fs-warning">
              <span className="fs-warning-icon">!</span>
              <span className="fs-warning-text">
                Exclusive fullscreen bypasses DWM. Use <strong>Borderless Windowed</strong> in game settings.
              </span>
            </div>
          </div>

        </aside>

        {/* PREVIEW */}
        <main className="preview-panel">
          <div className="preview-header">
            <span className="preview-title">▸ LIVE PREVIEW</span>
            <span className="preview-meta">SHAPE: {SHAPES[cfg.shape]}</span>
          </div>
          <div className="preview-canvas">
            <div className="scan-overlay"/>
            {<CrosshairSVG config={cfg}/>}
          </div>
          <div className="preview-info">
            <div className="info-cell">
              <span className="info-key">SIZE</span>
              <span className="info-val">{cfg.size}px</span>
            </div>
            <div className="info-cell">
              <span className="info-key">WIDTH</span>
              <span className="info-val">{cfg.thickness}px</span>
            </div>
            <div className="info-cell">
              <span className="info-key">COLOR</span>
              <span className="info-val" style={{color:cfg.color,textShadow:`0 0 8px ${cfg.color}`}}>
                {cfg.color.toUpperCase()}
              </span>
            </div>
            <div className="info-cell">
              <span className="info-key">STATUS</span>
              <span className="info-val" style={{color:cfg.visible?'var(--accent)':'var(--text-muted)'}}>
                {cfg.visible?'ACTIVE':'OFF'}
              </span>
            </div>
          </div>
        </main>

      </div>
    </>
  );
}
