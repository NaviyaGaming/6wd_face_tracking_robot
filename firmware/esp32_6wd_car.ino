#include <WiFi.h>
#include <WebServer.h>

// =====================================================================
//  WIFI
// =====================================================================
const char* ssid     = "daminda";
const char* password = "theja123";
WebServer server(80);

// =====================================================================
//  MOTOR PINS  (BTS7960)
// =====================================================================
#define L_RPWM 25
#define L_LPWM 26
#define L_REN  18
#define L_LEN  21
#define R_RPWM 27
#define R_LPWM 14
#define R_REN  13
#define R_LEN  12

// =====================================================================
//  MOTOR PWM CHANNELS
// =====================================================================
#define PWM_FREQ   1000
#define PWM_RES    8
#define CH_L_FWD   0
#define CH_L_REV   1
#define CH_R_FWD   2
#define CH_R_REV   3

// =====================================================================
//  SPEED & RAMP
// =====================================================================
int  targetSpeed   = 60;   // forward/backward speed
int  turnSpeed      = 85;   // left/right turning speed (independent)
int  currentSpeedL = 0;
int  currentSpeedR = 0;
#define RAMP_STEP    6
#define RAMP_TICK_MS 20

// =====================================================================
//  BRAKING
// =====================================================================
#define BRAKE_PWM  120
#define BRAKE_MS    80

// =====================================================================
//  RAMP TASK (Core 0)
// =====================================================================
int  rampTargetL = 0, rampTargetR = 0;
bool rampRunning = false;
SemaphoreHandle_t rampMutex;

void writePWM(int l, int r) {
  ledcWrite(CH_L_FWD, l > 0 ?  l : 0);
  ledcWrite(CH_L_REV, l < 0 ? -l : 0);
  int ri = -r;
  ledcWrite(CH_R_FWD, ri > 0 ?  ri : 0);
  ledcWrite(CH_R_REV, ri < 0 ? -ri : 0);
  currentSpeedL = l;
  currentSpeedR = r;
}

void rampTask(void* pv) {
  for (;;) {
    if (rampRunning) {
      xSemaphoreTake(rampMutex, portMAX_DELAY);
      int tl = rampTargetL, tr = rampTargetR;
      xSemaphoreGive(rampMutex);
      int l = currentSpeedL, r = currentSpeedR;
      bool done = true;
      if      (l < tl) { l = min(l + RAMP_STEP, tl); done = false; }
      else if (l > tl) { l = max(l - RAMP_STEP, tl); done = false; }
      if      (r < tr) { r = min(r + RAMP_STEP, tr); done = false; }
      else if (r > tr) { r = max(r - RAMP_STEP, tr); done = false; }
      writePWM(l, r);
      if (done) rampRunning = false;
    }
    vTaskDelay(pdMS_TO_TICKS(RAMP_TICK_MS));
  }
}

void setTarget(int l, int r) {
  xSemaphoreTake(rampMutex, portMAX_DELAY);
  rampTargetL = l; rampTargetR = r; rampRunning = true;
  xSemaphoreGive(rampMutex);
}

void activeBrake() {
  xSemaphoreTake(rampMutex, portMAX_DELAY);
  rampRunning = false;
  xSemaphoreGive(rampMutex);
  int brakePow = (currentSpeedL > 0 || currentSpeedR > 0) ? BRAKE_PWM : 0;
  writePWM(-brakePow, -brakePow);
  delay(BRAKE_MS);
  writePWM(0, 0);
  xSemaphoreTake(rampMutex, portMAX_DELAY);
  rampTargetL = rampTargetR = 0;
  xSemaphoreGive(rampMutex);
}

// =====================================================================
//  ENABLE / DISABLE
// =====================================================================
void enableDrivers() {
  digitalWrite(L_REN,HIGH); digitalWrite(L_LEN,HIGH);
  digitalWrite(R_REN,HIGH); digitalWrite(R_LEN,HIGH);
}
void disableDrivers() {
  digitalWrite(L_REN,LOW);  digitalWrite(L_LEN,LOW);
  digitalWrite(R_REN,LOW);  digitalWrite(R_LEN,LOW);
}

// =====================================================================
//  MOVEMENT
// =====================================================================
void forward()  { enableDrivers(); setTarget( targetSpeed,  targetSpeed); }
void backward() { enableDrivers(); setTarget(-targetSpeed, -targetSpeed); }
void turnLeft() { enableDrivers(); setTarget( turnSpeed, -turnSpeed); }
void turnRight(){ enableDrivers(); setTarget(-turnSpeed,  turnSpeed); }
void stopCar()  { activeBrake();   disableDrivers(); }

