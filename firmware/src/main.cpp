/**
 * Panda Status Custom Firmware V2.0
 * =================================
 * ESP32-C3-MINI | 25x WS2812 an GPIO5 | CH340K USB-Serial
 *
 * Fernstatus-Anzeige fuer Kalibrier-Operator.
 * Alle Animationen mit sanften Fades - kein hartes Blinken.
 *
 * Kerntechnik: Doppelpuffer mit Interpolation.
 * Jeder Frame berechnet Ziel-Farben, das Fade-System interpoliert
 * sanft dorthin. Dadurch sind ALLE Zustandswechsel automatisch
 * weiche Uebergaenge.
 *
 * Protokoll (115200 Baud, Newline-terminiert):
 *   PING                -> PONG
 *   INFO                -> INFO:PANDA_CUSTOM,25,V2.1
 *   PROGRESS:<0-100>    -> OK     Gruener Fortschrittsbalken
 *   STATE:<zustand>     -> OK     IDLE|DONE|WAITING|ERROR|SAVE|CALIBRATED|CONNECT|OFF
 *   BRIGHTNESS:<0-255>  -> OK     Helligkeit (sanfter Uebergang)
 *   STATECOLOR:<st>,<r>,<g>,<b> -> OK  Farbe pro Zustand setzen
 *   FLIP:<0|1>          -> OK     LED-Richtung spiegeln
 *   TIMEOUT:<sekunden>  -> OK     Heartbeat-Timeout (0=aus)
 *   HEARTBEAT           -> OK     Watchdog zuruecksetzen
 *   CLEAR               -> OK     Alles aus
 */

#include <Adafruit_NeoPixel.h>

// === Hardware ===
#define LED_PIN         5
#define NUM_LEDS        25
#define BUTTON_PIN      9
#define SERIAL_BAUD     115200
#define CMD_BUFFER_SIZE 64

// === Firmware ===
#define FW_VERSION      "2.1"
#define FRAME_MS        16      // ~60fps

// === Fade-Geschwindigkeiten ===
#define FADE_NORMAL     0.08f   // Standard-Uebergang (~500ms)
#define FADE_SLOW       0.03f   // Langsamer Uebergang (~1s)
#define FADE_FAST       0.20f   // Schneller Uebergang (~200ms)

// === LED Strip ===
Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// ============================================================
//                        ZUSTAENDE
// ============================================================

enum State {
  ST_OFF,           // LEDs aus
  ST_IDLE,          // Standby: sanftes blaues Atmen
  ST_PROGRESS,      // Scan: gruener Fortschrittsbalken
  ST_DONE,          // Fertig: gruenes Atmen (naechstes Geraet!)
  ST_WAITING,       // Warte: gelbes/amber Atmen (Eingabe noetig)
  ST_ERROR,         // Fehler: rotes Atmen (zum PC gehen)
  ST_SAVE,          // Speichern: weiss/gold Atmen (EEPROM schreibt)
  ST_CALIBRATED,    // Komplett kalibriert: Regenbogen-Sweep (Belohnung)
  ST_CONNECT,       // Handstueck erkannt: gruener Flash -> auto IDLE
  ST_DISCONNECTED   // Verbindung verloren (Heartbeat-Timeout)
};

// ============================================================
//                    DISPLAY-SYSTEM (Fade)
//
// Doppelpuffer: dispX[] = aktuell angezeigt, targX[] = Ziel.
// Jeder Frame interpoliert disp sanft Richtung targ.
// Dadurch automatisch weiche Uebergaenge bei JEDEM Wechsel.
// ============================================================

float dispR[NUM_LEDS], dispG[NUM_LEDS], dispB[NUM_LEDS];
float targR[NUM_LEDS], targG[NUM_LEDS], targB[NUM_LEDS];

// Helligkeit mit sanftem Uebergang
float currentBrightness = 60.0f;
float targetBrightness  = 60.0f;

// Aktuelle Fade-Geschwindigkeit (wird pro Zustand gesetzt)
float fadeSpeed = FADE_NORMAL;

// ============================================================
//                     GLOBALE VARIABLEN
// ============================================================

State currentState  = ST_IDLE;
float smoothProgress = 0.0f;
float targetProgress = 0.0f;

unsigned long phase     = 0;    // Animations-Phasenzaehler
unsigned long lastFrame = 0;

// Heartbeat-Watchdog
unsigned long lastHeartbeat    = 0;
unsigned long heartbeatTimeout = 0;   // ms, 0 = deaktiviert
State stateBeforeDisconnect    = ST_IDLE;

