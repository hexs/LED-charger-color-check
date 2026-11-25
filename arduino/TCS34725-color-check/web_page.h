// ================= หน้า Web App (HTML + Chart.js) =================
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="th">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>ESP32-S3 Dual TCS34725 Spectral Monitor</title>
  <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
  <style>
    * { box-sizing: border-box; }
    body { font-family: 'Segoe UI', Tahoma, sans-serif; margin: 20px; background: #f4f6f9; color: #222; }
    h2 { margin-top: 0; }
    .controls { display: flex; flex-wrap: wrap; gap: 12px; align-items: center; margin-bottom: 16px; }
    button { padding: 8px 14px; border: none; border-radius: 6px; background: #007bff; color: #fff; cursor: pointer; font-size: 14px; }
    button.paused { background: #dc3545; }
    button.secondary { background: #6c757d; }
    button.active-mode { background: #198754; font-weight: bold; }
    select { padding: 7px 10px; border-radius: 6px; border: 1px solid #ccc; font-size: 14px; background: #fff; cursor: pointer; }
    .panel { background: #fff; padding: 14px 16px; border-radius: 8px; box-shadow: 0 2px 6px rgba(0,0,0,0.08); margin-bottom: 18px; display: flex; flex-wrap: wrap; gap: 12px; align-items: center; }
    .panel-title { font-weight: bold; font-size: 14px; }
    .manual-group { display: flex; flex-wrap: wrap; gap: 8px; align-items: center; }
    .manual-group.disabled { opacity: 0.45; pointer-events: none; }
    input[type="color"] { width: 42px; height: 34px; border: none; background: none; cursor: pointer; padding: 0; }
    .preset-btn { width: 26px; height: 26px; border-radius: 50%; border: 1px solid #999; padding: 0; }
    .sensor-section { background: #eef1f5; padding: 16px; border-radius: 10px; margin-bottom: 22px; border: 1px solid #dce1e8; }
    .sensor-header { display: flex; flex-wrap: wrap; gap: 10px; align-items: center; margin-bottom: 12px; }
    .sensor-header h3 { margin: 0; font-size: 18px; }
    .badge { padding: 4px 10px; border-radius: 999px; font-size: 12px; font-weight: bold; }
    .badge-ok { background: #d1e7dd; color: #0f5132; }
    .badge-err { background: #f8d7da; color: #842029; }
    .badge-warn { background: #fff3cd; color: #664d03; display: none; }
    .cards { display: grid; grid-template-columns: repeat(auto-fit, minmax(150px, 1fr)); gap: 12px; margin-bottom: 16px; }
    .card { background: #fff; padding: 12px 16px; border-radius: 8px; box-shadow: 0 2px 6px rgba(0,0,0,0.06); }
    .card .label { font-size: 12px; color: #666; }
    .card .val { font-size: 19px; font-weight: bold; margin-top: 4px; }
    .swatch { width: 100%; height: 28px; border-radius: 6px; border: 1px solid #ccc; margin-top: 4px; }
    .charts { display: grid; grid-template-columns: repeat(auto-fit, minmax(360px, 1fr)); gap: 16px; }
    .chart-box { background: #fff; padding: 14px; border-radius: 8px; box-shadow: 0 2px 6px rgba(0,0,0,0.06); min-width: 0; }
    .chart-wrapper { position: relative; width: 100%; height: 290px; }
  </style>
</head>
<body>
  <h2>Dual TCS34725 LED Spectral Monitor (ESP32-S3)</h2>

  <div class="controls">
    <button id="toggleBtn" onclick="togglePause()">Pause</button>
    <label>
      Gain:
      <select id="gainSelect" onchange="updateSensorConfig()">
        <option value="1">1X</option>
        <option value="4">4X</option>
        <option value="16">16X</option>
        <option value="60" selected>60X (แสงน้อยมาก)</option>
      </select>
    </label>
    <label>
      Integration:
      <select id="intSelect" onchange="updateSensorConfig()">
        <option value="3">2.4 ms</option>
        <option value="24">24 ms</option>
        <option value="50">50 ms</option>
        <option value="101">101 ms</option>
        <option value="154">154 ms</option>
        <option value="614" selected>614 ms (แสงน้อยมาก)</option>
      </select>
    </label>
    <label>
      Y-Axis Scale:
      <select id="yScaleSelect" onchange="onYScaleChange(this.value)">
        <option value="auto" selected>Auto (Smooth)</option>
        <option value="50">Fix: 50</option>
        <option value="100">Fix: 100</option>
        <option value="250">Fix: 250</option>
        <option value="500">Fix: 500</option>
        <option value="1000">Fix: 1,000</option>
        <option value="2000">Fix: 2,000</option>
        <option value="4000">Fix: 4,000</option>
        <option value="6000">Fix: 6,000</option>
        <option value="10000">Fix: 10,000</option>
        <option value="25000">Fix: 25,000</option>
        <option value="70000">Fix: 70,000 (Max 16-bit)</option>
      </select>
    </label>
    <span>API: <a href="/api/data" target="_blank">/api/data</a></span>
  </div>

  <!-- แผงควบคุม RGB LED บนบอร์ด -->
  <div class="panel">
    <span class="panel-title">Onboard RGB LED:</span>
    <button id="btnAuto1" class="active-mode" onclick="setLedMode('auto')">Auto (Sensor 1)</button>
    <button id="btnAuto2" class="secondary" onclick="setLedMode('auto2')">Auto (Sensor 2)</button>
    <button id="btnManual" class="secondary" onclick="setLedMode('manual')">Manual (เลือกสีเอง)</button>

    <div id="manualControls" class="manual-group disabled">
      <input type="color" id="colorPicker" value="#00ff00" oninput="pickColor(this.value)">
      <button class="preset-btn" style="background:#ff0000" title="Red" onclick="setManualRGB(255,0,0)"></button>
      <button class="preset-btn" style="background:#00ff00" title="Green" onclick="setManualRGB(0,255,0)"></button>
      <button class="preset-btn" style="background:#0000ff" title="Blue" onclick="setManualRGB(0,0,255)"></button>
      <button class="preset-btn" style="background:#ffff00" title="Yellow" onclick="setManualRGB(255,255,0)"></button>
      <button class="preset-btn" style="background:#ffffff" title="White" onclick="setManualRGB(255,255,255)"></button>
      <button class="secondary" style="padding:5px 10px; font-size:12px;" onclick="setManualRGB(0,0,0)">ปิดไฟ (Off)</button>
    </div>
  </div>

  <!-- ================= Sensor 1 (GPIO 4, 5) ================= -->
  <div class="sensor-section">
    <div class="sensor-header">
      <h3>Sensor 1 (SDA: 4, SCL: 5)</h3>
      <span id="s1Badge" class="badge badge-ok">Connected</span>
      <span id="s1Sat" class="badge badge-warn">⚠️ ค่าแสงทะลุเพดาน (Saturated)</span>
    </div>
    <div class="cards">
      <div class="card"><div class="label">Color Preview</div><div id="s1Swatch" class="swatch"></div></div>
      <div class="card"><div class="label">Raw (R / G / B / C)</div><div id="s1Raw" class="val">-</div></div>
      <div class="card"><div class="label">RGB (0-255)</div><div id="s1Rgb" class="val">-</div></div>
      <div class="card"><div class="label">Color Temp</div><div id="s1Cct" class="val">- K</div></div>
      <div class="card"><div class="label">Luminance</div><div id="s1Lux" class="val">- Lux</div></div>
    </div>
    <div class="charts">
      <div class="chart-box"><div class="chart-wrapper"><canvas id="s1Bar"></canvas></div></div>
      <div class="chart-box"><div class="chart-wrapper"><canvas id="s1Curve"></canvas></div></div>
    </div>
  </div>

  <!-- ================= Sensor 2 (GPIO 6, 7) ================= -->
  <div class="sensor-section">
    <div class="sensor-header">
      <h3>Sensor 2 (SDA: 6, SCL: 7)</h3>
      <span id="s2Badge" class="badge badge-ok">Connected</span>
      <span id="s2Sat" class="badge badge-warn">⚠️ ค่าแสงทะลุเพดาน (Saturated)</span>
    </div>
    <div class="cards">
      <div class="card"><div class="label">Color Preview</div><div id="s2Swatch" class="swatch"></div></div>
      <div class="card"><div class="label">Raw (R / G / B / C)</div><div id="s2Raw" class="val">-</div></div>
      <div class="card"><div class="label">RGB (0-255)</div><div id="s2Rgb" class="val">-</div></div>
      <div class="card"><div class="label">Color Temp</div><div id="s2Cct" class="val">- K</div></div>
      <div class="card"><div class="label">Luminance</div><div id="s2Lux" class="val">- Lux</div></div>
    </div>
    <div class="charts">
      <div class="chart-box"><div class="chart-wrapper"><canvas id="s2Bar"></canvas></div></div>
      <div class="chart-box"><div class="chart-wrapper"><canvas id="s2Curve"></canvas></div></div>
    </div>
  </div>

  <script>
    let paused = false;
    let currentLedMode = 'auto';
    let lastUserPickTime = 0;
    let lastConfigChangeTime = 0;

    const Y_LEVELS = [
      { max: 50,    step: 10 },
      { max: 100,   step: 20 },
      { max: 250,   step: 50 },
      { max: 500,   step: 100 },
      { max: 1000,  step: 200 },
      { max: 2000,  step: 500 },
      { max: 4000,  step: 1000 },
      { max: 6000,  step: 1000 },
      { max: 8000,  step: 2000 },
      { max: 10000, step: 2000 },
      { max: 15000, step: 3000 },
      { max: 25000, step: 5000 },
      { max: 50000, step: 10000 },
      { max: 70000, step: 10000 }
    ];

    let yScaleMode = 'auto';
    const SCALE_DOWN_DELAY_MS = 2000;

    function createScales(yMax, yStep) {
      return {
        x: {
          type: 'linear',
          min: 380,
          max: 700,
          offset: false,
          title: { display: true, text: 'Wavelength (nm)' },
          grid: { color: 'rgba(0,0,0,0.08)', offset: false }
        },
        y: {
          min: 0,
          max: yMax,
          title: { display: true, text: 'Raw Intensity' },
          grid: { color: 'rgba(0,0,0,0.08)' },
          ticks: { stepSize: yStep, autoSkip: false }
        }
      };
    }

    function createSensorView(prefix, labelName) {
      const initLevel = Y_LEVELS[4]; // เริ่มที่ 1,000
      const barChart = new Chart(document.getElementById(prefix + 'Bar'), {
        type: 'bar',
        data: {
          datasets: [{
            label: 'Raw Intensity',
            data: [{x: 465, y: 0}, {x: 525, y: 0}, {x: 615, y: 0}],
            backgroundColor: ['rgba(31,119,180,0.85)', 'rgba(44,160,44,0.85)', 'rgba(214,39,40,0.85)'],
            borderColor: '#000',
            borderWidth: 1,
            barThickness: 28
          }]
        },
        options: {
          responsive: true,
          maintainAspectRatio: false,
          animation: false,
          plugins: {
            title: { display: true, text: `${labelName} — Discrete Bar Chart`, font: { size: 14 } },
            legend: {
              display: true,
              onClick: () => {},
              labels: {
                generateLabels: () => [
                  { text: 'Blue (465nm)', fillStyle: 'rgba(31,119,180,0.85)', strokeStyle: '#000', lineWidth: 1 },
                  { text: 'Green (525nm)', fillStyle: 'rgba(44,160,44,0.85)', strokeStyle: '#000', lineWidth: 1 },
                  { text: 'Red (615nm)', fillStyle: 'rgba(214,39,40,0.85)', strokeStyle: '#000', lineWidth: 1 }
                ]
              }
            }
          },
          scales: createScales(initLevel.max, initLevel.step)
        }
      });

      const curveChart = new Chart(document.getElementById(prefix + 'Curve'), {
        type: 'line',
        data: {
          datasets: [
            {
              label: 'Estimated Curve',
              data: [{x: 380, y: 0}, {x: 465, y: 0}, {x: 525, y: 0}, {x: 615, y: 0}, {x: 700, y: 0}],
              borderColor: 'teal',
              backgroundColor: 'rgba(0, 128, 128, 0.2)',
              borderWidth: 2,
              fill: true,
              cubicInterpolationMode: 'monotone',
              tension: 0.4,
              pointRadius: 0
            },
            {
              label: 'Measured Points',
              type: 'scatter',
              data: [{x: 465, y: 0}, {x: 525, y: 0}, {x: 615, y: 0}],
              backgroundColor: ['#1f77b4', '#2ca02c', '#d62728'],
              pointRadius: 6
            }
          ]
        },
        options: {
          responsive: true,
          maintainAspectRatio: false,
          animation: false,
          plugins: {
            title: { display: true, text: `${labelName} — Approximated Spectral Curve`, font: { size: 14 } },
            legend: { display: true }
          },
          scales: createScales(initLevel.max, initLevel.step)
        }
      });

      return { prefix, barChart, curveChart, levelIdx: 4, lowSinceTime: 0 };
    }

    const views = [
      createSensorView('s1', 'Sensor 1'),
      createSensorView('s2', 'Sensor 2')
    ];

    function getTargetYScale(view, peak) {
      if (yScaleMode !== 'auto') {
        const fixedMax = Number(yScaleMode);
        const found = Y_LEVELS.find(l => l.max === fixedMax);
        return found || { max: fixedMax, step: fixedMax / 5 };
      }

      if (peak > Y_LEVELS[view.levelIdx].max * 0.90) {
        while (view.levelIdx < Y_LEVELS.length - 1 && peak > Y_LEVELS[view.levelIdx].max * 0.90) {
          view.levelIdx++;
        }
        view.lowSinceTime = 0;
        return Y_LEVELS[view.levelIdx];
      }

      if (view.levelIdx > 0 && peak < Y_LEVELS[view.levelIdx - 1].max * 0.70) {
        if (view.lowSinceTime === 0) {
          view.lowSinceTime = Date.now();
        } else if (Date.now() - view.lowSinceTime >= SCALE_DOWN_DELAY_MS) {
          while (view.levelIdx > 0 && peak < Y_LEVELS[view.levelIdx - 1].max * 0.70) {
            view.levelIdx--;
          }
          view.lowSinceTime = 0;
        }
      } else {
        view.lowSinceTime = 0;
      }

      return Y_LEVELS[view.levelIdx];
    }

    function applyYScale(view, yMax, yStep) {
      [view.barChart, view.curveChart].forEach(chart => {
        chart.options.scales.y.max = yMax;
        chart.options.scales.y.ticks.stepSize = yStep;
      });
    }

    function onYScaleChange(val) {
      yScaleMode = val;
      views.forEach(v => {
        v.lowSinceTime = 0;
        if (val !== 'auto') {
          const cfg = getTargetYScale(v, 0);
          applyYScale(v, cfg.max, cfg.step);
          v.barChart.update();
          v.curveChart.update();
        }
      });
    }

    async function updateSensorConfig() {
      lastConfigChangeTime = Date.now();
      const gain = document.getElementById('gainSelect').value;
      const intMs = document.getElementById('intSelect').value;
      await fetch(`/api/config?gain=${gain}&int_ms=${intMs}`);
    }

    function rgbToHex(r, g, b) {
      return '#' + [r, g, b]
        .map(v => Math.max(0, Math.min(255, Number(v) || 0)).toString(16).padStart(2, '0'))
        .join('');
    }

    function togglePause() {
      paused = !paused;
      const btn = document.getElementById('toggleBtn');
      btn.textContent = paused ? 'Resume' : 'Pause';
      btn.classList.toggle('paused', paused);
    }

    function updateLedUI(mode) {
      currentLedMode = mode;
      document.getElementById('btnAuto1').className = (mode === 'auto') ? 'active-mode' : 'secondary';
      document.getElementById('btnAuto2').className = (mode === 'auto2') ? 'active-mode' : 'secondary';
      document.getElementById('btnManual').className = (mode === 'manual') ? 'active-mode' : 'secondary';
      document.getElementById('manualControls').classList.toggle('disabled', mode !== 'manual');
    }

    async function setLedMode(mode) {
      updateLedUI(mode);
      if (mode === 'manual') {
        pickColor(document.getElementById('colorPicker').value);
      } else {
        await fetch(`/api/led?mode=${mode}`);
      }
    }

    function pickColor(hex) {
      const r = parseInt(hex.substr(1, 2), 16);
      const g = parseInt(hex.substr(3, 2), 16);
      const b = parseInt(hex.substr(5, 2), 16);
      setManualRGB(r, g, b);
    }

    async function setManualRGB(r, g, b) {
      lastUserPickTime = Date.now();
      updateLedUI('manual');
      document.getElementById('colorPicker').value = rgbToHex(r, g, b);
      await fetch(`/api/led?mode=manual&r=${r}&g=${g}&b=${b}`);
    }

    function updateSensorSection(view, s) {
      const p = view.prefix;
      const badge = document.getElementById(p + 'Badge');
      const sat = document.getElementById(p + 'Sat');

      if (!s.connected) {
        badge.textContent = '❌ Disconnected (รอเชื่อมต่อใหม่...)';
        badge.className = 'badge badge-err';
        sat.style.display = 'none';
        document.getElementById(p + 'Raw').textContent = 'Disconnected';
        document.getElementById(p + 'Rgb').textContent = '-';
        document.getElementById(p + 'Cct').textContent = '- K';
        document.getElementById(p + 'Lux').textContent = '- Lux';
        document.getElementById(p + 'Swatch').style.backgroundColor = '#e9ecef';
      } else {
        badge.textContent = '● Connected';
        badge.className = 'badge badge-ok';
        sat.style.display = s.saturated ? 'inline-block' : 'none';
        document.getElementById(p + 'Raw').textContent = `${s.raw.r} / ${s.raw.g} / ${s.raw.b} / ${s.raw.c}`;
        document.getElementById(p + 'Rgb').textContent = `${s.rgb.r}, ${s.rgb.g}, ${s.rgb.b}`;
        document.getElementById(p + 'Cct').textContent = `${s.color_temp_k} K`;
        document.getElementById(p + 'Lux').textContent = `${s.lux} Lux`;
        document.getElementById(p + 'Swatch').style.backgroundColor = `rgb(${s.rgb.r}, ${s.rgb.g}, ${s.rgb.b})`;
      }

      const peak = Math.max(s.raw.r, s.raw.g, s.raw.b);
      const yCfg = getTargetYScale(view, peak);
      applyYScale(view, yCfg.max, yCfg.step);

      view.barChart.data.datasets[0].data = [
        {x: 465, y: s.raw.b},
        {x: 525, y: s.raw.g},
        {x: 615, y: s.raw.r}
      ];
      view.barChart.update();

      view.curveChart.data.datasets[0].data = [
        {x: 380, y: 0},
        {x: 465, y: s.raw.b},
        {x: 525, y: s.raw.g},
        {x: 615, y: s.raw.r},
        {x: 700, y: 0}
      ];
      view.curveChart.data.datasets[1].data = [
        {x: 465, y: s.raw.b},
        {x: 525, y: s.raw.g},
        {x: 615, y: s.raw.r}
      ];
      view.curveChart.update();
    }

    async function fetchData() {
      if (paused) return;
      try {
        const res = await fetch('/api/data');
        const d = await res.json();

        if (d.config && (Date.now() - lastConfigChangeTime > 800)) {
          const gSel = document.getElementById('gainSelect');
          const iSel = document.getElementById('intSelect');
          if (Number(gSel.value) !== d.config.gain) gSel.value = String(d.config.gain);
          if (Number(iSel.value) !== d.config.integration_ms) iSel.value = String(d.config.integration_ms);
        }

        if (d.led) {
          if (d.led.mode !== currentLedMode) {
            updateLedUI(d.led.mode);
          }
          if (d.led.mode === 'manual' && (Date.now() - lastUserPickTime > 800)) {
            const hex = rgbToHex(d.led.r, d.led.g, d.led.b);
            const picker = document.getElementById('colorPicker');
            if (picker.value.toLowerCase() !== hex.toLowerCase()) {
              picker.value = hex;
            }
          }
        }

        if (d.sensors && d.sensors.length >= 2) {
          updateSensorSection(views[0], d.sensors[0]);
          updateSensorSection(views[1], d.sensors[1]);
        }
      } catch (err) {
        console.error('Fetch error:', err);
      }
    }

    setInterval(fetchData, 300);
    fetchData();
  </script>
</body>
</html>
)rawliteral";