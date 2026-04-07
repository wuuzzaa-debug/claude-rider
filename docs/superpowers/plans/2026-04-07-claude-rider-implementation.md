# Claude Rider Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** LED-Leiste (BIGTREETECH Panda Status, ESP32-C3, 25x WS2812) als physische Statusanzeige fuer Claude Code — Knight Rider Scanner beim Denken, Farb-Animationen fuer verschiedene Zustaende, gesteuert ueber Claude Code Hooks.

**Architecture:** Drei-Schichten-System: Claude Code Hooks rufen Python CLI auf → Python Daemon haelt Serial-Verbindung offen und sendet Befehle → ESP32 Firmware rendert Animationen mit Doppelpuffer-Fade-System bei 60fps.

**Tech Stack:** ESP32-C3 (Arduino/PlatformIO, Adafruit NeoPixel), Python 3.8+ (pyserial), Claude Code Hooks (settings.json)

---

## File Structure

```
claude-rider/
├── README.md                        # Setup-Anleitung fuer Open Source
├── CLAUDE.md                        # Projekt-Kontext fuer Claude Code
├── LICENSE                          # MIT Lizenz
├── config.json.example              # Beispiel-Konfiguration mit Defaults
├── firmware/
│   ├── platformio.ini               # PlatformIO Build-Konfiguration
│   ├── .gitignore                   # .pio/ Build-Artefakte ignorieren
│   └── src/
│       └── main.cpp                 # Claude Rider Firmware V3.0
├── python/
│   ├── claude_rider.py              # Controller: CLI + Daemon + Serial
│   └── test_claude_rider.py         # Tests fuer Controller
├── start_claude_rider.bat           # Windows: Installation + Start
└── stop_claude_rider.bat            # Windows: Sauber herunterfahren
```

**Verantwortlichkeiten:**
- `firmware/src/main.cpp` — Alle LED-Animationen, Serial-Protokoll, Hardware-Steuerung
- `python/claude_rider.py` — Daemon (Serial offen halten), CLI (Events senden), Config, Port-Erkennung
- `python/test_claude_rider.py` — Unit-Tests fuer Config-Parsing, Event-Mapping, CLI-Argumente
- `start_claude_rider.bat` / `stop_claude_rider.bat` — One-Click Setup und Shutdown
- `config.json.example` — Dokumentiert alle Optionen als kopierbares Template

---

## Task 1: Projekt bereinigen und Grundstruktur anlegen

**Files:**
- Delete: `panda_status_v1.0.2.bin`, `python/integration_beispiel.py`, `archiv/` (komplett), `ACTION_ITEMS_2026-03-27.md`, `ANLEITUNG.md`, `PROTOKOLL_V2.md`
- Delete: `firmware/panda_custom/` (altes Arduino-Projekt)
- Delete: `firmware/.pio/` (Build-Artefakte, wird neu gebaut)
- Create: `LICENSE`
- Modify: `firmware/.gitignore` — `.pio/` ignorieren

- [ ] **Step 1: Kundendaten und alte Dateien entfernen**

```bash
cd "j:/SynologyDrive/Claue Projekte/Status LED Claude Rider"
rm -f panda_status_v1.0.2.bin
rm -f ACTION_ITEMS_2026-03-27.md
rm -f ANLEITUNG.md
rm -f PROTOKOLL_V2.md
rm -f python/integration_beispiel.py
rm -rf archiv/
rm -rf firmware/panda_custom/
rm -rf firmware/.pio/
rm -rf firmware/.vscode/
```

- [ ] **Step 2: MIT Lizenz erstellen**

Datei `LICENSE` mit folgendem Inhalt erstellen:

```
MIT License

Copyright (c) 2026 Claude Rider Contributors

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

- [ ] **Step 3: .gitignore fuer Firmware aktualisieren**

Datei `firmware/.gitignore` mit folgendem Inhalt:

```
.pio/
.vscode/
```

- [ ] **Step 4: Commit**

```bash
git add -A
git commit -m "Projekt bereinigen: Kundendaten entfernen, MIT Lizenz, saubere Struktur"
```

---

## Task 2: Firmware — Knight Rider Animation und Rebranding

**Files:**
- Create: `firmware/src/main.cpp` (komplett neu geschrieben)
- Modify: `firmware/platformio.ini` (COM-Port entfernen)

Die Firmware wird komplett neu geschrieben, basierend auf dem bewaehrten Doppelpuffer-Fade-System.
Keine kundenspezifischen Referenzen.

- [ ] **Step 1: platformio.ini bereinigen**

Datei `firmware/platformio.ini`:

```ini
; Claude Rider Firmware
; ESP32-C3-MINI mit 25x WS2812 LEDs an GPIO5

[env:esp32c3]
platform = espressif32
board = esp32-c3-devkitm-1
framework = arduino
monitor_speed = 115200
upload_speed = 460800

lib_deps =
    adafruit/Adafruit NeoPixel@^1.12.0

build_flags =
    -DARDUINO_USB_MODE=1
    -DARDUINO_USB_CDC_ON_BOOT=0
```

- [ ] **Step 2: Firmware main.cpp komplett neu schreiben**

Datei `firmware/src/main.cpp` — komplette Firmware. Zentrale Elemente:

```cpp
/**
 * Claude Rider Firmware V3.0
 * ==========================
 * ESP32-C3-MINI | 25x WS2812 an GPIO5 | USB-Serial
 *
 * Physische Statusanzeige fuer Claude Code.
 * Knight Rider Scanner beim Denken, Farb-Animationen fuer Zustaende.
 * Alle Uebergaenge mit sanften Fades (Doppelpuffer-Interpolation, 60fps).
 *
 * Protokoll (115200 Baud, Newline-terminiert):
 *   PING                         -> PONG
 *   INFO                         -> INFO:CLAUDE_RIDER,25,V3.0
 *   STATE:<zustand>              -> OK
 *   PROGRESS:<0-100>             -> OK
 *   BRIGHTNESS:<0-255>           -> OK
 *   STATECOLOR:<st>,<r>,<g>,<b>  -> OK
 *   SPEED:<1-5>                  -> OK
 *   FLIP:<0|1>                   -> OK
 *   TIMEOUT:<sekunden>           -> OK
 *   HEARTBEAT                    -> OK
 *   CLEAR                        -> OK
 *
 * Open Source: https://github.com/xxx/claude-rider
 */

#include <Adafruit_NeoPixel.h>

// === Hardware ===
#define LED_PIN         5
#define NUM_LEDS        25
#define BUTTON_PIN      9
#define SERIAL_BAUD     115200
#define CMD_BUFFER_SIZE 64

// === Firmware ===
#define FW_NAME         "CLAUDE_RIDER"
#define FW_VERSION      "3.0"
#define FRAME_MS        16      // ~60fps

// === Fade-Geschwindigkeiten ===
#define FADE_NORMAL     0.08f
#define FADE_SLOW       0.03f
#define FADE_FAST       0.20f

Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// === Zustaende ===
enum State {
  ST_OFF,
  ST_IDLE,
  ST_KNIGHT_RIDER,    // NEU: Classic KITT Scanner
  ST_PROGRESS,
  ST_DONE,
  ST_WAITING,
  ST_ERROR,
  ST_SAVE,
  ST_CONNECT,
  ST_DISCONNECTED
};

// === Display-System (Doppelpuffer mit Interpolation) ===
float dispR[NUM_LEDS], dispG[NUM_LEDS], dispB[NUM_LEDS];
float targR[NUM_LEDS], targG[NUM_LEDS], targB[NUM_LEDS];
float currentBrightness = 60.0f;
float targetBrightness  = 60.0f;
float fadeSpeed = FADE_NORMAL;

// === Globale Variablen ===
State currentState  = ST_IDLE;
float smoothProgress = 0.0f;
float targetProgress = 0.0f;
unsigned long phase     = 0;
unsigned long lastFrame = 0;