// Button
bool buttonPressed          = false;
unsigned long buttonDebounce = 0;

// Flash-Effekt (bei DONE- und CONNECT-Uebergang)
bool flashActive         = false;
unsigned long flashStart = 0;
#define FLASH_DURATION   500   // ms

// Connect-Flash (auto-Rueckkehr zu IDLE)
unsigned long connectStart = 0;
#define CONNECT_DURATION 800   // ms bis auto-IDLE

// LED-Richtung (Montage oben/unten am Monitor)
bool flipped = false;

// ============================================================
//              STATE-FARBEN (konfigurierbar)
//
// Default-Farben pro Zustand. Aenderbar per STATECOLOR-Befehl.
// Index = State-Enum (ST_OFF=0 ... ST_DISCONNECTED=9)
// ============================================================

//                        OFF  IDLE  PROG  DONE  WAIT  ERR   SAVE       CALIB CONN  DISC
uint8_t stR[] =         { 0,   0,    0,    0,    255,  255,  255,       0,    0,    255 };
uint8_t stG[] =         { 0,   0,    120,  255,  160,  0,    220,       0,    255,  100 };
uint8_t stB[] =         { 0,   50,   255,  0,    0,    0,    120,       0,    0,    0   };

// Seriell
char cmdBuffer[CMD_BUFFER_SIZE];
uint8_t cmdPos = 0;

// Forward Declaration (wird von computeConnectTargets benoetigt)
void changeState(State newState);

// ============================================================
//                     HILFSFUNKTIONEN
// ============================================================

void setTarget(uint16_t i, float r, float g, float b) {
  targR[i] = r;
  targG[i] = g;
  targB[i] = b;
}

void setAllTargets(float r, float g, float b) {
  for (uint16_t i = 0; i < NUM_LEDS; i++) {
    targR[i] = r;
    targG[i] = g;
    targB[i] = b;
  }
}

// Sinus-basiertes Atmen (Ergebnis: 0.0 - 1.0)
float breathe(float speed) {
  return (sin(phase * speed) + 1.0f) * 0.5f;
}

float clampF(float v) {
  if (v < 0.0f) return 0.0f;
  if (v > 255.0f) return 255.0f;
  return v;
}

// ============================================================
//                     DISPLAY UPDATE
// ============================================================

void updateDisplay() {
  // Helligkeit sanft ueberblenden
  currentBrightness += (targetBrightness - currentBrightness) * 0.05f;
  float brightScale = currentBrightness / 255.0f;

  for (uint16_t i = 0; i < NUM_LEDS; i++) {
    // Sanfte Interpolation zum Zielwert
    dispR[i] += (targR[i] - dispR[i]) * fadeSpeed;
    dispG[i] += (targG[i] - dispG[i]) * fadeSpeed;
    dispB[i] += (targB[i] - dispB[i]) * fadeSpeed;

    // Helligkeit anwenden und an Strip senden (mit Flip)
    uint8_t r = (uint8_t)clampF(dispR[i] * brightScale);
    uint8_t g = (uint8_t)clampF(dispG[i] * brightScale);
    uint8_t b = (uint8_t)clampF(dispB[i] * brightScale);
    uint16_t idx = flipped ? (NUM_LEDS - 1 - i) : i;
    strip.setPixelColor(idx, strip.Color(r, g, b));
  }
  strip.show();
}

// ============================================================
//                   ANIMATIONS-FUNKTIONEN
//
// Berechnen nur ZIEL-Farben. Das Fade-System macht den Rest.
// ============================================================

// Helper: Atmen mit konfigurierbarer Farbe aus stR/stG/stB
void computeBreathingForState(State st, float speed, float minFrac) {
  fadeSpeed = FADE_NORMAL;
  float breath = breathe(speed);
  float scale = minFrac + breath * (1.0f - minFrac);
  setAllTargets(stR[st] * scale, stG[st] * scale, stB[st] * scale);
}

// --- IDLE: Sanftes Atmen (Default: Blau) ---
void computeIdleTargets() {
  computeBreathingForState(ST_IDLE, 0.04f, 0.15f);
}

