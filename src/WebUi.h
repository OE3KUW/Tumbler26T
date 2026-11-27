#ifndef WebUi_h
#define WebUi_h

// ===========================================================================
//  Weboberflaeche
//
//  Gehoert NICHT zum Original - der ATmega328 hat weder WLAN noch Speicher
//  dafuer. An der Regelung aendert sie nichts: sie liest die Groessen aus
//  BalanceCar.h und schreibt Tastenzeichen in dieselbe Variable, die auch die
//  serielle Konsole benutzt. Webserver und WLAN laufen auf Kern 0, die
//  Regelung in ihrer eigenen Task auf Kern 1.
//
//  Die Reglerwerte sind einstellbar, weil genau das die interessante Frage
//  ist: wie schlaegt sich die Vorlage mit IHREN Werten gegenueber einer auf
//  diesen Roboter abgestimmten Fassung. Beim Neustart stehen wieder die
//  Originalwerte da - hier wird nichts dauerhaft gespeichert.
// ===========================================================================

#include "Bt.h"

#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

AsyncWebServer webServer(80);
AsyncWebSocket webSocket("/ws");

extern char key_value;

// -1 = nichts zu tun, 0 = ausschalten, 1 = einschalten. Wird in loop() erledigt.
volatile int bt_request = -1;

const char TUMBLLER_PAGE[] = R"HTML(
<!DOCTYPE html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="theme-color" content="#12100e">
<meta name="mobile-web-app-capable" content="yes">
<meta name="apple-mobile-web-app-capable" content="yes">
<meta name="apple-mobile-web-app-status-bar-style" content="black-translucent">
<link rel="manifest" href="/manifest.webmanifest">
<link rel="icon" href="/icon.svg" type="image/svg+xml">
<link rel="apple-touch-icon" href="/icon.svg">
<title>Tumbller</title>
<style>
:root{--bg:#12100e;--card:#1e1b17;--line:#332e28;--fg:#f0ebe4;--dim:#9a8f82;
  --ok:#7fb069;--warn:#e0a020;--bad:#d1495b;--acc:#c9a227}
*{box-sizing:border-box}
html{-webkit-text-size-adjust:100%}
button{touch-action:manipulation}          /* kein Doppeltipp-Zoom auf Knoepfen */
body{overscroll-behavior:none;             /* kein Ueberziehen zum Neuladen */
  padding-bottom:calc(12px + env(safe-area-inset-bottom))}
body{font-family:system-ui,sans-serif;background:var(--bg);color:var(--fg);
  margin:0 auto;padding:12px;max-width:640px}
header{display:flex;align-items:baseline;justify-content:space-between;margin-bottom:4px}
h1{font-size:1.1em;margin:0;font-weight:600}
.tag{font-size:.7em;color:var(--dim)}
#conn{font-size:.75em;color:var(--dim);display:flex;align-items:center;gap:6px}
#dot{width:8px;height:8px;border-radius:50%;background:var(--bad)}
#dot.up{background:var(--ok)}
.card{background:var(--card);border:1px solid var(--line);border-radius:10px;
  padding:12px;margin-bottom:12px}
.top{display:flex;align-items:center;gap:16px}
.readout{flex:1}
#mode{font-size:.7em;letter-spacing:.08em;padding:3px 9px;border-radius:4px;
  display:inline-block;background:#3a352e;color:var(--dim);font-weight:600}
#mode.on{background:var(--ok);color:#0d1a08}
#mode.stop{background:var(--bad);color:#fff}
#angle{font-size:2.1em;font-weight:700;line-height:1.1;margin:4px 0 2px;
  font-variant-numeric:tabular-nums}
.sub{font-size:.8em;color:var(--dim);font-variant-numeric:tabular-nums}
canvas{width:100%;display:block;border-radius:6px}
.legend{display:flex;gap:14px;font-size:.72em;color:var(--dim);margin-top:6px}
.legend i{display:inline-block;width:9px;height:3px;border-radius:2px;
  margin-right:4px;vertical-align:middle}