// Knight Rider
float knightPos = 0.0f;
int   knightDir = 1;
float knightSpeed = 0.18f;   // Aggressiv (Speed-Stufe 3)
#define KNIGHT_TAIL_LEN 8

// Speed-Stufen: Index 0-4 fuer SPEED:1-5
const float SPEED_TABLE[] = { 0.06f, 0.12f, 0.18f, 0.25f, 0.35f };

// Heartbeat-Watchdog
unsigned long lastHeartbeat    = 0;
unsigned long heartbeatTimeout = 0;
State stateBeforeDisconnect    = ST_IDLE;

// Button
bool buttonPressed          = false;
unsigned long buttonDebounce = 0;

// Flash-Effekt
bool flashActive         = false;
unsigned long flashStart = 0;
#define FLASH_DURATION   500

// Connect-Flash
unsigned long connectStart = 0;
#define CONNECT_DURATION 800

// LED-Richtung
bool flipped = false;

// State-Farben (konfigurierbar via STATECOLOR)
//                         OFF  IDLE  KNGHT PROG  DONE  WAIT  ERR   SAVE  CONN  DISC
uint8_t stR[] =          { 0,   0,    255,  0,    0,    255,  255,  255,  0,    255 };
uint8_t stG[] =          { 0,   0,    10,   120,  255,  160,  0,    220,  255,  100 };
uint8_t stB[] =          { 0,   50,   0,    255,  0,    0,    0,    120,  0,    0   };

// Seriell
char cmdBuffer[CMD_BUFFER_SIZE];
uint8_t cmdPos = 0;

void changeState(State newState);

// === Hilfsfunktionen ===

void setTarget(uint16_t i, float r, float g, float b) {
  targR[i] = r; targG[i] = g; targB[i] = b;
}

void setAllTargets(float r, float g, float b) {
  for (uint16_t i = 0; i < NUM_LEDS; i++) {
    targR[i] = r; targG[i] = g; targB[i] = b;
  }
}

float breathe(float speed) {
  return (sin(phase * speed) + 1.0f) * 0.5f;
}

float clampF(float v) {
  if (v < 0.0f) return 0.0f;
  if (v > 255.0f) return 255.0f;
  return v;
}

// === Display Update ===

void updateDisplay() {
  currentBrightness += (targetBrightness - currentBrightness) * 0.05f;
  float brightScale = currentBrightness / 255.0f;

  for (uint16_t i = 0; i < NUM_LEDS; i++) {
    dispR[i] += (targR[i] - dispR[i]) * fadeSpeed;
    dispG[i] += (targG[i] - dispG[i]) * fadeSpeed;
    dispB[i] += (targB[i] - dispB[i]) * fadeSpeed;

    uint8_t r = (uint8_t)clampF(dispR[i] * brightScale);
    uint8_t g = (uint8_t)clampF(dispG[i] * brightScale);
    uint8_t b = (uint8_t)clampF(dispB[i] * brightScale);
    uint16_t idx = flipped ? (NUM_LEDS - 1 - i) : i;
    strip.setPixelColor(idx, strip.Color(r, g, b));
  }
  strip.show();
}

// === Animations-Funktionen ===

void computeBreathingForState(State st, float speed, float minFrac) {
  fadeSpeed = FADE_NORMAL;
  float breath = breathe(speed);
  float scale = minFrac + breath * (1.0f - minFrac);
  setAllTargets(stR[st] * scale, stG[st] * scale, stB[st] * scale);
}

void computeIdleTargets() {
  computeBreathingForState(ST_IDLE, 0.04f, 0.15f);
}

// --- KNIGHT RIDER: Classic KITT Scanner ---
void computeKnightRiderTargets() {
  fadeSpeed = FADE_FAST;

  knightPos += knightSpeed * (float)knightDir;
  if (knightPos >= (float)(NUM_LEDS - 1)) {
    knightPos = (float)(NUM_LEDS - 1);
    knightDir = -1;
  }
  if (knightPos <= 0.0f) {
    knightPos = 0.0f;
    knightDir = 1;
  }

  float cR = (float)stR[ST_KNIGHT_RIDER];
  float cG = (float)stG[ST_KNIGHT_RIDER];
  float cB = (float)stB[ST_KNIGHT_RIDER];

  for (uint16_t i = 0; i < NUM_LEDS; i++) {
    float dist = fabsf((float)i - knightPos);
    float brightness;
    if (dist < 1.0f) {
      brightness = 1.0f - dist * 0.3f;
    } else if (dist < (float)KNIGHT_TAIL_LEN) {
      brightness = powf(1.0f - dist / (float)KNIGHT_TAIL_LEN, 2.5f);
    } else {
      brightness = 0.0f;
    }
    setTarget(i, cR * brightness, cG * brightness, cB * brightness);
  }
}

void computeProgressTargets() {
  fadeSpeed = FADE_NORMAL;
  smoothProgress += (targetProgress - smoothProgress) * 0.12f;
  float ledsToLight = smoothProgress * NUM_LEDS / 100.0f;
  uint16_t fullLeds = (uint16_t)ledsToLight;
  float partial = ledsToLight - (float)fullLeds;
  float shimmer = breathe(0.15f) * 0.12f;
  float cR = stR[ST_PROGRESS], cG = stG[ST_PROGRESS], cB = stB[ST_PROGRESS];

  for (uint16_t i = 0; i < NUM_LEDS; i++) {
    if (i < fullLeds) {
      float posRatio = (float)i / (float)NUM_LEDS;
      float intensity = 0.7f + posRatio * 0.3f + shimmer;
      if (intensity > 1.0f) intensity = 1.0f;
      setTarget(i, cR * intensity, cG * intensity, cB * intensity);
    } else if (i == fullLeds) {
      setTarget(i, cR * partial * 0.7f, cG * partial * 0.7f, cB * partial * 0.7f);
    } else if (i == fullLeds + 1 && partial > 0.3f) {
      setTarget(i, cR * partial * 0.1f, cG * partial * 0.1f, cB * partial * 0.1f);
    } else {
      setTarget(i, cR * 0.01f, cG * 0.01f, cB * 0.01f);
    }
  }
}

void computeDoneTargets() {
  if (flashActive) {
    fadeSpeed = FADE_FAST;
    unsigned long elapsed = millis() - flashStart;
    if (elapsed < FLASH_DURATION) {
      float t = (float)elapsed / (float)FLASH_DURATION;
      float flash = (1.0f - t) * (1.0f - t);
      float w = flash * 50.0f;
      setAllTargets(stR[ST_DONE] * 0.8f + w,
                    stG[ST_DONE] * 0.8f + w,
                    stB[ST_DONE] * 0.8f + w);
      return;
    } else {
      flashActive = false;
    }
  }
  computeBreathingForState(ST_DONE, 0.05f, 0.14f);
}

void computeWaitingTargets() {
  computeBreathingForState(ST_WAITING, 0.04f, 0.10f);
}

void computeErrorTargets() {
  computeBreathingForState(ST_ERROR, 0.07f, 0.05f);
}

void computeDisconnectedTargets() {
  fadeSpeed = FADE_SLOW;
  float breath = breathe(0.03f);
  for (uint16_t i = 0; i < NUM_LEDS; i++) {
    if (i >= 10 && i <= 14) {
      float dist = (float)abs((int)i - 12) / 2.0f;
      float fade = 1.0f - dist * 0.3f;
      float scale = breath * fade;
      setTarget(i, stR[ST_DISCONNECTED] * scale,
                   stG[ST_DISCONNECTED] * scale,
                   stB[ST_DISCONNECTED] * scale);
    } else {
      setTarget(i, 0, 0, 0);
    }
  }
}

void computeSaveTargets() {
  computeBreathingForState(ST_SAVE, 0.05f, 0.35f);
}

