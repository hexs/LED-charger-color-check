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
    button { padding: 8px 16px; border: none; border-radius: 6px; background: #007bff; color: #fff; cursor: pointer; font-size: 14px; }
    button.paused { background: #dc3545; }
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

  <div class="controls">
    <button id="toggleBtn" onclick="togglePause()">Pause</button>
    <span>API Endpoint: <a href="/api/data" target="_blank">/api/data</a></span>
    <span id="satWarn" class="warn">⚠️ ค่าแสงทะลุเพดาน (Saturated) กรุณาขยับหลอดออกห่าง</span>
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

    const commonScales = {
      x: {
        type: 'linear', min: 380, max: 700,
        title: { display: true, text: 'Wavelength (nm)' },
        grid: { color: 'rgba(0,0,0,0.08)' }
      },
      y: {
        min: 0, suggestedMax: 1100,
        title: { display: true, text: 'Raw Intensity' },
        grid: { color: 'rgba(0,0,0,0.08)' }
      }
    };

    // 1. กราฟแท่ง 3 จุด (Discrete Bar Chart)
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

    // 2. กราฟเส้นโค้งประมาณการ (Approximated Spectral Curve)
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

    function togglePause() {
      paused = !paused;
      const btn = document.getElementById('toggleBtn');
      btn.textContent = paused ? 'Resume' : 'Pause';
      btn.classList.toggle('paused', paused);
    }

    async function fetchData() {
      if (paused) return;
      try {
        const res = await fetch('/api/data');
        const d = await res.json();

        // อัปเดตตัวเลขและกล่องสี
        document.getElementById('rawVal').textContent = `${d.raw.r} / ${d.raw.g} / ${d.raw.b} / ${d.raw.c}`;
        document.getElementById('rgbVal').textContent = `${d.rgb.r}, ${d.rgb.g}, ${d.rgb.b}`;
        document.getElementById('cctVal').textContent = `${d.color_temp_k} K`;
        document.getElementById('luxVal').textContent = `${d.lux} Lux`;
        document.getElementById('swatch').style.backgroundColor = `rgb(${d.rgb.r}, ${d.rgb.g}, ${d.rgb.b})`;
        document.getElementById('satWarn').style.display = d.saturated ? 'inline' : 'none';

        // อัปเดตกราฟแท่ง
        barChart.data.datasets[0].data = [
          {x: 465, y: d.raw.b},
          {x: 525, y: d.raw.g},
          {x: 615, y: d.raw.r}
        ];
        barChart.update();

        // อัปเดตกราฟเส้นโค้ง
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

    setInterval(fetchData, 300); // ดึงค่าใหม่ทุก 300ms
    fetchData();
  </script>
</body>
</html>
)rawliteral";