.row{display:flex;gap:8px;margin-bottom:12px}
button{flex:1;padding:13px 4px;font-size:.92em;font-weight:600;border:0;
  border-radius:8px;background:#3a352e;color:var(--fg);cursor:pointer}
button.go{background:var(--ok);color:#0d1a08}
button.halt{background:var(--bad);color:#fff}
button.on{background:var(--acc);color:#12100e}
h2{font-size:.75em;text-transform:uppercase;letter-spacing:.08em;
  color:var(--dim);margin:0 0 10px;font-weight:600}
.p{margin-bottom:11px}
.plabel{display:flex;justify-content:space-between;font-size:.85em;margin-bottom:4px}
.pval{color:var(--acc);font-variant-numeric:tabular-nums;font-weight:600}
.orig{color:var(--dim);font-weight:400;font-size:.85em}
input[type=range]{width:100%;accent-color:var(--acc);margin:0}
</style></head><body>

<header>
  <div><h1>Tumbller</h1>
    <div class="tag">Portierung von OE3KUW/TumbllerVSC auf den ESP32</div></div>
  <div id="conn"><span id="dot"></span><span id="connt">verbinde...</span></div>
</header>

<div class="card">
  <div class="top">
    <svg viewBox="0 0 100 100" width="84" height="84" style="flex:0 0 84px">
      <circle cx="50" cy="50" r="46" fill="none" stroke="#332e28" stroke-width="2"/>
      <line x1="50" y1="50" x2="50" y2="96" stroke="#332e28" stroke-width="2"/>
      <g id="bot"><rect x="44" y="14" width="12" height="38" rx="3" fill="#c9a227"/>
        <circle cx="50" cy="58" r="9" fill="none" stroke="#9a8f82" stroke-width="3"/></g>
    </svg>
    <div class="readout">
      <span id="mode">--</span>
      <div id="angle">--.--&deg;</div>
      <div class="sub" id="sub">pwm --/-- &middot; Schritt --</div>
      <div class="sub" id="msg" style="color:var(--acc);min-height:1.2em"></div>
    </div>
  </div>
</div>

<div class="card">
  <canvas id="chart" height="150"></canvas>
  <div class="legend">
    <span><i style="background:#7fb069"></i>Winkel &plusmn;10&deg;</span>
    <span><i style="background:#c9a227"></i>pwm links &plusmn;255</span>
    <span><i style="background:#d1495b"></i>pwm rechts &plusmn;255</span>
  </div>
</div>

<div class="row">
  <button class="go" onclick="cmd('5')">START</button>
  <button onclick="cmd('s')">STANDBY</button>
  <button class="halt" onclick="cmd('x')">STOP</button>
  <button onclick="cmd('z')" title="Nur im Stillstand, aufrecht und ruhig halten">ZERO</button>
</div>
<div class="card"><h2>Bluetooth</h2>
  <div class="toggle" style="display:flex;gap:8px">
    <button id="btOn">ein</button><button id="btOff">aus</button>
  </div>
  <div class="sub" id="btState" style="margin-top:8px">--</div>
  <div class="sub">Koppeln als <b>Tumbller26</b>, dann mit einer
    Terminal-App verbinden &mdash; dieselben Tasten wie hier.
    <b>Solange Bluetooth l&auml;uft, ist die Aktualisierung &uuml;ber Funk
    unzuverl&auml;ssig</b>: beide teilen sich eine Antenne.</div>
</div>

<div class="card"><h2>Winkelfilter</h2>
  <div class="toggle" style="display:flex;gap:8px">
    <button id="kalOn">Kalman</button><button id="kalOff">Komplement&auml;r</button>
  </div>
  <div class="sub" style="margin-top:8px">Beide rechnen immer mit, umgeschaltet
    wird nur, welcher den Regler speist. <code>Yiorderfilter()</code> ist in der
    Vorlage vorhanden und wird dort nie aufgerufen.</div>
</div>
<div class="row">
  <button onclick="cmd('l')">&#8634; links</button>
  <button onclick="cmd('f')">&#9650; vor</button>
  <button onclick="cmd('b')">&#9660; zur&uuml;ck</button>
  <button onclick="cmd('i')">rechts &#8635;</button>
</div>

<div class="card"><h2>Reglerwerte der Vorlage</h2><div id="g"></div>
  <div class="sub">Die grauen Zahlen sind die Werte des Originals. &Auml;nderungen
    wirken sofort, werden aber nicht gespeichert &mdash; nach einem Neustart
    steht wieder die Vorlage da.</div>
</div>

<script>
// [id, Kommando, Original, min, max, Schritt, Nachkommastellen]
const P = [
  ['kp_balance','KPB',55,0,120,1,0], ['kd_balance','KDB',0.75,0,3,0.05,2],
  ['kp_speed','KPS',10,0,40,0.5,1],  ['ki_speed','KIS',0.26,0,1,0.01,2],
  ['kp_turn','KPT',2.5,0,10,0.1,1],  ['kd_turn','KDT',0.5,0,3,0.05,2]
];
const box = document.getElementById('g');
for (const [id,c,orig,mn,mx,st,dec] of P) {
  const d = document.createElement('div');
  d.className = 'p';
  d.innerHTML = '<div class="plabel"><span>'+id+
    ' <span class="orig">(Original '+orig+')</span></span>'+
    '<span class="pval" id="v'+id+'">--</span></div>'+
    '<input type="range" id="'+id+'" min="'+mn+'" max="'+mx+'" step="'+st+'">';
  box.appendChild(d);
  const sl = document.getElementById(id);
  // Anzeigen beim Ziehen, senden erst beim Loslassen - sonst flutet ein
  // Schieberegler den Anschluss mit hunderten Kommandos.
  sl.addEventListener('input', e =>
    document.getElementById('v'+id).textContent = (+e.target.value).toFixed(dec));
  sl.addEventListener('change', e => cmd(c+'='+e.target.value));
}

const N = 240, hist = {a:[], l:[], r:[]};
const cv = document.getElementById('chart'), cx = cv.getContext('2d');
const push = (arr,v) => { arr.push(v); if (arr.length > N) arr.shift(); };
function draw() {
  const w = cv.width = cv.clientWidth * devicePixelRatio;
  const h = cv.height = 150 * devicePixelRatio;
  cx.clearRect(0,0,w,h);
  cx.strokeStyle = '#332e28'; cx.lineWidth = 1;
  for (let i = 0; i <= 4; i++) {
    const y = h*i/4;
    cx.globalAlpha = i === 2 ? 1 : .45;
    cx.beginPath(); cx.moveTo(0,y); cx.lineTo(w,y); cx.stroke();
  }
  cx.globalAlpha = 1; cx.lineWidth = 1.6*devicePixelRatio;
  const trace = (arr,scale,col) => {
    if (arr.length < 2) return;
    cx.strokeStyle = col; cx.beginPath();
    arr.forEach((v,i) => {
      const x = w*i/(N-1);
      const y = h/2 - Math.max(-1,Math.min(1,v/scale))*h/2*.92;
      i ? cx.lineTo(x,y) : cx.moveTo(x,y);
    });
    cx.stroke();
  };
  trace(hist.l,255,'#c9a227'); trace(hist.r,255,'#d1495b'); trace(hist.a,10,'#7fb069');
}
addEventListener('resize', draw);

let ws;
function cmd(c){ if (ws && ws.readyState === 1) ws.send(c); }
function connect(){
  ws = new WebSocket('ws://'+location.host+'/ws');
  ws.onopen  = () => { document.getElementById('dot').className='up';
                       document.getElementById('connt').textContent='verbunden'; };
  ws.onclose = () => { document.getElementById('dot').className='';
                       document.getElementById('connt').textContent='getrennt';
                       setTimeout(connect,1000); };
  ws.onmessage = e => {
    const p = e.data.split(',');
    if (p[0] === 'M') {
      const m = document.getElementById('msg');
      m.textContent = p.slice(1).join(',');
      clearTimeout(m._t);
      m._t = setTimeout(() => m.textContent = '', 6000);
      return;
    }
    if (p[0] !== 'T') return;
    // T,mode,winkel,pwmL,pwmR,us,kpb,kdb,kps,kis,kpt,kdt
    const ang = +p[2], l = +p[3], r = +p[4];
    document.getElementById('angle').textContent = ang.toFixed(2)+'°';
    document.getElementById('sub').textContent =
      'pwm '+l.toFixed(0)+' / '+r.toFixed(0)+' · Richtung '+(+p[12]).toFixed(1)+'° · Null '+(+p[13]).toFixed(2)+'° · Schritt '+p[5]+' µs';
    document.getElementById('bot').setAttribute('transform',
      'rotate('+(-ang).toFixed(1)+' 50 58)');
    const m = document.getElementById('mode');
    m.textContent = p[1];
    m.className = p[1]==='STOP' ? 'stop' : (p[1]==='STANDBY' ? 'on' : '');
    push(hist.a,ang); push(hist.l,l); push(hist.r,r); draw();
    const bs = p[15];
    document.getElementById('btState').textContent =
      bs === '2' ? 'verbunden' : (bs === '1' ? 'an, wartet auf Verbindung' : 'aus');
    document.getElementById('btOn').className  = bs === '0' ? '' : 'on';
    document.getElementById('btOff').className = bs === '0' ? 'on' : '';
    const kal = p[14] === '1';
    document.getElementById('kalOn').className  = kal ? 'on' : '';
    document.getElementById('kalOff').className = kal ? '' : 'on';
    const vals = [p[6],p[7],p[8],p[9],p[10],p[11]];
    P.forEach(([id,,,,,,dec],i) => {
      const el = document.getElementById(id);
      if (el !== document.activeElement) {
        el.value = vals[i];
        document.getElementById('v'+id).textContent = (+vals[i]).toFixed(dec);
      }
    });
  };
}
document.getElementById('btOn').onclick  = () => cmd('BT=1');
document.getElementById('btOff').onclick = () => cmd('BT=0');
document.getElementById('kalOn').onclick  = () => cmd('KAL=1');
document.getElementById('kalOff').onclick = () => cmd('KAL=0');
connect();
</script>
</body></html>
)HTML";

// Kurze Rueckmeldung an die Seite. Ohne die laufen Ablehnungen ins Leere -
// der Knopf sieht dann kaputt aus, obwohl er sauber antwortet.
inline void webUiNotify(const String &text)
{
  if (webSocket.count() == 0) return;
  webSocket.textAll(String("M,") + text);
}

inline void webUiSend()
{
  if (webSocket.count() == 0) return;
  static const char *mn[] = {"STANDBY","FORWARD","BACKWARD","TURNLEFT",
                             "TURNRIGHT","STOP","START"};
  char buf[220];
  snprintf(buf, sizeof(buf), "T,%s,%.2f,%.0f,%.0f,%lu,%.1f,%.2f,%.1f,%.2f,%.1f,%.2f,%.1f,%.2f,%d,%d",
           mn[motion_mode], kalmanfilter_angle, pwm_left, pwm_right,
           (unsigned long)balance_us,
           kp_balance, kd_balance, kp_speed, ki_speed, kp_turn, kd_turn,
           yaw_angle, angle_zero, use_kalman ? 1 : 0, btState());
  webSocket.textAll(buf);
}

inline void webUiEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
                       AwsEventType type, void *arg, uint8_t *data, size_t len)
{
  if (type != WS_EVT_DATA) return;
  AwsFrameInfo *info = (AwsFrameInfo *)arg;
  if (!info->final || info->index != 0 || info->len != len) return;
  if (info->opcode != WS_TEXT) return;

  String msg;
  msg.reserve(len);
  for (size_t i = 0; i < len; i++) msg += (char)data[i];

  if (msg.length() == 1) { key_value = msg[0]; return; }

  // Reglerwerte: wirken sofort, werden nicht gespeichert.
  const float v = msg.substring(msg.indexOf('=') + 1).toFloat();
  if      (msg.startsWith("KPB=")) kp_balance = v;
  else if (msg.startsWith("KDB=")) kd_balance = v;
  else if (msg.startsWith("KPS=")) kp_speed   = v;
  else if (msg.startsWith("KIS=")) ki_speed   = v;
  else if (msg.startsWith("KPT=")) kp_turn    = v;
  else if (msg.startsWith("KDT=")) kd_turn    = v;
  else if (msg.startsWith("BT="))
  {
    // Nur vormerken, nicht hier ausfuehren: bt.begin() startet den ganzen
    // Bluetooth-Stapel und braucht dafuer hunderte Millisekunden. Aus der
    // Webserver-Task heraus aufgerufen passierte schlicht nichts - die
    // Umschaltung gehoert in die Hauptschleife.
    bt_request = (v != 0) ? 1 : 0;
  }
  else if (msg.startsWith("KAL="))
  {
    use_kalman = v != 0;
    webUiNotify(use_kalman ? "Winkelfilter: Kalman" : "Winkelfilter: Komplementaer");
  }
}

// Damit sich die Seite auf dem Handy wie eine App auf den Startbildschirm
// legen laesst: ein Manifest und ein Symbol. Das Symbol ist ein SVG - ein
// paar hundert Byte statt eines PNG-Satzes in mehreren Groessen.
const char TUMBLLER_MANIFEST[] = R"JSON({
  "name": "Tumbller",
  "short_name": "Tumbller",
  "description": "Balancierroboter tumlerV2",
  "start_url": "/",
  "display": "standalone",
  "orientation": "portrait",
  "background_color": "#12100e",
  "theme_color": "#12100e",
  "icons": [
    { "src": "/icon.svg", "sizes": "any", "type": "image/svg+xml", "purpose": "any maskable" }
  ]
})JSON";