void computeConnectTargets() {
  unsigned long elapsed = millis() - connectStart;
  if (elapsed < CONNECT_DURATION) {
    fadeSpeed = FADE_FAST;
    float t = (float)elapsed / (float)CONNECT_DURATION;
    float flash = (1.0f - t) * (1.0f - t);
    float w = flash * 80.0f;
    setAllTargets(stR[ST_CONNECT] * flash + w,
                  stG[ST_CONNECT] * flash + w,
                  stB[ST_CONNECT] * flash + w);
  } else {
    changeState(ST_IDLE);
  }
}

void computeOffTargets() {
  fadeSpeed = FADE_SLOW;
  setAllTargets(0, 0, 0);
}

// === Zustandswechsel ===

void changeState(State newState) {
  if (newState == currentState) return;

  if (newState == ST_DONE) {
    flashActive = true;
    flashStart = millis();
  }
  if (newState == ST_CONNECT) {
    connectStart = millis();
  }
  if (newState == ST_PROGRESS && currentState != ST_PROGRESS) {
    smoothProgress = 0.0f;
    targetProgress = 0.0f;
  }
  if (newState == ST_KNIGHT_RIDER && currentState != ST_KNIGHT_RIDER) {
    knightPos = 0.0f;
    knightDir = 1;
  }
  currentState = newState;
}

// === Befehlsverarbeitung ===

void processCommand(const char* cmd) {
  lastHeartbeat = millis();

  if (currentState == ST_DISCONNECTED) {
    changeState(stateBeforeDisconnect);
  }

  if (strcmp(cmd, "PING") == 0) {
    Serial.println("PONG");
    return;
  }
  if (strcmp(cmd, "INFO") == 0) {
    Serial.print("INFO:");
    Serial.print(FW_NAME);
    Serial.print(",");
    Serial.print(NUM_LEDS);
    Serial.print(",V");
    Serial.println(FW_VERSION);
    return;
  }
  if (strcmp(cmd, "HEARTBEAT") == 0) {
    Serial.println("OK");
    return;
  }
  if (strcmp(cmd, "CLEAR") == 0) {
    changeState(ST_OFF);
    Serial.println("OK");
    return;
  }

  char* colon = strchr(cmd, ':');
  if (!colon) {
    Serial.println("ERR:UNKNOWN_CMD");
    return;
  }

  *colon = '\0';
  const char* command = cmd;
  const char* value   = colon + 1;

  if (strcmp(command, "PROGRESS") == 0) {
    int p = atoi(value);
    targetProgress = (float)constrain(p, 0, 100);
    if (currentState != ST_PROGRESS) changeState(ST_PROGRESS);
    Serial.println("OK");
  }
  else if (strcmp(command, "STATE") == 0) {
    if      (strcmp(value, "IDLE") == 0)          changeState(ST_IDLE);
    else if (strcmp(value, "KNIGHT_RIDER") == 0)  changeState(ST_KNIGHT_RIDER);
    else if (strcmp(value, "DONE") == 0)          changeState(ST_DONE);
    else if (strcmp(value, "WAITING") == 0)       changeState(ST_WAITING);
    else if (strcmp(value, "ERROR") == 0)         changeState(ST_ERROR);
    else if (strcmp(value, "SAVE") == 0)          changeState(ST_SAVE);
    else if (strcmp(value, "CONNECT") == 0)       changeState(ST_CONNECT);
    else if (strcmp(value, "OFF") == 0)           changeState(ST_OFF);
    else {
      Serial.println("ERR:UNKNOWN_STATE");
      return;
    }
    Serial.println("OK");
  }
  else if (strcmp(command, "BRIGHTNESS") == 0) {
    targetBrightness = (float)constrain(atoi(value), 0, 255);
    Serial.println("OK");
  }
  else if (strcmp(command, "SPEED") == 0) {
    int s = constrain(atoi(value), 1, 5);
    knightSpeed = SPEED_TABLE[s - 1];
    Serial.println("OK");
  }
  else if (strcmp(command, "TIMEOUT") == 0) {
    int secs = constrain(atoi(value), 0, 300);
    heartbeatTimeout = (unsigned long)secs * 1000UL;
    lastHeartbeat = millis();
    Serial.println("OK");
  }
  else if (strcmp(command, "FLIP") == 0) {
    flipped = atoi(value) != 0;
    Serial.println("OK");
  }
  else if (strcmp(command, "STATECOLOR") == 0) {
    char buf[32];
    strncpy(buf, value, 31);
    buf[31] = '\0';

    char* tok = strtok(buf, ",");
    if (!tok) { Serial.println("ERR:INVALID_ARGS"); return; }

    int si = -1;
    if      (strcmp(tok, "IDLE") == 0)          si = ST_IDLE;
    else if (strcmp(tok, "KNIGHT_RIDER") == 0)  si = ST_KNIGHT_RIDER;
    else if (strcmp(tok, "PROGRESS") == 0)      si = ST_PROGRESS;
    else if (strcmp(tok, "DONE") == 0)          si = ST_DONE;
    else if (strcmp(tok, "WAITING") == 0)       si = ST_WAITING;
    else if (strcmp(tok, "ERROR") == 0)         si = ST_ERROR;
    else if (strcmp(tok, "SAVE") == 0)          si = ST_SAVE;
    else if (strcmp(tok, "CONNECT") == 0)       si = ST_CONNECT;
    else if (strcmp(tok, "DISCONNECTED") == 0)  si = ST_DISCONNECTED;

    if (si < 0) { Serial.println("ERR:UNKNOWN_STATE"); return; }

    int vals[3];
    for (int j = 0; j < 3; j++) {
      tok = strtok(NULL, ",");
      if (!tok) { Serial.println("ERR:INVALID_COLOR"); return; }
      vals[j] = constrain(atoi(tok), 0, 255);
    }
    stR[si] = vals[0];
    stG[si] = vals[1];
    stB[si] = vals[2];
    Serial.println("OK");
  }
  else {
    Serial.println("ERR:UNKNOWN_CMD");
  }
}

// === Setup ===

void setup() {
  Serial.begin(SERIAL_BAUD);
  strip.begin();
  strip.setBrightness(255);
  strip.clear();
  strip.show();
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  memset(dispR, 0, sizeof(dispR));
  memset(dispG, 0, sizeof(dispG));
  memset(dispB, 0, sizeof(dispB));
  memset(targR, 0, sizeof(targR));
  memset(targG, 0, sizeof(targG));
  memset(targB, 0, sizeof(targB));

  // Startup: Knight Rider Scan (einmal hin und her)
  float pos = 0.0f;
  int dir = 1;
  float startSpeed = 0.18f;
  float startBright = currentBrightness / 255.0f;

  for (int frame = 0; frame < 300; frame++) {
    pos += startSpeed * (float)dir;
    if (pos >= (float)(NUM_LEDS - 1)) { pos = (float)(NUM_LEDS - 1); dir = -1; }
    if (pos <= 0.0f) { pos = 0.0f; if (dir == -1) break; }

    strip.clear();
    for (uint16_t i = 0; i < NUM_LEDS; i++) {
      float dist = fabsf((float)i - pos);
      float b = 0.0f;
      if (dist < 1.0f) b = 1.0f - dist * 0.3f;
      else if (dist < 8.0f) b = powf(1.0f - dist / 8.0f, 2.5f);
      uint8_t r = (uint8_t)(255.0f * b * startBright);
      uint8_t g = (uint8_t)(10.0f * b * startBright);
      strip.setPixelColor(i, strip.Color(r, g, 0));
    }
    strip.show();
    delay(FRAME_MS);
  }

  // Display-Puffer synchronisieren fuer sanften Uebergang zu IDLE
  for (uint16_t i = 0; i < NUM_LEDS; i++) {
    dispR[i] = 0; dispG[i] = 0; dispB[i] = 0;
  }

  currentState = ST_IDLE;
  lastFrame = millis();
  lastHeartbeat = millis();

  Serial.print("READY:");
  Serial.print(FW_NAME);
  Serial.print(",");
  Serial.print(NUM_LEDS);
  Serial.print(",V");
  Serial.println(FW_VERSION);
}

