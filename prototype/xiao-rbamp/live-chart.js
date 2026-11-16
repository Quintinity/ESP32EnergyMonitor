// Live indicator for the ESPHome v3 web UI sparkline charts (esp-entity-chart).
// Embedded via `web_server: js_include:` and served as /0.js.
//
// Adds a dot on the newest point of each sensor chart that:
//   - pulses continuously while data is fresh,
//   - flashes when a new reading arrives,
//   - goes grey and stops pulsing if no reading for STALE_MS (board offline / stalled).
//
// The stock chart dims its whole host element (opacity 0.1 collapsed, 0.5 expanded);
// that opacity is moved onto the line + axis only, so the dot stays clearly visible.

const STALE_MS = 15000; // 3 x the 5 s rbamp update_interval

const STYLE = `
  svg, .axis { opacity: var(--live-chart-opacity, 0.1); }
  .live-dot {
    position: absolute; width: 7px; height: 7px; margin: -3.5px 0 0 -3.5px;
    border-radius: 50%; background: var(--primary-color, #03a9f4);
    pointer-events: none; display: none;
  }
  .live-dot::after {
    content: ""; position: absolute; inset: 0; border-radius: 50%;
    background: inherit; animation: live-ping 1.6s ease-out infinite;
  }
  .live-dot.flash { animation: live-flash 0.6s ease-out; }
  .live-dot.stale { background: #9e9e9e; }
  .live-dot.stale::after { animation: none; opacity: 0; }
  @keyframes live-ping {
    0%   { transform: scale(1); opacity: 0.7; }
    100% { transform: scale(3.2); opacity: 0; }
  }
  @keyframes live-flash {
    0%   { transform: scale(1.9); }
    100% { transform: scale(1); }
  }
  @media (prefers-reduced-motion: reduce) {
    .live-dot::after, .live-dot.flash { animation: none; }
  }
`;

function ensureDot(el) {
  const root = el.shadowRoot;
  if (!root) return null;
  let dot = root.querySelector('.live-dot');
  if (!dot) {
    const style = document.createElement('style');
    style.textContent = STYLE;
    root.appendChild(style);
    dot = document.createElement('span');
    dot.className = 'live-dot';
    root.appendChild(dot);
    // Re-check freshness once a second so the dot greys out when updates stop.
    el._liveTimer = setInterval(() => {
      dot.classList.toggle('stale', Date.now() - (el._liveLast || 0) > STALE_MS);
    }, 1000);
  }
  return dot;
}

function placeDot(el, isNewData) {
  const dot = ensureDot(el);
  if (!dot) return;
  const vals = el._values();
  if (!vals.length) { dot.style.display = 'none'; return; }

  // Same mapping as esp-entity-chart._points(): x 0..100, y 96 (min) .. 4 (max), flat = 50.
  const v = vals[vals.length - 1];
  const min = Math.min(...vals), range = Math.max(...vals) - min;
  const x = vals.length > 1 ? 100 : 50;
  const y = range > 0 ? 96 - ((v - min) / range) * 92 : 50;
  dot.style.left = x + '%';
  dot.style.top = y + '%';
  dot.style.display = 'block';

  if (isNewData) {
    el._liveLast = Date.now();
    dot.classList.remove('stale', 'flash');
    void dot.offsetWidth; // restart the flash animation
    dot.classList.add('flash');
  }
}

customElements.whenDefined('esp-entity-chart').then(() => {
  const Chart = customElements.get('esp-entity-chart');
  const proto = Chart.prototype;

  // Move the host-level opacity onto the line/axis so the dot is not dimmed with it,
  // and stop the chart short of the value column so the dot sits beside the value
  // text instead of on top of it (stock width is calc(100% - 42px)).
  const origApply = proto._applyExpandedStyles;
  proto._applyExpandedStyles = function () {
    origApply.call(this);
    const o = this.style.opacity;
    this.style.setProperty('--live-chart-opacity', o || '0.1');
    this.style.opacity = '1';
    this.style.width = 'calc(100% - 142px)';
  };

  const origUpdated = proto.updated;
  proto.updated = function (changed) {
    if (origUpdated) origUpdated.call(this, changed);
    if (!this._liveInit) { this._liveInit = true; this._applyExpandedStyles(); }
    placeDot(this, changed.has('chartdata'));
  };

  const origDisconnected = proto.disconnectedCallback;
  proto.disconnectedCallback = function () {
    clearInterval(this._liveTimer);
    this._liveTimer = undefined;
    const dot = this.shadowRoot && this.shadowRoot.querySelector('.live-dot');
    if (dot) dot.remove(); // recreated (with a fresh timer) if the chart reconnects
    origDisconnected.call(this);
  };
});
