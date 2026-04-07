/**
 * Claude Rider Firmware V3.0
 * ==========================
 * ESP32-C3-MINI | 25x WS2812 an GPIO5 | CH340K USB-Serial
 *
 * Remote status display controlled via USB-Serial.
 * All animations use smooth fades — no hard blinking.
 *
 * Core technique: Double-buffer with float interpolation.
 * Each frame computes target colors; the fade system interpolates
 * smoothly toward them. All state transitions are automatically
 * soft crossfades.
 *
 * Serial Protocol (115200 Baud, newline-terminated):
 *   PING                          -> PONG
 *   INFO                          -> INFO:CLAUDE_RIDER,25,V3.0
 *   PROGRESS:<0-100>              -> OK     Progress bar
 *   STATE:<state>                 -> OK     IDLE|KNIGHT_RIDER|DONE|WAITING|ERROR|SAVE|CONNECT|OFF
 *   BRIGHTNESS:<0-255>            -> OK     Brightness (smooth transition)
 *   SPEED:<1-5>                   -> OK     Knight Rider speed (1=slow, 5=fast)
 *   STATECOLOR:<st>,<r>,<g>,<b>  -> OK     Set color per state
 *   FLIP:<0|1>                    -> OK     Mirror LED direction
 *   TIMEOUT:<seconds>             -> OK     Heartbeat timeout (0=off)
 *   HEARTBEAT                     -> OK     Reset watchdog
 *   CLEAR                         -> OK     All off
 */

#include <Adafruit_NeoPixel.h>

// === Hardware ===
#define LED_PIN         5
#define NUM_LEDS        25
#define BUTTON_PIN      9
#define SERIAL_BAUD     115200
#define CMD_BUFFER_SIZE 64

// === Firmware ===
#define FW_ID           "CLAUDE_RIDER"
#define FW_VERSION      "3.0"
#define FRAME_MS        16      // ~60fps

// === Fade speeds ===
#define FADE_NORMAL     0.08f   // Standard transition (~500ms)
#define FADE_SLOW       0.03f   // Slow transition (~1s)
#define FADE_FAST       0.20f   // Fast transition (~200ms)

// === Knight Rider ===
#define KNIGHT_TAIL_LEN 8

// Speed table for SPEED:1-5
const float knightSpeedTable[5] = { 0.06f, 0.12f, 0.18f, 0.25f, 0.35f };

// === LED Strip ===
Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// ============================================================
//                        STATES
// ============================================================

enum State {
  ST_OFF,           // LEDs off
  ST_IDLE,          // Standby: soft blue breathing
  ST_KNIGHT_RIDER,  // Classic KITT scanner animation
  ST_PROGRESS,      // Scan: progress bar
  ST_DONE,          // Finished: green breathing
  ST_WAITING,       // Waiting: yellow/amber breathing
  ST_ERROR,         // Error: red breathing (fast)
  ST_SAVE,          // Saving: white/gold breathing
  ST_CONNECT,       // Connected: green flash -> auto IDLE
  ST_DISCONNECTED   // Connection lost (heartbeat timeout)
};

// ============================================================
//                    DISPLAY SYSTEM (Fade)
//
// Double-buffer: dispX[] = currently displayed, targX[] = target.
// Each frame interpolates disp smoothly toward targ.
// All state transitions are automatically soft crossfades.
// ============================================================

float dispR[NUM_LEDS], dispG[NUM_LEDS], dispB[NUM_LEDS];
float targR[NUM_LEDS], targG[NUM_LEDS], targB[NUM_LEDS];

// Brightness with smooth transition
float currentBrightness = 60.0f;
float targetBrightness  = 60.0f;

// Current fade speed (set per state)
float fadeSpeed = FADE_NORMAL;

// ============================================================
//                     GLOBAL VARIABLES
// ============================================================

State currentState  = ST_IDLE;
float smoothProgress = 0.0f;
float targetProgress = 0.0f;

unsigned long phase     = 0;    // Animation phase counter
unsigned long lastFrame = 0;