// === Hauptschleife ===

void loop() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (cmdPos > 0) {
        cmdBuffer[cmdPos] = '\0';
        processCommand(cmdBuffer);
        cmdPos = 0;
      }
    } else if (cmdPos < CMD_BUFFER_SIZE - 1) {
      cmdBuffer[cmdPos++] = c;
    }
  }

  bool btnState = digitalRead(BUTTON_PIN) == LOW;
  if (btnState && !buttonPressed && (millis() - buttonDebounce > 200)) {
    buttonPressed = true;
    buttonDebounce = millis();
    if (currentState == ST_DONE || currentState == ST_ERROR ||
        currentState == ST_WAITING) {
      changeState(ST_IDLE);
    }
  }
  if (!btnState) buttonPressed = false;

  if (heartbeatTimeout > 0 &&
      currentState != ST_OFF &&
      currentState != ST_DISCONNECTED) {
    if (millis() - lastHeartbeat > heartbeatTimeout) {
      stateBeforeDisconnect = currentState;
      currentState = ST_DISCONNECTED;
    }
  }

  unsigned long now = millis();
  if (now - lastFrame < FRAME_MS) return;
  lastFrame = now;
  phase++;

  switch (currentState) {
    case ST_IDLE:          computeIdleTargets();          break;
    case ST_KNIGHT_RIDER:  computeKnightRiderTargets();   break;
    case ST_PROGRESS:      computeProgressTargets();      break;
    case ST_DONE:          computeDoneTargets();          break;
    case ST_WAITING:       computeWaitingTargets();       break;
    case ST_ERROR:         computeErrorTargets();         break;
    case ST_SAVE:          computeSaveTargets();          break;
    case ST_CONNECT:       computeConnectTargets();       break;
    case ST_DISCONNECTED:  computeDisconnectedTargets();  break;
    case ST_OFF:           computeOffTargets();           break;
  }

  updateDisplay();
}
```

- [ ] **Step 3: Firmware bauen und pruefen**

```bash
cd "j:/SynologyDrive/Claue Projekte/Status LED Claude Rider/firmware"
pio run
```

Erwartete Ausgabe: `SUCCESS` — keine Errors. Warnings sind OK.
Falls PlatformIO nicht installiert: `pip install platformio`

- [ ] **Step 4: Commit**

```bash
git add firmware/platformio.ini firmware/src/main.cpp firmware/.gitignore
git commit -m "Firmware V3.0: Knight Rider Scanner, Claude Rider Rebranding"
```

---

## Task 3: Python Controller — Config und Serial-Basis

**Files:**
- Create: `python/claude_rider.py`
- Create: `python/test_claude_rider.py`
- Create: `config.json.example`

Dieser Task implementiert: Config-Parsing, Serial-Kommunikation, Protokoll-basierte Port-Erkennung.

- [ ] **Step 1: Tests fuer Config-Parsing schreiben**

Datei `python/test_claude_rider.py`:

```python
"""Tests fuer Claude Rider Controller."""
import json
import os
import socket
import sys
import tempfile
import threading
from unittest.mock import MagicMock
import pytest

sys.path.insert(0, os.path.dirname(__file__))


class TestConfig:
    """Tests fuer Config-Laden und Defaults."""

    def test_default_config(self):
        from claude_rider import load_config
        config = load_config(None)
        assert config["serial"]["port"] == "auto"
        assert config["serial"]["baud"] == 115200
        assert config["brightness"] == 80
        assert config["flip"] is False
        assert config["knight_rider_speed"] == 3
        assert "thinking" in config["events"]
        assert "KNIGHT_RIDER" in config["colors"]

    def test_load_config_from_file(self):
        from claude_rider import load_config
        custom = {
            "brightness": 120,
            "knight_rider_speed": 5,
            "colors": {"IDLE": [10, 20, 30]}
        }
        with tempfile.NamedTemporaryFile(mode='w', suffix='.json',
                                          delete=False) as f:
            json.dump(custom, f)
            f.flush()
            config = load_config(f.name)
        os.unlink(f.name)
        assert config["brightness"] == 120
        assert config["knight_rider_speed"] == 5
        assert config["colors"]["IDLE"] == [10, 20, 30]
        # Defaults bleiben erhalten fuer nicht-gesetzte Felder
        assert config["serial"]["port"] == "auto"
        assert config["colors"]["KNIGHT_RIDER"] == [255, 10, 0]

    def test_event_mapping(self):
        from claude_rider import load_config, map_event
        config = load_config(None)
        assert map_event(config, "thinking") == "STATE:KNIGHT_RIDER"
        assert map_event(config, "error") == "STATE:ERROR"
        assert map_event(config, "idle") == "STATE:IDLE"
        assert map_event(config, "task_done") == "STATE:DONE"

    def test_event_mapping_with_value(self):
        from claude_rider import load_config, map_event
        config = load_config(None)
        assert map_event(config, "progress", value=75) == "PROGRESS:75"

    def test_unknown_event_returns_none(self):
        from claude_rider import load_config, map_event
        config = load_config(None)
        assert map_event(config, "nonexistent_event") is None
```

- [ ] **Step 2: Tests ausfuehren — muessen fehlschlagen**

```bash
cd "j:/SynologyDrive/Claue Projekte/Status LED Claude Rider"
python -m pytest python/test_claude_rider.py -v
```

Erwartet: FAIL — `claude_rider` Module existiert noch nicht.

- [ ] **Step 3: Config und Event-Mapping implementieren**

Beginn von `python/claude_rider.py` — nur Config und Mapping (noch kein Serial/Daemon):

```python
"""
Claude Rider — LED Status Controller fuer Claude Code
=====================================================
Steuert eine WS2812 LED-Leiste (ESP32-C3) als physische Statusanzeige
fuer Claude Code. Knight Rider Scanner beim Denken, Farb-Animationen
fuer verschiedene Zustaende.

Verwendung:
    claude_rider.py --daemon              Daemon starten (haelt Serial offen)
    claude_rider.py --event <name>        Event an Daemon senden
    claude_rider.py --event progress --value 75
    claude_rider.py --stop                Daemon herunterfahren
    claude_rider.py --status              Status anzeigen
"""

import argparse
import json
import os
import socket
import sys
import threading
import time
from typing import Optional

# Daemon-Kommunikation
DAEMON_HOST = "127.0.0.1"
DAEMON_PORT = 17177
LOCK_FILE = os.path.join(os.path.expanduser("~"), ".claude_rider.lock")

# === Default-Konfiguration ===

DEFAULT_CONFIG = {
    "serial": {
        "port": "auto",
        "baud": 115200
    },
    "brightness": 80,
    "flip": False,
    "knight_rider_speed": 3,
    "events": {
        "session_start":  {"action": "STATE:CONNECT"},
        "thinking":       {"action": "STATE:KNIGHT_RIDER"},
        "tool_pending":   {"action": "STATE:WAITING"},
        "tool_running":   {"action": "STATE:KNIGHT_RIDER"},
        "error":          {"action": "STATE:ERROR"},
        "task_done":      {"action": "STATE:DONE"},
        "idle":           {"action": "STATE:IDLE"},
        "waiting":        {"action": "STATE:WAITING"},
        "progress":       {"action": "PROGRESS:{value}"},
    },
    "colors": {
        "KNIGHT_RIDER": [255, 10, 0],
        "IDLE":         [0, 0, 50],
        "WAITING":      [255, 160, 0],
        "ERROR":        [255, 0, 0],
        "DONE":         [0, 255, 0],
        "PROGRESS":     [0, 120, 255],
    }
}