// Der Roboter als Strichzeichnung: Aufbau, Achse, zwei Raeder.
const char TUMBLLER_ICON[] = R"SVG(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 192 192">
<rect width="192" height="192" rx="42" fill="#12100e"/>
<rect x="84" y="34" width="24" height="74" rx="7" fill="#c9a227"/>
<circle cx="96" cy="126" r="26" fill="none" stroke="#f0ebe4" stroke-width="9"/>
<circle cx="96" cy="126" r="5" fill="#f0ebe4"/>
<path d="M40 126h22M130 126h22" stroke="#7fb069" stroke-width="9" stroke-linecap="round"/>
</svg>)SVG";

inline void webUiBegin()
{
  webSocket.onEvent(webUiEvent);
  webServer.addHandler(&webSocket);
  webServer.on("/", HTTP_GET, [](AsyncWebServerRequest *r) {
    r->send(200, "text/html", TUMBLLER_PAGE);
  });
  webServer.on("/manifest.webmanifest", HTTP_GET, [](AsyncWebServerRequest *r) {
    r->send(200, "application/manifest+json", TUMBLLER_MANIFEST);
  });
  webServer.on("/icon.svg", HTTP_GET, [](AsyncWebServerRequest *r) {
    r->send(200, "image/svg+xml", TUMBLLER_ICON);
  });
  webServer.begin();
}

inline void webUiLoop()
{
  static uint32_t last = 0;
  if (millis() - last >= 1000) { last = millis(); webSocket.cleanupClients(); }
}

#endif