// =====================================================================
//  CORS
// =====================================================================
void cors() {
  server.sendHeader("Access-Control-Allow-Origin",  "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

// =====================================================================
//  WEB PAGE
// =====================================================================
void handleRoot() {
  const char* html = R"HTML(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no">
<title>6WD Robot</title>
<style>
  :root{--bg:#0a0a0f;--panel:#12121a;--border:#1e1e2e;
        --accent:#00c8ff;--accent2:#7c3aed;--red:#ff4060;--text:#e2e8f0;--muted:#64748b;}
  *{box-sizing:border-box;margin:0;padding:0;-webkit-tap-highlight-color:transparent}
  body{background:var(--bg);color:var(--text);font-family:'Segoe UI',system-ui,sans-serif;
       min-height:100dvh;display:flex;flex-direction:column;align-items:center;
       padding:24px 16px 32px;gap:20px}
  h1{font-size:1.4rem;font-weight:700;letter-spacing:5px;text-transform:uppercase;
     background:linear-gradient(90deg,var(--accent),var(--accent2));
     -webkit-background-clip:text;-webkit-text-fill-color:transparent}
  .subtitle{font-size:.65rem;color:#555;letter-spacing:2px}
  .statusbar{display:flex;align-items:center;gap:10px;background:var(--panel);
             border:1px solid var(--border);border-radius:999px;padding:6px 18px;
             font-size:.72rem;letter-spacing:1px;color:var(--muted)}
  .dot{width:8px;height:8px;border-radius:50%;background:#22c55e;box-shadow:0 0 6px #22c55e}
  #cmdlabel{color:var(--text);min-width:80px;text-align:center}
  .speed-section{width:100%;max-width:340px;background:var(--panel);
                 border:1px solid var(--border);border-radius:16px;padding:16px 20px}
  .speed-header{display:flex;justify-content:space-between;align-items:baseline;margin-bottom:12px}
  .speed-header span:first-child{font-size:.62rem;letter-spacing:3px;text-transform:uppercase;color:var(--muted)}
  #speedDisplay{font-size:1.3rem;font-weight:700;color:var(--accent);font-variant-numeric:tabular-nums}
  input[type=range]{width:100%;-webkit-appearance:none;appearance:none;background:transparent;cursor:pointer}
  input[type=range]::-webkit-slider-runnable-track{height:8px;border-radius:99px;
    background:linear-gradient(90deg,var(--accent2) calc(var(--pct)*1%),#1e1e2e calc(var(--pct)*1%))}
  input[type=range]::-webkit-slider-thumb{-webkit-appearance:none;width:22px;height:22px;
    border-radius:50%;background:var(--accent);box-shadow:0 0 8px var(--accent);margin-top:-7px}
  .dpad{display:grid;grid-template-columns:repeat(3,76px);grid-template-rows:repeat(3,68px);gap:10px}
  .btn{background:var(--panel);border:1px solid var(--border);border-radius:14px;color:var(--text);
       font-size:.58rem;letter-spacing:1.5px;text-transform:uppercase;display:flex;flex-direction:column;
       align-items:center;justify-content:center;gap:4px;cursor:pointer;user-select:none;
       transition:background .1s,border-color .1s,transform .08s}
  .btn .ico{font-size:1.4rem;line-height:1}
  .btn:active,.btn.active{background:rgba(0,200,255,.12);border-color:var(--accent);color:var(--accent);transform:scale(.94)}
  .btn.stop-btn{border-color:rgba(255,64,96,.35);color:var(--red)}
  .btn.stop-btn:active{background:rgba(255,64,96,.12);border-color:var(--red)}
  .empty{pointer-events:none}
  footer{font-size:.58rem;color:#2a2a3a;letter-spacing:2px}
</style>
</head>
<body>
<h1>6WD Robot</h1>
<p class="subtitle">ESP32 · BTS7960 · WiFi</p>
<div class="statusbar">
  <div class="dot"></div>
  <span>CMD</span>
  <span id="cmdlabel">READY</span>
</div>
<div class="speed-section">
  <div class="speed-header"><span>Speed</span><span id="speedDisplay">60</span></div>
  <input type="range" id="slider" min="60" max="255" value="60" style="--pct:0" oninput="onSpeed(this)">
</div>
<div class="dpad">
  <div class="empty"></div>
  <button class="btn" id="bF" onpointerdown="press('F',this)" onpointerup="release()" onpointerleave="release()">
    <span class="ico">▲</span>FORWARD</button>
  <div class="empty"></div>
  <button class="btn" id="bL" onpointerdown="press('L',this)" onpointerup="release()" onpointerleave="release()">
    <span class="ico">◀</span>LEFT</button>
  <button class="btn stop-btn" onpointerdown="cmd('S')" ontouchstart="cmd('S')">
    <span class="ico">■</span>STOP</button>
  <button class="btn" id="bR" onpointerdown="press('R',this)" onpointerup="release()" onpointerleave="release()">
    <span class="ico">▶</span>RIGHT</button>
  <div class="empty"></div>
  <button class="btn" id="bB" onpointerdown="press('B',this)" onpointerup="release()" onpointerleave="release()">
    <span class="ico">▼</span>BACK</button>
  <div class="empty"></div>
</div>
<footer>6WD · LEFT 3 MOTORS · RIGHT 3 MOTORS</footer>
<script>
let activeBtn=null, busy=false;
function cmd(c){
  fetch('/'+c).catch(()=>{});
  document.getElementById('cmdlabel').textContent=
    {F:'FORWARD',B:'BACKWARD',L:'LEFT',R:'RIGHT',S:'STOP'}[c]||c;
}
function press(c,btn){
  if(activeBtn)activeBtn.classList.remove('active');
  activeBtn=btn; btn.classList.add('active'); cmd(c);
}
function release(){
  if(activeBtn){activeBtn.classList.remove('active');activeBtn=null;} cmd('S');
}
function onSpeed(el){
  const v=parseInt(el.value);
  el.style.setProperty('--pct',Math.round((v-60)/(255-60)*100));
  document.getElementById('speedDisplay').textContent=v;
  if(!busy){busy=true;fetch('/SPD?v='+v).finally(()=>busy=false);}
}
</script>
</body>
</html>
)HTML";
  server.send(200, "text/html", html);
}

// =====================================================================
//  SETUP
// =====================================================================
void setup() {
  Serial.begin(115200);

  pinMode(L_REN,OUTPUT); pinMode(L_LEN,OUTPUT);
  pinMode(R_REN,OUTPUT); pinMode(R_LEN,OUTPUT);
  disableDrivers();

  ledcSetup(CH_L_FWD,PWM_FREQ,PWM_RES); ledcAttachPin(L_RPWM,CH_L_FWD);
  ledcSetup(CH_L_REV,PWM_FREQ,PWM_RES); ledcAttachPin(L_LPWM,CH_L_REV);
  ledcSetup(CH_R_FWD,PWM_FREQ,PWM_RES); ledcAttachPin(R_RPWM,CH_R_FWD);
  ledcSetup(CH_R_REV,PWM_FREQ,PWM_RES); ledcAttachPin(R_LPWM,CH_R_REV);
  writePWM(0,0);

  rampMutex = xSemaphoreCreateMutex();
  xTaskCreatePinnedToCore(rampTask,"ramp",2048,NULL,1,NULL,0);

  WiFi.begin(ssid,password);
  Serial.print("Connecting");
  int t=0;
  while(WiFi.status()!=WL_CONNECTED){
    delay(500); Serial.print('.');
    if(++t>40){Serial.println("\nFailed!"); return;}
  }
  Serial.println("\nIP: "+WiFi.localIP().toString());

  server.on("/",   handleRoot);
  server.on("/F",  [](){ forward();   cors(); server.send(200,"text/plain","F"); });
  server.on("/B",  [](){ backward();  cors(); server.send(200,"text/plain","B"); });
  server.on("/L",  [](){ turnLeft();  cors(); server.send(200,"text/plain","L"); });
  server.on("/R",  [](){ turnRight(); cors(); server.send(200,"text/plain","R"); });
  server.on("/S",  [](){ stopCar();   cors(); server.send(200,"text/plain","S"); });
  server.on("/SPD",[](){
    if(server.hasArg("v")) targetSpeed=constrain(server.arg("v").toInt(),0,255);
    cors(); server.send(200,"text/plain",String(targetSpeed));
  });
  server.on("/TURNSPD",[](){
    if(server.hasArg("v")) turnSpeed=constrain(server.arg("v").toInt(),0,255);
    cors(); server.send(200,"text/plain",String(turnSpeed));
  });

  server.begin();
  Serial.println("Server started.");
}

// =====================================================================
//  LOOP
// =====================================================================
void loop() {
  server.handleClient();
}