def _deep_merge(base: dict, override: dict) -> dict:
    """Merge override in base (rekursiv). Gibt neues dict zurueck."""
    result = base.copy()
    for key, val in override.items():
        if key in result and isinstance(result[key], dict) and isinstance(val, dict):
            result[key] = _deep_merge(result[key], val)
        else:
            result[key] = val
    return result


def load_config(path: Optional[str]) -> dict:
    """Config laden. Fehlende Felder werden mit Defaults aufgefuellt."""
    config = DEFAULT_CONFIG.copy()
    config = _deep_merge(DEFAULT_CONFIG, {})

    if path and os.path.exists(path):
        with open(path, 'r') as f:
            user_config = json.load(f)
        config = _deep_merge(config, user_config)

    return config


def map_event(config: dict, event: str, value: Optional[int] = None) -> Optional[str]:
    """Event-Name auf Firmware-Befehl mappen."""
    events = config.get("events", {})
    entry = events.get(event)
    if entry is None:
        return None
    action = entry.get("action", "")
    if value is not None:
        action = action.replace("{value}", str(value))
    return action
```

- [ ] **Step 4: Tests ausfuehren — muessen jetzt passen**

```bash
cd "j:/SynologyDrive/Claue Projekte/Status LED Claude Rider"
python -m pytest python/test_claude_rider.py -v
```

Erwartet: 5 PASSED.

- [ ] **Step 5: config.json.example erstellen**

Datei `config.json.example`:

```json
{
  "serial": {
    "port": "auto",
    "baud": 115200
  },
  "brightness": 80,
  "flip": false,
  "knight_rider_speed": 3,

  "events": {
    "session_start":  { "action": "STATE:CONNECT" },
    "thinking":       { "action": "STATE:KNIGHT_RIDER" },
    "tool_pending":   { "action": "STATE:WAITING" },
    "tool_running":   { "action": "STATE:KNIGHT_RIDER" },
    "error":          { "action": "STATE:ERROR" },
    "task_done":      { "action": "STATE:DONE" },
    "idle":           { "action": "STATE:IDLE" },
    "waiting":        { "action": "STATE:WAITING" },
    "progress":       { "action": "PROGRESS:{value}" }
  },

  "colors": {
    "KNIGHT_RIDER": [255, 10, 0],
    "IDLE":         [0, 0, 50],
    "WAITING":      [255, 160, 0],
    "ERROR":        [255, 0, 0],
    "DONE":         [0, 255, 0],
    "PROGRESS":     [0, 120, 255]
  }
}
```

- [ ] **Step 6: Commit**

```bash
git add python/claude_rider.py python/test_claude_rider.py config.json.example
git commit -m "Python Controller: Config-System mit Defaults und Event-Mapping"
```

---

## Task 4: Python Controller — Serial-Kommunikation und Port-Erkennung

**Files:**
- Modify: `python/claude_rider.py` — Serial-Klasse und Port-Scanning
- Modify: `python/test_claude_rider.py` — Tests fuer Port-Erkennung (mit Mock)

- [ ] **Step 1: Tests fuer Serial-Klasse schreiben**

Ans Ende von `python/test_claude_rider.py` anfuegen:

```python
class TestSerialConnection:
    """Tests fuer Serial-Kommunikation."""

    def test_send_command_formats_correctly(self):
        from claude_rider import ClaudeRiderSerial
        crs = ClaudeRiderSerial.__new__(ClaudeRiderSerial)
        crs._serial = MagicMock()
        crs._serial.is_open = True
        crs._serial.readline.return_value = b"OK\n"
        crs._lock = threading.Lock()

        result = crs.send("STATE:KNIGHT_RIDER")
        crs._serial.write.assert_called_once_with(b"STATE:KNIGHT_RIDER\n")
        assert result == "OK"

    def test_ping_returns_true_on_pong(self):
        from claude_rider import ClaudeRiderSerial
        crs = ClaudeRiderSerial.__new__(ClaudeRiderSerial)
        crs._serial = MagicMock()
        crs._serial.is_open = True
        crs._serial.readline.return_value = b"PONG\n"
        crs._lock = threading.Lock()

        assert crs.ping() is True

    def test_ping_returns_false_on_wrong_response(self):
        from claude_rider import ClaudeRiderSerial
        crs = ClaudeRiderSerial.__new__(ClaudeRiderSerial)
        crs._serial = MagicMock()
        crs._serial.is_open = True
        crs._serial.readline.return_value = b"NOPE\n"
        crs._lock = threading.Lock()

        assert crs.ping() is False
```

- [ ] **Step 2: Tests ausfuehren — muessen fehlschlagen**

```bash
python -m pytest python/test_claude_rider.py::TestSerialConnection -v
```

Erwartet: FAIL — `ClaudeRiderSerial` existiert noch nicht.

- [ ] **Step 3: Serial-Klasse und Port-Erkennung implementieren**

Folgenden Code ans Ende von `python/claude_rider.py` anfuegen (vor einem eventuellen `if __name__`-Block):

```python
try:
    import serial
    import serial.tools.list_ports
    HAS_SERIAL = True
except ImportError:
    HAS_SERIAL = False


class ClaudeRiderSerial:
    """Serial-Verbindung zur Claude Rider LED-Leiste."""

    def __init__(self, port: str = "auto", baud: int = 115200):
        if not HAS_SERIAL:
            raise RuntimeError("pyserial nicht installiert: pip install pyserial")
        self._serial = None
        self._lock = threading.Lock()
        self._port = port
        self._baud = baud

    def connect(self) -> bool:
        """Verbindung herstellen. Bei port='auto' wird gescannt."""
        port = self._port
        if port == "auto":
            port = self._find_device()
        if port is None:
            print("[ClaudeRider] Kein Geraet gefunden.")
            return False

        try:
            self._serial = serial.Serial(
                port=port, baudrate=self._baud,
                timeout=1.0, write_timeout=1.0
            )
            self._port = port
            time.sleep(0.5)  # ESP32 Reset abwarten
            self._serial.reset_input_buffer()

            if not self.ping():
                print(f"[ClaudeRider] Geraet auf {port} antwortet nicht.")
                self._serial.close()
                self._serial = None
                return False

            info = self.send("INFO")
            if not info.startswith("INFO:CLAUDE_RIDER"):
                print(f"[ClaudeRider] Unbekanntes Geraet: {info}")
                self._serial.close()
                self._serial = None
                return False

            print(f"[ClaudeRider] Verbunden: {port} ({info})")
            return True

        except serial.SerialException as e:
            print(f"[ClaudeRider] Verbindungsfehler: {e}")
            return False

    def _find_device(self) -> Optional[str]:
        """Alle COM-Ports scannen, PING/PONG Protokoll-Erkennung."""
        for port_info in serial.tools.list_ports.comports():
            port = port_info.device
            try:
                s = serial.Serial(port=port, baudrate=self._baud, timeout=0.5)
                time.sleep(0.3)
                s.reset_input_buffer()
                s.write(b"PING\n")
                s.flush()
                response = s.readline().decode('ascii', errors='ignore').strip()
                s.close()
                if response == "PONG":
                    print(f"[ClaudeRider] Geraet gefunden: {port}")
                    return port
            except (serial.SerialException, OSError):
                continue
        return None

    def send(self, command: str) -> str:
        """Befehl senden und Antwort lesen (thread-safe)."""
        if not self._serial or not self._serial.is_open:
            return "ERR:NOT_CONNECTED"
        with self._lock:
            try:
                self._serial.reset_input_buffer()
                self._serial.write(f"{command}\n".encode('ascii'))
                self._serial.flush()
                return self._serial.readline().decode('ascii', errors='ignore').strip()
            except (serial.SerialException, UnicodeDecodeError) as e:
                return f"ERR:{e}"

    def ping(self) -> bool:
        """PING/PONG Verbindungstest."""
        return self.send("PING") == "PONG"

    @property
    def connected(self) -> bool:
        return self._serial is not None and self._serial.is_open

    @property
    def port(self) -> str:
        return self._port

    def close(self):
        """Verbindung trennen."""
        if self._serial and self._serial.is_open:
            try:
                self.send("STATE:OFF")
                time.sleep(0.2)
                self._serial.close()
            except serial.SerialException:
                pass
        self._serial = None