// --- PROGRESS: Gruener Fortschrittsbalken mit Glow ---
void computeProgressTargets() {
  fadeSpeed = FADE_NORMAL;

  // Sanfte Fortschritts-Interpolation
  smoothProgress += (targetProgress - smoothProgress) * 0.12f;

  float ledsToLight = smoothProgress * NUM_LEDS / 100.0f;
  uint16_t fullLeds = (uint16_t)ledsToLight;
  float partial = ledsToLight - (float)fullLeds;

  // Dezenter Schimmer auf gefuelltem Bereich
  float shimmer = breathe(0.15f) * 0.12f;

  float cR = stR[ST_PROGRESS], cG = stG[ST_PROGRESS], cB = stB[ST_PROGRESS];

  for (uint16_t i = 0; i < NUM_LEDS; i++) {
    if (i < fullLeds) {
      // Gefuellt: State-Farbe mit Positions-Gradient + Schimmer
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

// --- DONE: Atmen (Default: Gruen, hell, aus der Ferne sichtbar) ---
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

// --- WAITING: Atmen (Default: Gelb/Amber) ---
void computeWaitingTargets() {
  computeBreathingForState(ST_WAITING, 0.04f, 0.10f);
}

// --- ERROR: Atmen schneller (Default: Rot) ---
void computeErrorTargets() {
  computeBreathingForState(ST_ERROR, 0.07f, 0.05f);
}

// --- DISCONNECTED: Pulsieren auf mittleren LEDs (Default: Orange) ---
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

// --- SAVE: Atmen (Default: Weiss/Gold) ---
void computeSaveTargets() {
  computeBreathingForState(ST_SAVE, 0.05f, 0.35f);
}

// --- CALIBRATED: Regenbogen-Sweep (Belohnungs-Animation) ---
void computeCalibratedTargets() {
  fadeSpeed = FADE_NORMAL;

  // Regenbogen wandert langsam ueber die Leiste
  float hueOffset = phase * 0.8f;  // Geschwindigkeit (~3s fuer einen Durchlauf)

  for (uint16_t i = 0; i < NUM_LEDS; i++) {
    // HSV -> RGB Konvertierung mit Position + Offset
    float hue = fmod((float)i / NUM_LEDS * 360.0f + hueOffset, 360.0f);
    float h = hue / 60.0f;
    int hi = (int)h % 6;
    float f = h - (int)h;

    float v = 200.0f;  // Helligkeit
    float p = 0.0f;
    float q = v * (1.0f - f);
    float t = v * f;

    float r, g, b;
    switch (hi) {
      case 0: r = v; g = t; b = p; break;
      case 1: r = q; g = v; b = p; break;
      case 2: r = p; g = v; b = t; break;
      case 3: r = p; g = q; b = v; break;
      case 4: r = t; g = p; b = v; break;
      default: r = v; g = p; b = q; break;
    }
    setTarget(i, r, g, b);
  }
}

// --- CONNECT: Flash -> automatisch zurueck zu IDLE (Default: Gruen) ---
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

// --- OFF: Alles dunkel ---
void computeOffTargets() {
  fadeSpeed = FADE_SLOW;
  setAllTargets(0, 0, 0);
}

// ============================================================
//                    ZUSTANDSWECHSEL
// ============================================================

void changeState(State newState) {
  if (newState == currentState) return;

  // Flash-Effekt bei Uebergang zu DONE
  if (newState == ST_DONE) {
    flashActive = true;
    flashStart = millis();
  }

  // Connect-Flash Timer starten
  if (newState == ST_CONNECT) {
    connectStart = millis();
  }

  // Fortschritt zuruecksetzen bei neuem Scan
  if (newState == ST_PROGRESS && currentState != ST_PROGRESS) {
    smoothProgress = 0.0f;
    targetProgress = 0.0f;
  }

  currentState = newState;
}

// ============================================================
//                   BEFEHLSVERARBEITUNG
// ============================================================

void processCommand(const char* cmd) {
  // Jeder empfangene Befehl setzt den Heartbeat-Timer zurueck
  lastHeartbeat = millis();

  // Bei Reconnect nach Disconnect: vorherigen Zustand wiederherstellen
  if (currentState == ST_DISCONNECTED) {
    changeState(stateBeforeDisconnect);
  }

  // --- Befehle ohne Parameter ---

  if (strcmp(cmd, "PING") == 0) {
    Serial.println("PONG");
    return;
  }

  if (strcmp(cmd, "INFO") == 0) {
    Serial.print("INFO:PANDA_CUSTOM,");
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

  // --- Befehle mit Parameter (CMD:VALUE) ---

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
    if (currentState != ST_PROGRESS) {
      changeState(ST_PROGRESS);
    }
    Serial.println("OK");
  }
  else if (strcmp(command, "STATE") == 0) {
    if      (strcmp(value, "IDLE") == 0)       changeState(ST_IDLE);
    else if (strcmp(value, "DONE") == 0)       changeState(ST_DONE);
    else if (strcmp(value, "WAITING") == 0)    changeState(ST_WAITING);
    else if (strcmp(value, "ERROR") == 0)      changeState(ST_ERROR);
    else if (strcmp(value, "SAVE") == 0)       changeState(ST_SAVE);
    else if (strcmp(value, "CALIBRATED") == 0) changeState(ST_CALIBRATED);
    else if (strcmp(value, "CONNECT") == 0)    changeState(ST_CONNECT);
    else if (strcmp(value, "OFF") == 0)        changeState(ST_OFF);
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
    // Format: STATECOLOR:STATENAME,R,G,B
    char buf[32];
    strncpy(buf, value, 31);
    buf[31] = '\0';

    char* tok = strtok(buf, ",");
    if (!tok) { Serial.println("ERR:INVALID_ARGS"); return; }

    int si = -1;
    if      (strcmp(tok, "IDLE") == 0)         si = ST_IDLE;
    else if (strcmp(tok, "PROGRESS") == 0)     si = ST_PROGRESS;
    else if (strcmp(tok, "DONE") == 0)         si = ST_DONE;
    else if (strcmp(tok, "WAITING") == 0)      si = ST_WAITING;
    else if (strcmp(tok, "ERROR") == 0)        si = ST_ERROR;
    else if (strcmp(tok, "SAVE") == 0)         si = ST_SAVE;
    else if (strcmp(tok, "CONNECT") == 0)      si = ST_CONNECT;
    else if (strcmp(tok, "DISCONNECTED") == 0) si = ST_DISCONNECTED;

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

// ============================================================
//                         SETUP
// ============================================================

void setup() {
  Serial.begin(SERIAL_BAUD);

  strip.begin();
  strip.setBrightness(255);   // Helligkeit wird manuell skaliert
  strip.clear();
  strip.show();

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // Display-Puffer initialisieren
  memset(dispR, 0, sizeof(dispR));
  memset(dispG, 0, sizeof(dispG));
  memset(dispB, 0, sizeof(dispB));
  memset(targR, 0, sizeof(targR));
  memset(targG, 0, sizeof(targG));
  memset(targB, 0, sizeof(targB));

  // Startup: sanfter Regenbogen-Sweep
  for (uint16_t i = 0; i < NUM_LEDS; i++) {
    uint16_t hue = i * 65536 / NUM_LEDS;
    uint32_t color = strip.gamma32(strip.ColorHSV(hue, 255, (uint8_t)currentBrightness));
    strip.setPixelColor(i, color);

    // Display-Puffer synchronisieren fuer sanften Uebergang danach
    dispR[i] = (float)((color >> 16) & 0xFF);
    dispG[i] = (float)((color >> 8) & 0xFF);
    dispB[i] = (float)(color & 0xFF);

    strip.show();
    delay(25);
  }
  delay(400);

  // Fade-System uebernimmt ab hier den Uebergang zu Idle
  currentState = ST_IDLE;
  lastFrame = millis();
  lastHeartbeat = millis();

  Serial.println("READY:PANDA_CUSTOM,25,V2.1");
}

// ============================================================
//                       HAUPTSCHLEIFE
// ============================================================

void loop() {
  // === Serielle Befehle einlesen ===
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

  // === Button: DONE/ERROR/WAITING quittieren -> IDLE ===
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

  // === Heartbeat-Watchdog ===
  if (heartbeatTimeout > 0 &&
      currentState != ST_OFF &&
      currentState != ST_DISCONNECTED) {
    if (millis() - lastHeartbeat > heartbeatTimeout) {
      stateBeforeDisconnect = currentState;
      currentState = ST_DISCONNECTED;
    }
  }

  // === Frame-Timing (60fps) ===
  unsigned long now = millis();
  if (now - lastFrame < FRAME_MS) return;
  lastFrame = now;
  phase++;

  // === Ziel-Farben berechnen (je nach Zustand) ===
  switch (currentState) {
    case ST_IDLE:         computeIdleTargets();         break;
    case ST_PROGRESS:     computeProgressTargets();     break;
    case ST_DONE:         computeDoneTargets();         break;
    case ST_WAITING:      computeWaitingTargets();      break;
    case ST_ERROR:        computeErrorTargets();        break;
    case ST_SAVE:         computeSaveTargets();         break;
    case ST_CALIBRATED:   computeCalibratedTargets();   break;
    case ST_CONNECT:      computeConnectTargets();      break;
    case ST_DISCONNECTED: computeDisconnectedTargets(); break;
    case ST_OFF:          computeOffTargets();          break;
  }

  // === Sanftes Fade + Display-Update ===
  updateDisplay();
}
