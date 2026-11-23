// ================= หน้า Web App (HTML + Chart.js) =================
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="th">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>ESP32-S3 TCS34725 Spectral Monitor</title>
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
    .alert-banner { display: none; background: #f8d7da; color: #842029; border: 1px solid #f5c2c7; padding: 12px 16px; border-radius: 8px; margin-bottom: 16px; font-weight: bold; }
    .panel { background: #fff; padding: 14px 16px; border-radius: 8px; box-shadow: 0 2px 6px rgba(0,0,0,0.08); margin-bottom: 16px; display: flex; flex-wrap: wrap; gap: 14px; align-items: center; }
    .panel-title { font-weight: bold; font-size: 14px; }
    .manual-group { display: flex; flex-wrap: wrap; gap: 8px; align-items: center; }
    .manual-group.disabled { opacity: 0.45; pointer-events: none; }
    input[type="color"] { width: 42px; height: 34px; border: none; background: none; cursor: pointer; padding: 0; }
    .preset-btn { width: 26px; height: 26px; border-radius: 50%; border: 1px solid #999; padding: 0; }
    .cards { display: grid; grid-template-columns: repeat(auto-fit, minmax(150px, 1fr)); gap: 12px; margin-bottom: 20px; }
    .card { background: #fff; padding: 12px 16px; border-radius: 8px; box-shadow: 0 2px 6px rgba(0,0,0,0.08); }
    .card .label { font-size: 12px; color: #666; }
    .card .val { font-size: 20px; font-weight: bold; margin-top: 4px; }
    .swatch { width: 100%; height: 28px; border-radius: 6px; border: 1px solid #ccc; margin-top: 4px; }
    .charts { display: grid; grid-template-columns: repeat(auto-fit, minmax(360px, 1fr)); gap: 20px; }
    .chart-box { background: #fff; padding: 16px; border-radius: 8px; box-shadow: 0 2px 6px rgba(0,0,0,0.08); min-width: 0; }
    .chart-wrapper { position: relative; width: 100%; height: 320px; }
    .warn { color: #d62728; font-weight: bold; display: none; }
  </style>
</head>
<body>
  <h2>TCS34725 LED Spectral Monitor (ESP32-S3)</h2>

  <!-- แถบแจ้งเตือนเมื่อเซนเซอร์หลุด -->
  <div id="sensorAlert" class="alert-banner">
    ❌ ไม่พบเซนเซอร์ TCS34725 (ถูกถอดออกหรือสาย I2C หลุด) — กำลังรอการเชื่อมต่อใหม่...
  </div>

  <div class="controls">
    <button id="toggleBtn" onclick="togglePause()">Pause</button>
    <span>API: <a href="/api/data" target="_blank">/api/data</a></span>
    <span id="satWarn" class="warn">⚠️ ค่าแสงทะลุเพดาน (Saturated) กรุณาขยับหลอดออกห่าง</span>
  </div>

  <!-- แผงควบคุม RGB LED บนบอร์ด -->
  <div class="panel">
    <span class="panel-title">Onboard RGB LED:</span>
    <button id="btnAuto" class="active-mode" onclick="setLedMode('auto')">Auto (ตามเซนเซอร์)</button>
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

  <div class="cards">
    <div class="card"><div class="label">Color Preview</div><div id="swatch" class="swatch"></div></div>
    <div class="card"><div class="label">Raw (R / G / B / C)</div><div id="rawVal" class="val">-</div></div>
    <div class="card"><div class="label">RGB (0-255)</div><div id="rgbVal" class="val">-</div></div>
    <div class="card"><div class="label">Color Temp</div><div id="cctVal" class="val">- K</div></div>
    <div class="card"><div class="label">Luminance</div><div id="luxVal" class="val">- Lux</div></div>
  </div>

  <div class="charts">
    <div class="chart-box">
      <div class="chart-wrapper"><canvas id="barChart"></canvas></div>
    </div>
    <div class="chart-box">
      <div class="chart-wrapper"><canvas id="curveChart"></canvas></div>
    </div>
  </div>

  <script>
    let paused = false;
    let currentLedMode = 'auto';
    let lastUserPickTime = 0;

    const commonScales = {
      x: {
        type: 'linear', min: 380, max: 700,
        offset: false,
        title: { display: true, text: 'Wavelength (nm)' },
        grid: { color: 'rgba(0,0,0,0.08)' }
      },
      y: {
        min: 0, suggestedMax: 1100,
        title: { display: true, text: 'Raw Intensity' },
        grid: { color: 'rgba(0,0,0,0.08)' }
      }
    };

    const barChart = new Chart(document.getElementById('barChart'), {
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
          title: { display: true, text: '1. Discrete Bar Chart (ค่าจริงจากเซนเซอร์)', font: { size: 15 } },
          legend: { display: false }
        },
        scales: commonScales
      }
    });

    const curveChart = new Chart(document.getElementById('curveChart'), {
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
          title: { display: true, text: '2. Approximated Spectral Curve (ประมาณการ)', font: { size: 15 } }
        },
        scales: commonScales
      }
    });

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
      const isAuto = (mode === 'auto');
      document.getElementById('btnAuto').className = isAuto ? 'active-mode' : 'secondary';
      document.getElementById('btnManual').className = !isAuto ? 'active-mode' : 'secondary';
      document.getElementById('manualControls').classList.toggle('disabled', isAuto);
    }

    async function setLedMode(mode) {
      updateLedUI(mode);
      if (mode === 'auto') {
        await fetch('/api/led?mode=auto');
      } else {
        pickColor(document.getElementById('colorPicker').value);
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

    async function fetchData() {
      if (paused) return;
      try {
        const res = await fetch('/api/data');
        const d = await res.json();

        const alertBox = document.getElementById('sensorAlert');

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

        if (!d.sensor_connected) {
          alertBox.style.display = 'block';
          document.getElementById('rawVal').textContent = 'Disconnected';
          document.getElementById('rgbVal').textContent = '-';
          document.getElementById('cctVal').textContent = '- K';
          document.getElementById('luxVal').textContent = '- Lux';
          document.getElementById('swatch').style.backgroundColor = '#e9ecef';
          document.getElementById('satWarn').style.display = 'none';
        } else {
          alertBox.style.display = 'none';
          document.getElementById('rawVal').textContent = `${d.raw.r} / ${d.raw.g} / ${d.raw.b} / ${d.raw.c}`;
          document.getElementById('rgbVal').textContent = `${d.rgb.r}, ${d.rgb.g}, ${d.rgb.b}`;
          document.getElementById('cctVal').textContent = `${d.color_temp_k} K`;
          document.getElementById('luxVal').textContent = `${d.lux} Lux`;
          document.getElementById('swatch').style.backgroundColor = `rgb(${d.rgb.r}, ${d.rgb.g}, ${d.rgb.b})`;
          document.getElementById('satWarn').style.display = d.saturated ? 'inline' : 'none';
        }

        barChart.data.datasets[0].data = [
          {x: 465, y: d.raw.b},
          {x: 525, y: d.raw.g},
          {x: 615, y: d.raw.r}
        ];
        barChart.update();

        curveChart.data.datasets[0].data = [
          {x: 380, y: 0},
          {x: 465, y: d.raw.b},
          {x: 525, y: d.raw.g},
          {x: 615, y: d.raw.r},
          {x: 700, y: 0}
        ];
        curveChart.data.datasets[1].data = [
          {x: 465, y: d.raw.b},
          {x: 525, y: d.raw.g},
          {x: 615, y: d.raw.r}
        ];
        curveChart.update();
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