```

- [ ] **Step 4: Tests ausfuehren — muessen passen**

```bash
python -m pytest python/test_claude_rider.py -v
```

Erwartet: 8 PASSED (5 Config + 3 Serial).

- [ ] **Step 5: Commit**

```bash
git add python/claude_rider.py python/test_claude_rider.py
git commit -m "Serial-Kommunikation und Protokoll-basierte Port-Erkennung"
```

---

## Task 5: Python Controller — Daemon und CLI

**Files:**
- Modify: `python/claude_rider.py` — Daemon-Klasse und CLI main()
- Modify: `python/test_claude_rider.py` — Tests fuer Daemon-Kommunikation

- [ ] **Step 1: Tests fuer Daemon-Kommunikation schreiben**

Ans Ende von `python/test_claude_rider.py` anfuegen:

```python
class TestDaemonProtocol:
    """Tests fuer Daemon Socket-Kommunikation."""

    def test_daemon_message_format(self):
        """Daemon erwartet JSON-Messages ueber Socket."""
        from claude_rider import build_daemon_message
        msg = build_daemon_message("thinking")
        parsed = json.loads(msg)
        assert parsed["event"] == "thinking"
        assert parsed.get("value") is None

    def test_daemon_message_with_value(self):
        from claude_rider import build_daemon_message
        msg = build_daemon_message("progress", value=42)
        parsed = json.loads(msg)
        assert parsed["event"] == "progress"
        assert parsed["value"] == 42

    def test_send_to_daemon_and_receive(self):
        """Socket-basierte Kommunikation testen."""
        from claude_rider import build_daemon_message

        received = []

        # Mini-Server simuliert Daemon
        server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        server.bind(("127.0.0.1", 0))
        port = server.getsockname()[1]
        server.listen(1)

        def accept():
            conn, _ = server.accept()
            data = conn.recv(1024).decode('utf-8')
            received.append(data)
            conn.send(b"OK\n")
            conn.close()

        t = threading.Thread(target=accept)
        t.start()

        # Client sendet Message
        client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        client.connect(("127.0.0.1", port))
        msg = build_daemon_message("error")
        client.send(msg.encode('utf-8'))
        response = client.recv(1024).decode('utf-8')
        client.close()

        t.join(timeout=2)
        server.close()

        assert response.strip() == "OK"
        assert len(received) == 1
        parsed = json.loads(received[0])
        assert parsed["event"] == "error"
```

- [ ] **Step 2: Tests ausfuehren — muessen fehlschlagen**

```bash
python -m pytest python/test_claude_rider.py::TestDaemonProtocol -v
```

Erwartet: FAIL — `build_daemon_message` existiert noch nicht.

- [ ] **Step 3: Daemon und CLI implementieren**

Folgenden Code ans Ende von `python/claude_rider.py` anfuegen:

```python
def build_daemon_message(event: str, value: Optional[int] = None) -> str:
    """JSON-Message fuer Daemon-Kommunikation bauen."""
    msg = {"event": event}
    if value is not None:
        msg["value"] = value
    return json.dumps(msg)


def send_to_daemon(event: str, value: Optional[int] = None,
                   host: str = DAEMON_HOST, port: int = DAEMON_PORT) -> bool:
    """Event an laufenden Daemon senden."""
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(2.0)
        sock.connect((host, port))
        msg = build_daemon_message(event, value)
        sock.send(msg.encode('utf-8'))
        response = sock.recv(1024).decode('utf-8').strip()
        sock.close()
        return response == "OK"
    except (ConnectionRefusedError, socket.timeout, OSError):
        return False


def is_daemon_running(host: str = DAEMON_HOST, port: int = DAEMON_PORT) -> bool:
    """Pruefen ob Daemon laeuft."""
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(1.0)
        sock.connect((host, port))
        sock.send(build_daemon_message("ping").encode('utf-8'))
        response = sock.recv(1024).decode('utf-8').strip()
        sock.close()
        return response == "OK"
    except (ConnectionRefusedError, socket.timeout, OSError):
        return False


class ClaudeRiderDaemon:
    """Daemon: haelt Serial offen, empfaengt Events via Socket."""

    def __init__(self, config: dict):
        self._config = config
        self._serial = ClaudeRiderSerial(
            port=config["serial"]["port"],
            baud=config["serial"]["baud"]
        )
        self._server = None
        self._running = False
        self._heartbeat_thread = None

    def start(self):
        """Daemon starten: Serial verbinden, Socket oeffnen, lauschen."""
        if not self._serial.connect():
            print("[ClaudeRider] Daemon konnte nicht starten: kein Geraet.")
            sys.exit(1)

        # Initiale Konfiguration senden
        self._serial.send(f"BRIGHTNESS:{self._config['brightness']}")
        if self._config.get("flip"):
            self._serial.send("FLIP:1")
        speed = self._config.get("knight_rider_speed", 3)
        self._serial.send(f"SPEED:{speed}")
        for state, rgb in self._config.get("colors", {}).items():
            r, g, b = rgb
            self._serial.send(f"STATECOLOR:{state},{r},{g},{b}")
        self._serial.send("STATE:IDLE")

        # Socket-Server starten
        self._server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self._server.bind((DAEMON_HOST, DAEMON_PORT))
        self._server.listen(5)
        self._server.settimeout(1.0)
        self._running = True

        port_name = self._serial.port
        print(f"[ClaudeRider] Daemon gestartet auf {port_name}")
        print(f"[ClaudeRider] Socket: {DAEMON_HOST}:{DAEMON_PORT}")

        # Heartbeat-Thread
        self._heartbeat_thread = threading.Thread(
            target=self._heartbeat_loop, daemon=True
        )
        self._heartbeat_thread.start()

        # Hauptschleife
        try:
            while self._running:
                try:
                    conn, addr = self._server.accept()
                    threading.Thread(
                        target=self._handle_client,
                        args=(conn,), daemon=True
                    ).start()
                except socket.timeout:
                    continue
        except KeyboardInterrupt:
            pass
        finally:
            self.stop()

    def _handle_client(self, conn: socket.socket):
        """Eingehende Event-Message verarbeiten."""
        try:
            data = conn.recv(1024).decode('utf-8').strip()
            if not data:
                conn.close()
                return

            msg = json.loads(data)
            event = msg.get("event", "")
            value = msg.get("value")

            if event == "stop":
                conn.send(b"OK\n")
                conn.close()
                self._running = False
                return

            if event == "ping":
                conn.send(b"OK\n")
                conn.close()
                return

            if event == "status":
                info = {
                    "running": True,
                    "port": self._serial.port,
                    "connected": self._serial.connected
                }
                conn.send((json.dumps(info) + "\n").encode('utf-8'))
                conn.close()
                return

            # Event auf Firmware-Befehl mappen
            command = map_event(self._config, event, value)
            if command:
                result = self._serial.send(command)
                conn.send(f"{result}\n".encode('utf-8'))
            else:
                conn.send(b"ERR:UNKNOWN_EVENT\n")

            conn.close()
        except (json.JSONDecodeError, ConnectionResetError, OSError):
            try:
                conn.close()
            except OSError:
                pass

    def _heartbeat_loop(self):
        """Regelmaessig Heartbeat senden."""
        while self._running:
            time.sleep(4)
            if self._running and self._serial.connected:
                self._serial.send("HEARTBEAT")

    def stop(self):
        """Daemon sauber herunterfahren."""
        self._running = False
        if self._serial.connected:
            self._serial.send("STATE:OFF")
            time.sleep(0.3)
            self._serial.close()
        if self._server:
            self._server.close()
        print("[ClaudeRider] Daemon gestoppt.")