// Heartbeat watchdog
unsigned long lastHeartbeat    = 0;
unsigned long heartbeatTimeout = 0;   // ms, 0 = disabled
State stateBeforeDisconnect    = ST_IDLE;

// Button
bool buttonPressed          = false;
unsigned long buttonDebounce = 0;

// Flash effect (on DONE and CONNECT transition)
bool flashActive         = false;
unsigned long flashStart = 0;
#define FLASH_DURATION   500   // ms

// Connect flash (auto-return to IDLE)
unsigned long connectStart = 0;
#define CONNECT_DURATION 800   // ms until auto-IDLE

// LED direction (mounting top/bottom)
bool flipped = false;

// Knight Rider position (float for smooth movement)
float knightPos  = 0.0f;
float knightDir  = 1.0f;
float knightSpeed = 0.18f;   // Default speed (SPEED:3)

// ============================================================
//              STATE COLORS (configurable)
//
// Default colors per state. Changeable via STATECOLOR command.
// Index = State enum (ST_OFF=0 ... ST_DISCONNECTED=9)
// ============================================================

//                       OFF  IDLE  KR    PROG  DONE  WAIT  ERR  SAVE  CONN  DISC
uint8_t stR[] =        { 0,   0,    255,  0,    0,    255,  255, 255,  0,    255 };
uint8_t stG[] =        { 0,   0,    10,   120,  255,  160,  0,   220,  255,  100 };
uint8_t stB[] =        { 0,   50,   0,    255,  0,    0,    0,   120,  0,    0   };

// Serial command buffer
char cmdBuffer[CMD_BUFFER_SIZE];
uint8_t cmdPos = 0;

// Forward declaration (needed by computeConnectTargets)
void changeState(State newState);

// ============================================================
//                     HELPER FUNCTIONS
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

// Sine-based breathing (result: 0.0 - 1.0)
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
  // Smooth brightness transition
  currentBrightness += (targetBrightness - currentBrightness) * 0.05f;
  float brightScale = currentBrightness / 255.0f;

  for (uint16_t i = 0; i < NUM_LEDS; i++) {
    // Smooth interpolation toward target
    dispR[i] += (targR[i] - dispR[i]) * fadeSpeed;
    dispG[i] += (targG[i] - dispG[i]) * fadeSpeed;
    dispB[i] += (targB[i] - dispB[i]) * fadeSpeed;

    // Apply brightness and send to strip (with flip)
    uint8_t r = (uint8_t)clampF(dispR[i] * brightScale);
    uint8_t g = (uint8_t)clampF(dispG[i] * brightScale);
    uint8_t b = (uint8_t)clampF(dispB[i] * brightScale);
    uint16_t idx = flipped ? (NUM_LEDS - 1 - i) : i;
    strip.setPixelColor(idx, strip.Color(r, g, b));
  }
  strip.show();
}

// ============================================================
//                   ANIMATION FUNCTIONS
//
// Only compute TARGET colors. The fade system does the rest.
// ============================================================

// Helper: breathing with configurable color from stR/stG/stB
void computeBreathingForState(State st, float speed, float minFrac) {
  fadeSpeed = FADE_NORMAL;
  float breath = breathe(speed);
  float scale = minFrac + breath * (1.0f - minFrac);
  setAllTargets(stR[st] * scale, stG[st] * scale, stB[st] * scale);
}

// --- IDLE: Soft breathing (default: blue) ---
void computeIdleTargets() {
  computeBreathingForState(ST_IDLE, 0.04f, 0.15f);
}

// --- KNIGHT RIDER: Classic KITT scanner with exponential decay tail ---
void computeKnightRiderTargets() {
  fadeSpeed = FADE_FAST;

  // Advance scanner position
  knightPos += knightDir * knightSpeed;

  // Bounce at ends
  if (knightPos >= (float)(NUM_LEDS - 1)) {
    knightPos = (float)(NUM_LEDS - 1);
    knightDir = -1.0f;
  } else if (knightPos <= 0.0f) {
    knightPos = 0.0f;
    knightDir = 1.0f;
  }

  float cR = stR[ST_KNIGHT_RIDER];
  float cG = stG[ST_KNIGHT_RIDER];
  float cB = stB[ST_KNIGHT_RIDER];

  for (uint16_t i = 0; i < NUM_LEDS; i++) {
    float dist = fabsf((float)i - knightPos);
    float intensity = 0.0f;

    if (dist < (float)KNIGHT_TAIL_LEN) {
      // Exponential decay: bright head, fading tail
      intensity = powf(2.5f, -(dist));
    }

    setTarget(i, cR * intensity, cG * intensity, cB * intensity);
  }
}

