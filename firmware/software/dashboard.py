
import threading, time
from http.server import BaseHTTPRequestHandler, HTTPServer

latest = "0"
history = []

def read_sensor():
    global latest, history
    while True:
        try:
            with open("/dev/ttyRPMSG0", "r") as f:
                for line in f:
                    line = line.strip()
                    if line.startswith("DIST:"):
                        val = line.replace("DIST:", "").strip()
                        latest = val
                        history.append(float(val))
                        if len(history) > 50:
                            history.pop(0)
        except:
            time.sleep(1)

threading.Thread(target=read_sensor, daemon=True).start()

HTML = """<!DOCTYPE html>
<html>
<head>
<title>Sulaiman's Distance Sensor</title>
<meta charset="UTF-8">
<link href="https://fonts.googleapis.com/css2?family=DM+Mono:wght@300;400&family=DM+Sans:wght@200;400&display=swap" rel="stylesheet">
<style>
  * { margin: 0; padding: 0; box-sizing: border-box; }
  body {
    background: #f7f6f3;
    font-family: 'DM Sans', sans-serif;
    display: flex;
    flex-direction: column;
    align-items: center;
    justify-content: center;
    min-height: 100vh;
    color: #1a1a1a;
  }
  .container {
    width: 480px;
    padding: 48px 0;
    text-align: center;
  }
  .title {
    font-size: 36px;
    font-weight: 400;
    letter-spacing: -0.5px;
    margin-bottom: 48px;
    color: #1a1a1a;
  }
  .label {
    font-size: 11px;
    letter-spacing: 0.15em;
    text-transform: uppercase;
    color: #999;
    margin-bottom: 8px;
  }
  .reading {
    font-family: 'DM Mono', monospace;
    font-size: 72px;
    font-weight: 300;
    line-height: 1;
    margin-bottom: 4px;
  }
  .unit {
    font-size: 13px;
    color: #999;
    font-family: 'DM Mono', monospace;
    margin-bottom: 48px;
  }
  .divider {
    width: 100%;
    height: 1px;
    background: #e0ddd8;
    margin-bottom: 32px;
  }
  canvas {
    width: 100%;
    height: 140px;
    display: block;
  }
  .graph-label {
    font-size: 10px;
    letter-spacing: 0.12em;
    text-transform: uppercase;
    color: #bbb;
    margin-top: 12px;
  }
</style>
</head>
<body>
<div class="container">
  <div class="title">Sulaiman's Distance Sensor</div>
  <div class="label">Current Reading</div>
  <div class="reading" id="val">--</div>
  <div class="unit">centimeters</div>
  <div class="divider"></div>
  <canvas id="graph" width="480" height="140"></canvas>
  <div class="graph-label">Last 50 readings</div>
</div>
<script>
  const canvas = document.getElementById('graph');
  const ctx = canvas.getContext('2d');

  function drawGraph(data) {
    const W = canvas.width, H = canvas.height;
    ctx.clearRect(0, 0, W, H);
    if (data.length < 2) return;
    const max = Math.max(...data, 10);
    const min = Math.min(...data);
    const pad = 8;
    ctx.beginPath();
    data.forEach((v, i) => {
      const x = (i / (data.length - 1)) * W;
      const y = H - pad - ((v - min) / (max - min + 1)) * (H - pad * 2);
      i === 0 ? ctx.moveTo(x, y) : ctx.lineTo(x, y);
    });
    ctx.strokeStyle = '#1a1a1a';
    ctx.lineWidth = 1.5;
    ctx.stroke();
  }

  function update() {
    fetch('/data').then(r => r.json()).then(d => {
      document.getElementById('val').innerText = parseFloat(d.latest).toFixed(1);
      drawGraph(d.history);
    });
  }

  setInterval(update, 400);
  update();
</script>
</body>
</html>"""

class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path == '/data':
            import json
            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.end_headers()
            self.wfile.write(json.dumps({'latest': latest, 'history': history}).encode())
        else:
            self.send_response(200)
            self.send_header('Content-Type', 'text/html')
            self.end_headers()
            self.wfile.write(HTML.encode())
    def log_message(self, *args):
        pass

HTTPServer(('0.0.0.0', 8080), Handler).serve_forever()