# === CLI ===

def find_config_path() -> Optional[str]:
    """config.json im aktuellen oder Script-Verzeichnis suchen."""
    candidates = [
        os.path.join(os.getcwd(), "config.json"),
        os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "config.json"),
        os.path.join(os.path.expanduser("~"), ".claude_rider.json"),
    ]
    for path in candidates:
        if os.path.exists(path):
            return path
    return None


def main():
    parser = argparse.ArgumentParser(
        description="Claude Rider — LED Status Controller fuer Claude Code"
    )
    parser.add_argument("--daemon", action="store_true",
                        help="Daemon starten (haelt Serial-Verbindung offen)")
    parser.add_argument("--event", type=str,
                        help="Event an Daemon senden")
    parser.add_argument("--value", type=int, default=None,
                        help="Optionaler Wert (z.B. Progress-Prozent)")
    parser.add_argument("--stop", action="store_true",
                        help="Daemon herunterfahren")
    parser.add_argument("--status", action="store_true",
                        help="Daemon-Status anzeigen")
    parser.add_argument("--config", type=str, default=None,
                        help="Pfad zur config.json")

    args = parser.parse_args()

    if args.daemon:
        config_path = args.config or find_config_path()
        config = load_config(config_path)
        daemon = ClaudeRiderDaemon(config)
        daemon.start()

    elif args.event:
        if not send_to_daemon(args.event, args.value):
            # Daemon laeuft nicht — Fallback: direkt senden
            print("[ClaudeRider] Daemon nicht erreichbar. Direkt-Modus...")
            config_path = args.config or find_config_path()
            config = load_config(config_path)
            command = map_event(config, args.event, args.value)
            if command:
                ser = ClaudeRiderSerial(
                    port=config["serial"]["port"],
                    baud=config["serial"]["baud"]
                )
                if ser.connect():
                    ser.send(command)
                    ser.close()
            sys.exit(0)
        sys.exit(0)

    elif args.stop:
        if send_to_daemon("stop"):
            print("[ClaudeRider] Daemon wird heruntergefahren...")
        else:
            print("[ClaudeRider] Daemon laeuft nicht.")

    elif args.status:
        if is_daemon_running():
            # Status vom Daemon abfragen
            try:
                sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                sock.settimeout(2.0)
                sock.connect((DAEMON_HOST, DAEMON_PORT))
                sock.send(build_daemon_message("status").encode('utf-8'))
                response = sock.recv(1024).decode('utf-8').strip()
                sock.close()
                info = json.loads(response)
                print(f"[ClaudeRider] Daemon laeuft")
                print(f"  Port: {info.get('port', '?')}")
                print(f"  Verbunden: {info.get('connected', False)}")
            except (ConnectionRefusedError, socket.timeout, OSError):
                print("[ClaudeRider] Daemon nicht erreichbar.")
        else:
            print("[ClaudeRider] Daemon laeuft nicht.")

    else:
        parser.print_help()


if __name__ == "__main__":
    main()
```

- [ ] **Step 4: Alle Tests ausfuehren**

```bash
python -m pytest python/test_claude_rider.py -v
```

Erwartet: 11 PASSED (5 Config + 3 Serial + 3 Daemon).

- [ ] **Step 5: Commit**

```bash
git add python/claude_rider.py python/test_claude_rider.py
git commit -m "Daemon mit Socket-Kommunikation und CLI-Interface"
```

---

## Task 6: Batch-Scripts und Hook-Registration

**Files:**
- Create: `start_claude_rider.bat`
- Create: `stop_claude_rider.bat`

- [ ] **Step 1: start_claude_rider.bat erstellen**

```batch
@echo off
chcp 65001 >nul 2>&1
title Claude Rider Setup

echo.
echo  ╔══════════════════════════════════════╗
echo  ║         CLAUDE RIDER v3.0            ║
echo  ║   LED Status fuer Claude Code        ║
echo  ╚══════════════════════════════════════╝
echo.

:: Python pruefen
python --version >nul 2>&1
if errorlevel 1 (
    echo [FEHLER] Python nicht gefunden. Bitte installieren: https://python.org
    pause
    exit /b 1
)

:: pyserial installieren falls noetig
python -c "import serial" >nul 2>&1
if errorlevel 1 (
    echo [INFO] Installiere pyserial...
    pip install pyserial
    if errorlevel 1 (
        echo [FEHLER] pyserial konnte nicht installiert werden.
        pause
        exit /b 1
    )
    echo [OK] pyserial installiert.
)

:: Pruefen ob Daemon schon laeuft
python "%~dp0python\claude_rider.py" --status >nul 2>&1
if not errorlevel 1 (
    echo [INFO] Claude Rider Daemon laeuft bereits.
    pause
    exit /b 0
)

:: Daemon starten
echo [INFO] Starte Claude Rider Daemon...
start /b "" python "%~dp0python\claude_rider.py" --daemon
timeout /t 3 /nobreak >nul

:: Pruefen ob Daemon gestartet ist
python "%~dp0python\claude_rider.py" --status
if errorlevel 1 (
    echo [FEHLER] Daemon konnte nicht gestartet werden.
    echo          Pruefen: Ist die LED-Leiste per USB angeschlossen?
    pause
    exit /b 1
)

echo.
echo [OK] Claude Rider laeuft!
echo.
echo Tipp: Hooks in Claude Code registrieren:
echo   claude_rider.py liegt in: %~dp0python\claude_rider.py
echo.
pause
```

- [ ] **Step 2: stop_claude_rider.bat erstellen**

```batch
@echo off
chcp 65001 >nul 2>&1
title Claude Rider Stop

echo.
echo [INFO] Fahre Claude Rider herunter...

python "%~dp0python\claude_rider.py" --stop

echo [OK] Claude Rider gestoppt.
echo.
pause
```

- [ ] **Step 3: Commit**

```bash
git add start_claude_rider.bat stop_claude_rider.bat
git commit -m "Batch-Scripts: One-Click Start und Stop fuer Windows"
```

---

## Task 7: CLAUDE.md und README.md

**Files:**
- Rewrite: `CLAUDE.md`
- Create: `README.md`

- [ ] **Step 1: CLAUDE.md neu schreiben**

```markdown
# Claude Rider

## Projekt
Open Source LED-Statusanzeige fuer Claude Code.
BIGTREETECH Panda Status LED-Leiste (ESP32-C3, 25x WS2812) zeigt
den Zustand von Claude Code durch Animationen — Knight Rider Scanner
beim Denken, Farb-Animationen fuer verschiedene Zustaende.

## Hardware

| Parameter       | Wert                           |
|-----------------|--------------------------------|
| MCU             | ESP32-C3-MINI (RISC-V)        |
| LED Data Pin    | GPIO 5 (ueber RMT)            |
| LED Typ         | WS2812 (GRB, 800kHz)          |
| Anzahl LEDs     | 25                             |
| Button          | GPIO 9 (aktiv LOW, Pullup)     |
| USB-Serial      | CH340K                         |
| Stromversorgung | USB-C, 5V 3A                   |

## Projektstruktur

```
claude-rider/
├── README.md                    # Setup-Anleitung
├── CLAUDE.md                    # Diese Datei
├── LICENSE                      # MIT
├── config.json.example          # Konfigurationsvorlage
├── firmware/
│   ├── platformio.ini           # Build-Konfiguration
│   └── src/
│       └── main.cpp             # Firmware V3.0
├── python/
│   ├── claude_rider.py          # Controller + Daemon + CLI
│   └── test_claude_rider.py     # Tests
├── start_claude_rider.bat       # Windows: Start
└── stop_claude_rider.bat        # Windows: Stop
```

## Serielles Protokoll (115200 Baud)