// --- PROGRESS: Progress bar with glow ---
void computeProgressTargets() {
  fadeSpeed = FADE_NORMAL;

  // Smooth progress interpolation
  smoothProgress += (targetProgress - smoothProgress) * 0.12f;

  float ledsToLight = smoothProgress * NUM_LEDS / 100.0f;
  uint16_t fullLeds = (uint16_t)ledsToLight;
  float partial = ledsToLight - (float)fullLeds;

  // Subtle shimmer on filled area
  float shimmer = breathe(0.15f) * 0.12f;

  float cR = stR[ST_PROGRESS], cG = stG[ST_PROGRESS], cB = stB[ST_PROGRESS];

  for (uint16_t i = 0; i < NUM_LEDS; i++) {
    if (i < fullLeds) {
      // Filled: state color with position gradient + shimmer
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

// --- DONE: Breathing (default: green, bright) ---
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

// --- WAITING: Breathing (default: yellow/amber) ---
void computeWaitingTargets() {
  computeBreathingForState(ST_WAITING, 0.04f, 0.10f);
}

// --- ERROR: Fast breathing (default: red) ---
void computeErrorTargets() {
  computeBreathingForState(ST_ERROR, 0.07f, 0.05f);
}

// --- DISCONNECTED: Pulsing center LEDs (default: orange) ---
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

// --- SAVE: Breathing (default: white/gold) ---
void computeSaveTargets() {
  computeBreathingForState(ST_SAVE, 0.05f, 0.35f);
}

// --- CONNECT: Flash -> automatically return to IDLE (default: green) ---
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

// --- OFF: All dark ---
void computeOffTargets() {
  fadeSpeed = FADE_SLOW;
  setAllTargets(0, 0, 0);
}

// ============================================================
//                    STATE TRANSITION
// ============================================================

void changeState(State newState) {
  if (newState == currentState) return;

  // Flash effect on transition to DONE
  if (newState == ST_DONE) {
    flashActive = true;
    flashStart = millis();
  }

  // Connect flash timer start
  if (newState == ST_CONNECT) {
    connectStart = millis();
  }

  // Reset progress on new scan
  if (newState == ST_PROGRESS && currentState != ST_PROGRESS) {
    smoothProgress = 0.0f;
    targetProgress = 0.0f;
  }

  // Reset Knight Rider position on entry
  if (newState == ST_KNIGHT_RIDER) {
    knightPos = 0.0f;
    knightDir = 1.0f;
  }

  currentState = newState;
}

// ============================================================
//                   COMMAND PROCESSING
// ============================================================

void processCommand(const char* cmd) {
  // Any received command resets the heartbeat timer
  lastHeartbeat = millis();

  // On reconnect after disconnect: restore previous state
  if (currentState == ST_DISCONNECTED) {
    changeState(stateBeforeDisconnect);
  }

  // --- Commands without parameters ---

  if (strcmp(cmd, "PING") == 0) {
    Serial.println("PONG");
    return;
  }

  if (strcmp(cmd, "INFO") == 0) {
    Serial.print("INFO:");
    Serial.print(FW_ID);
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

  // --- Commands with parameter (CMD:VALUE) ---

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
    if      (strcmp(value, "IDLE") == 0)         changeState(ST_IDLE);
    else if (strcmp(value, "KNIGHT_RIDER") == 0) changeState(ST_KNIGHT_RIDER);
    else if (strcmp(value, "DONE") == 0)         changeState(ST_DONE);
    else if (strcmp(value, "WAITING") == 0)      changeState(ST_WAITING);
    else if (strcmp(value, "ERROR") == 0)        changeState(ST_ERROR);
    else if (strcmp(value, "SAVE") == 0)         changeState(ST_SAVE);
    else if (strcmp(value, "CONNECT") == 0)      changeState(ST_CONNECT);
    else if (strcmp(value, "OFF") == 0)          changeState(ST_OFF);
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
    knightSpeed = knightSpeedTable[s - 1];
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

// ============================================================
//                         SETUP
// ============================================================

void setup() {
  Serial.begin(SERIAL_BAUD);

  strip.begin();
  strip.setBrightness(255);   // Brightness is scaled manually
  strip.clear();
  strip.show();

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // Initialize display buffers
  memset(dispR, 0, sizeof(dispR));
  memset(dispG, 0, sizeof(dispG));
  memset(dispB, 0, sizeof(dispB));
  memset(targR, 0, sizeof(targR));
  memset(targG, 0, sizeof(targG));
  memset(targB, 0, sizeof(targB));

  // Startup animation: Knight Rider scan (one pass back and forth)
  float pos = 0.0f;
  float dir = 1.0f;
  float startSpeed = 0.18f;
  int passes = 0;

  while (passes < 2) {
    pos += dir * startSpeed;

    if (pos >= (float)(NUM_LEDS - 1)) {
      pos = (float)(NUM_LEDS - 1);
      dir = -1.0f;
      passes++;
    } else if (pos <= 0.0f) {
      pos = 0.0f;
      dir = 1.0f;
      if (passes > 0) passes++;
    }

    for (uint16_t i = 0; i < NUM_LEDS; i++) {
      float dist = fabsf((float)i - pos);
      float intensity = 0.0f;
      if (dist < (float)KNIGHT_TAIL_LEN) {
        intensity = powf(2.5f, -dist);
      }
      uint8_t r = (uint8_t)clampF(255.0f * intensity * (currentBrightness / 255.0f));
      uint8_t g = (uint8_t)clampF(10.0f  * intensity * (currentBrightness / 255.0f));
      uint8_t b = 0;
      strip.setPixelColor(i, strip.Color(r, g, b));

      // Sync display buffer for smooth transition afterward
      dispR[i] = 255.0f * intensity;
      dispG[i] = 10.0f  * intensity;
      dispB[i] = 0.0f;
    }
    strip.show();
    delay(16);
  }

  delay(200);

  // Fade system takes over from here
  currentState = ST_IDLE;
  lastFrame = millis();
  lastHeartbeat = millis();

  Serial.print("READY:");
  Serial.print(FW_ID);
  Serial.print(",");
  Serial.print(NUM_LEDS);
  Serial.print(",V");
  Serial.println(FW_VERSION);
}

// ============================================================
//                       MAIN LOOP
// ============================================================

void loop() {
  // === Read serial commands ===
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

  // === Button: acknowledge DONE/ERROR/WAITING -> IDLE ===
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

  // === Heartbeat watchdog ===
  if (heartbeatTimeout > 0 &&
      currentState != ST_OFF &&
      currentState != ST_DISCONNECTED) {
    if (millis() - lastHeartbeat > heartbeatTimeout) {
      stateBeforeDisconnect = currentState;
      currentState = ST_DISCONNECTED;
    }
  }

  // === Frame timing (60fps) ===
  unsigned long now = millis();
  if (now - lastFrame < FRAME_MS) return;
  lastFrame = now;
  phase++;

  // === Compute target colors (per state) ===
  switch (currentState) {
    case ST_IDLE:         computeIdleTargets();         break;
    case ST_KNIGHT_RIDER: computeKnightRiderTargets();  break;
    case ST_PROGRESS:     computeProgressTargets();     break;
    case ST_DONE:         computeDoneTargets();         break;
    case ST_WAITING:      computeWaitingTargets();      break;
    case ST_ERROR:        computeErrorTargets();        break;
    case ST_SAVE:         computeSaveTargets();         break;
    case ST_CONNECT:      computeConnectTargets();      break;
    case ST_DISCONNECTED: computeDisconnectedTargets(); break;
    case ST_OFF:          computeOffTargets();          break;
  }

  // === Smooth fade + display update ===
  updateDisplay();
}