```
PING                           -> PONG
INFO                           -> INFO:CLAUDE_RIDER,25,V3.0
STATE:<zustand>                -> OK    IDLE|KNIGHT_RIDER|DONE|WAITING|ERROR|SAVE|CONNECT|OFF
PROGRESS:<0-100>               -> OK
BRIGHTNESS:<0-255>             -> OK
SPEED:<1-5>                    -> OK    Knight Rider Scan-Geschwindigkeit
STATECOLOR:<st>,<r>,<g>,<b>   -> OK
FLIP:<0|1>                     -> OK
TIMEOUT:<sekunden>             -> OK
HEARTBEAT                      -> OK
CLEAR                          -> OK
```

## LED-Zustaende

| Zustand      | Farbe         | Animation                  | Bedeutung                  |
|--------------|---------------|----------------------------|----------------------------|
| KNIGHT_RIDER | Rot           | KITT Scanner (aggressiv)   | Claude denkt / arbeitet    |
| IDLE         | Blau          | Sanftes Atmen              | System bereit              |
| WAITING      | Amber/Gelb    | Pulsieren                  | Input noetig               |
| ERROR        | Rot           | Schnelles Atmen            | Fehler aufgetreten         |
| DONE         | Gruen         | Flash + Atmen              | Task abgeschlossen         |
| PROGRESS     | Cyan          | Fortschrittsbalken         | Langer Task                |

## Konventionen
- Sprache: Deutsch (Code-Kommentare duerfen Englisch sein)
- Framework: Arduino + Adafruit NeoPixel
- Python: pyserial, keine weiteren Abhaengigkeiten
- Tests: pytest
```

- [ ] **Step 2: README.md erstellen**

```markdown
# Claude Rider

> Knight Rider meets Claude Code — eine physische LED-Statusanzeige
> fuer deinen AI Coding Assistant.

Claude Rider verwandelt eine guenstige LED-Leiste in eine Knight-Rider-inspirierte
Statusanzeige, die dir auf einen Blick zeigt was Claude Code gerade macht:

- **Denkt:** Roter KITT-Scanner rast hin und her
- **Wartet auf Input:** Amber Pulsieren
- **Fehler:** Rotes Atmen
- **Fertig:** Gruener Flash
- **Idle:** Sanftes blaues Atmen

## Hardware

Du brauchst eine **BIGTREETECH Panda Status LED-Leiste** (~15 EUR):
- ESP32-C3-MINI mit 25 WS2812 LEDs
- USB-C Anschluss
- Einfach am Monitor befestigen

## Quick Start

### 1. Firmware flashen (einmalig)

```bash
cd firmware
pip install platformio
pio run -t upload
```

> Tipp: Falls der Upload nicht klappt, Button an der Leiste halten
> waehrend du USB einsteckst (Bootloader-Modus).

### 2. Starten

Doppelklick auf `start_claude_rider.bat` — fertig!

Das Script:
- Installiert pyserial falls noetig
- Findet die Leiste automatisch (PING/PONG)
- Startet den Daemon im Hintergrund

### 3. Claude Code Hooks einrichten

Fuege folgendes in deine `~/.claude/settings.json` ein:

```json
{
  "hooks": {
    "UserPromptSubmit": [
      {
        "hooks": [{ "type": "command", "command": "python PFAD/python/claude_rider.py --event thinking", "timeout": 3 }]
      }
    ],
    "Stop": [
      {
        "hooks": [{ "type": "command", "command": "python PFAD/python/claude_rider.py --event task_done", "timeout": 3 }]
      }
    ],
    "PreToolUse": [
      {
        "hooks": [{ "type": "command", "command": "python PFAD/python/claude_rider.py --event tool_pending", "timeout": 3 }]
      }
    ],
    "PostToolUse": [
      {
        "hooks": [{ "type": "command", "command": "python PFAD/python/claude_rider.py --event tool_running", "timeout": 3 }]
      }
    ],
    "PostToolUseFailure": [
      {
        "hooks": [{ "type": "command", "command": "python PFAD/python/claude_rider.py --event error", "timeout": 3 }]
      }
    ],
    "Notification": [
      {
        "matcher": "idle_prompt",
        "hooks": [{ "type": "command", "command": "python PFAD/python/claude_rider.py --event idle", "timeout": 3 }]
      },
      {
        "matcher": "permission_prompt",
        "hooks": [{ "type": "command", "command": "python PFAD/python/claude_rider.py --event waiting", "timeout": 3 }]
      }
    ],
    "SessionStart": [
      {
        "hooks": [{ "type": "command", "command": "python PFAD/python/claude_rider.py --event session_start", "timeout": 3 }]
      }
    ]
  }
}
```

Ersetze `PFAD` mit dem tatsaechlichen Pfad zu deinem Claude Rider Ordner.

## Customization

Kopiere `config.json.example` nach `config.json` und passe an:

```json
{
  "brightness": 80,
  "knight_rider_speed": 3,
  "colors": {
    "KNIGHT_RIDER": [255, 10, 0],
    "IDLE": [0, 0, 50]
  }
}
```

**Farben:** RGB-Werte (0-255) pro Zustand
**Speed:** 1 (langsam) bis 5 (Turbo)
**Brightness:** 0 (aus) bis 255 (volle Power)

## CLI

```bash
python claude_rider.py --daemon          # Daemon starten
python claude_rider.py --event thinking  # Event senden
python claude_rider.py --status          # Status pruefen
python claude_rider.py --stop            # Daemon stoppen
```

## Stoppen

Doppelklick auf `stop_claude_rider.bat` oder:

```bash
python claude_rider.py --stop
```

## Lizenz

MIT — siehe LICENSE
```

- [ ] **Step 3: Commit**

```bash
git add CLAUDE.md README.md
git commit -m "README und CLAUDE.md fuer Open Source Release"
```

---

## Task 8: Finale Bereinigung und .gitignore

**Files:**
- Create: `.gitignore` (Projekt-Root)
- Delete: verbleibende alte Dateien pruefen

- [ ] **Step 1: Root .gitignore erstellen**

```
# Python
__pycache__/
*.pyc
.pytest_cache/

# PlatformIO
firmware/.pio/
firmware/.vscode/

# Config (User-spezifisch)
config.json

# OS
.DS_Store
Thumbs.db

# Claude Rider Daemon
.claude_rider.lock

# Superpowers (Brainstorming-Artefakte)
.superpowers/
```

- [ ] **Step 2: Pruefen ob alte Dateien noch existieren**

```bash
ls -la "j:/SynologyDrive/Claue Projekte/Status LED Claude Rider/"
```

Falls noch alte Dateien da sind (panda_status_v1.0.2.bin, ANLEITUNG.md, etc.) — loeschen.

- [ ] **Step 3: Alten Python-Controller entfernen**

```bash
rm -f "j:/SynologyDrive/Claue Projekte/Status LED Claude Rider/python/panda_led_controller.py"
```

- [ ] **Step 4: Commit**

```bash
git add .gitignore
git add -A
git commit -m "Finale Bereinigung: .gitignore, alte Dateien entfernt"
```

---

## Zusammenfassung der Tasks

| Task | Beschreibung | Dateien |
|------|-------------|---------|
| 1 | Projekt bereinigen, Lizenz | Alte Dateien loeschen, LICENSE |
| 2 | Firmware V3.0 mit Knight Rider | firmware/src/main.cpp, platformio.ini |
| 3 | Python Config + Event-Mapping | claude_rider.py (Basis), Tests, config.json.example |
| 4 | Serial-Kommunikation + Port-Scan | claude_rider.py (Serial), Tests |
| 5 | Daemon + CLI | claude_rider.py (Daemon/CLI), Tests |
| 6 | Batch-Scripts | start/stop_claude_rider.bat |
| 7 | Dokumentation | CLAUDE.md, README.md |
| 8 | Finale Bereinigung | .gitignore, alte Dateien weg |
