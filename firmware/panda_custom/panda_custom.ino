/**
 * Panda Status Custom Firmware
 * ESP32-C3-MINI | 25x WS2812 an GPIO5 | CH340K USB-Serial
 *
 * Serielle Steuerung der LED-Leiste fuer Fortschrittsanzeige
 * und Statusvisualisierung aus Python-Programmen.
 *
 * Protokoll (115200 Baud, Newline-terminiert):
 *   PROGRESS:<0-100>          Fortschrittsbalken (0-100%)
 *   STATE:<zustand>           IDLE|SCANNING|ERROR|DONE|CALIBRATING|BUSY
 *   COLOR:<r>,<g>,<b>         Basisfarbe setzen (0-255 pro Kanal)
 *   BRIGHTNESS:<0-255>        Helligkeit setzen
 *   SOLID:<r>,<g>,<b>         Alle LEDs auf eine Farbe
 *   LED:<nr>,<r>,<g>,<b>      Einzelne LED setzen (0-24)
 *   CLEAR                     Alle LEDs aus
 *   PING                      Antwortet mit PONG (Verbindungstest)
 *   INFO                      Gibt Geraeteinfo zurueck
 *
 * Antworten:
 *   OK                        Befehl ausgefuehrt
 *   PONG                      Antwort auf PING
 *   ERR:<nachricht>           Fehler
 *   INFO:PANDA_CUSTOM,25,V1.0 Geraeteinfo
 */

#include <Adafruit_NeoPixel.h>

// === Hardware-Konfiguration ===
#define LED_PIN       5
#define NUM_LEDS      25
#define BUTTON_PIN    9

// === Serielle Kommunikation ===
#define SERIAL_BAUD   115200
#define CMD_BUFFER_SIZE 64

// === LED Strip ===
Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// === Zustaende ===
enum State {
  ST_IDLE,
  ST_SCANNING,
  ST_ERROR,
  ST_DONE,
  ST_CALIBRATING,
  ST_BUSY,
  ST_PROGRESS,
  ST_SOLID,
  ST_MANUAL
};

// === Globale Variablen ===
State currentState = ST_IDLE;
uint8_t progress = 0;            // 0-100
uint8_t brightness = 60;         // 0-255 (Standard: moderate Helligkeit)
uint8_t baseR = 0, baseG = 100, baseB = 255;  // Standard: Blau
char cmdBuffer[CMD_BUFFER_SIZE];
uint8_t cmdPos = 0;
unsigned long lastAnimUpdate = 0;
uint8_t animFrame = 0;
bool buttonPressed = false;
unsigned long buttonDebounce = 0;

// === Hilfsfunktionen ===

// Farbe mit Helligkeit skalieren
uint32_t scaledColor(uint8_t r, uint8_t g, uint8_t b) {
  return strip.Color(
    (uint16_t)r * brightness / 255,
    (uint16_t)g * brightness / 255,
    (uint16_t)b * brightness / 255
  );
}

// Fortschrittsbalken anzeigen
void showProgress(uint8_t pct) {
  strip.clear();

  // Anzahl voller LEDs
  uint16_t fullLeds = (uint16_t)pct * NUM_LEDS / 100;
  // Anteil der naechsten LED (fuer sanften Uebergang)
  uint8_t partialBrightness = ((uint16_t)pct * NUM_LEDS * 255 / 100) % 255;

  for (uint16_t i = 0; i < NUM_LEDS; i++) {
    if (i < fullLeds) {
      // Farbverlauf: Blau -> Gruen -> fertig
      uint8_t r, g, b;
      float ratio = (float)i / NUM_LEDS;
      if (ratio < 0.5) {
        // Blau -> Cyan
        r = 0;
        g = (uint8_t)(ratio * 2 * 200);
        b = 200;
      } else {
        // Cyan -> Gruen
        r = 0;
        g = 200;
        b = (uint8_t)((1.0 - (ratio - 0.5) * 2) * 200);
      }
      strip.setPixelColor(i, scaledColor(r, g, b));
    } else if (i == fullLeds && partialBrightness > 0) {
      // Teilweise beleuchtete LED
      uint8_t g = (uint8_t)((float)partialBrightness / 255 * 150);
      strip.setPixelColor(i, scaledColor(0, g, 150));
    }
    // Rest bleibt aus
  }
  strip.show();
}

// Idle-Animation: sanftes Atmen in Blau
void animIdle() {
  unsigned long now = millis();
  if (now - lastAnimUpdate < 30) return;
  lastAnimUpdate = now;
  animFrame++;

  // Sinuswelle fuer Atmen-Effekt
  float breath = (sin(animFrame * 0.05) + 1.0) * 0.5;  // 0.0 - 1.0
  uint8_t b = 20 + (uint8_t)(breath * 80);

  for (uint16_t i = 0; i < NUM_LEDS; i++) {
    strip.setPixelColor(i, scaledColor(0, 0, b));
  }
  strip.show();
}

// Scanning-Animation: laufender Punkt
void animScanning() {
  unsigned long now = millis();
  if (now - lastAnimUpdate < 40) return;
  lastAnimUpdate = now;
  animFrame++;

  uint8_t pos = animFrame % (NUM_LEDS * 2);
  if (pos >= NUM_LEDS) pos = NUM_LEDS * 2 - 1 - pos;  // Hin und zurueck

  for (uint16_t i = 0; i < NUM_LEDS; i++) {
    int dist = abs((int)i - (int)pos);
    if (dist == 0) {
      strip.setPixelColor(i, scaledColor(0, 150, 255));
    } else if (dist <= 3) {
      uint8_t fade = 255 / (dist + 1);
      strip.setPixelColor(i, scaledColor(0, fade / 2, fade));
    } else {
      strip.setPixelColor(i, scaledColor(0, 0, 10));
    }
  }
  strip.show();
}

// Error-Animation: rotes Blinken
void animError() {
  unsigned long now = millis();
  if (now - lastAnimUpdate < 50) return;
  lastAnimUpdate = now;
  animFrame++;

  bool on = (animFrame / 10) % 2 == 0;
  for (uint16_t i = 0; i < NUM_LEDS; i++) {
    if (on) {
      strip.setPixelColor(i, scaledColor(255, 0, 0));
    } else {
      strip.setPixelColor(i, scaledColor(40, 0, 0));
    }
  }
  strip.show();
}

// Done-Animation: gruenes Leuchten mit Funkeln
void animDone() {
  unsigned long now = millis();
  if (now - lastAnimUpdate < 50) return;
  lastAnimUpdate = now;
  animFrame++;

  for (uint16_t i = 0; i < NUM_LEDS; i++) {
    uint8_t sparkle = random(180, 255);
    strip.setPixelColor(i, scaledColor(0, sparkle, 0));
  }
  strip.show();
}

// Calibrating-Animation: orange Lauflicht
void animCalibrating() {
  unsigned long now = millis();
  if (now - lastAnimUpdate < 60) return;
  lastAnimUpdate = now;
  animFrame++;

  for (uint16_t i = 0; i < NUM_LEDS; i++) {
    float wave = (sin((i + animFrame) * 0.4) + 1.0) * 0.5;
    strip.setPixelColor(i, scaledColor(
      200,
      (uint8_t)(100 * wave),
      0
    ));
  }
  strip.show();
}

// Busy-Animation: rotierender Kreis (lila)
void animBusy() {
  unsigned long now = millis();
  if (now - lastAnimUpdate < 50) return;
  lastAnimUpdate = now;
  animFrame++;

  uint8_t pos = animFrame % NUM_LEDS;
  for (uint16_t i = 0; i < NUM_LEDS; i++) {
    int dist = (i - pos + NUM_LEDS) % NUM_LEDS;
    if (dist < 5) {
      uint8_t fade = 255 - dist * 50;
      strip.setPixelColor(i, scaledColor(fade, 0, fade));
    } else {
      strip.setPixelColor(i, scaledColor(5, 0, 5));
    }
  }
  strip.show();
}

// === Befehlsverarbeitung ===

int parseInt(const char* str) {
  return atoi(str);
}

bool parseColor(const char* str, uint8_t &r, uint8_t &g, uint8_t &b) {
  int vals[3];
  int idx = 0;
  char buf[12];
  strncpy(buf, str, 11);
  buf[11] = '\0';

  char* tok = strtok(buf, ",");
  while (tok && idx < 3) {
    vals[idx++] = atoi(tok);
    tok = strtok(NULL, ",");
  }
  if (idx != 3) return false;
  r = constrain(vals[0], 0, 255);
  g = constrain(vals[1], 0, 255);
  b = constrain(vals[2], 0, 255);
  return true;
}

void processCommand(const char* cmd) {
  // PING
  if (strcmp(cmd, "PING") == 0) {
    Serial.println("PONG");
    return;
  }

  // INFO
  if (strcmp(cmd, "INFO") == 0) {
    Serial.println("INFO:PANDA_CUSTOM,25,V1.0");
    return;
  }

  // CLEAR
  if (strcmp(cmd, "CLEAR") == 0) {
    currentState = ST_MANUAL;
    strip.clear();
    strip.show();
    Serial.println("OK");
    return;
  }

  // Befehle mit Parametern (CMD:VALUE Format)
  char* colon = strchr(cmd, ':');
  if (!colon) {
    Serial.println("ERR:UNKNOWN_CMD");
    return;
  }

  // Befehl und Wert trennen
  *colon = '\0';
  const char* command = cmd;
  const char* value = colon + 1;

  // PROGRESS:<0-100>
  if (strcmp(command, "PROGRESS") == 0) {
    int p = parseInt(value);
    progress = constrain(p, 0, 100);
    currentState = ST_PROGRESS;
    showProgress(progress);
    Serial.println("OK");
  }
  // STATE:<zustand>
  else if (strcmp(command, "STATE") == 0) {
    if (strcmp(value, "IDLE") == 0) currentState = ST_IDLE;
    else if (strcmp(value, "SCANNING") == 0) currentState = ST_SCANNING;
    else if (strcmp(value, "ERROR") == 0) currentState = ST_ERROR;
    else if (strcmp(value, "DONE") == 0) currentState = ST_DONE;
    else if (strcmp(value, "CALIBRATING") == 0) currentState = ST_CALIBRATING;
    else if (strcmp(value, "BUSY") == 0) currentState = ST_BUSY;
    else {
      Serial.println("ERR:UNKNOWN_STATE");
      return;
    }
    animFrame = 0;
    Serial.println("OK");
  }
  // COLOR:<r>,<g>,<b>
  else if (strcmp(command, "COLOR") == 0) {
    if (parseColor(value, baseR, baseG, baseB)) {
      Serial.println("OK");
    } else {
      Serial.println("ERR:INVALID_COLOR");
    }
  }
  // BRIGHTNESS:<0-255>
  else if (strcmp(command, "BRIGHTNESS") == 0) {
    brightness = constrain(parseInt(value), 0, 255);
    Serial.println("OK");
  }
  // SOLID:<r>,<g>,<b>
  else if (strcmp(command, "SOLID") == 0) {
    uint8_t r, g, b;
    if (parseColor(value, r, g, b)) {
      currentState = ST_SOLID;
      for (uint16_t i = 0; i < NUM_LEDS; i++) {
        strip.setPixelColor(i, scaledColor(r, g, b));
      }
      strip.show();
      Serial.println("OK");
    } else {
      Serial.println("ERR:INVALID_COLOR");
    }
  }
  // LED:<nr>,<r>,<g>,<b>
  else if (strcmp(command, "LED") == 0) {
    int vals[4];
    int idx = 0;
    char buf[20];
    strncpy(buf, value, 19);
    buf[19] = '\0';
    char* tok = strtok(buf, ",");
    while (tok && idx < 4) {
      vals[idx++] = atoi(tok);
      tok = strtok(NULL, ",");
    }
    if (idx == 4 && vals[0] >= 0 && vals[0] < NUM_LEDS) {
      currentState = ST_MANUAL;
      strip.setPixelColor(vals[0], scaledColor(vals[1], vals[2], vals[3]));
      strip.show();
      Serial.println("OK");
    } else {
      Serial.println("ERR:INVALID_LED");
    }
  }
  else {
    Serial.println("ERR:UNKNOWN_CMD");
  }
}

// === Setup ===
void setup() {
  Serial.begin(SERIAL_BAUD);

  // LED Strip initialisieren
  strip.begin();
  strip.setBrightness(255);  // Helligkeit wird manuell skaliert
  strip.clear();
  strip.show();

  // Button als Input mit Pull-Up
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // Startanimation: kurzer Regenbogen-Sweep
  for (uint16_t i = 0; i < NUM_LEDS; i++) {
    uint16_t hue = i * 65536 / NUM_LEDS;
    strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(hue, 255, brightness)));
    strip.show();
    delay(30);
  }
  delay(300);
  strip.clear();
  strip.show();

  Serial.println("READY:PANDA_CUSTOM,25,V1.0");
}

// === Hauptschleife ===
void loop() {
  // Serielle Befehle lesen
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

  // Button auslesen (Toggle zwischen Idle und Off)
  bool btnState = digitalRead(BUTTON_PIN) == LOW;
  if (btnState && !buttonPressed && (millis() - buttonDebounce > 200)) {
    buttonPressed = true;
    buttonDebounce = millis();
    // Button-Druck: kurze Bestaetigung
    for (uint16_t i = 0; i < NUM_LEDS; i++) {
      strip.setPixelColor(i, scaledColor(255, 255, 255));
    }
    strip.show();
    delay(100);
  }
  if (!btnState) buttonPressed = false;

  // Animationen ausfuehren (nur fuer animierte Zustaende)
  switch (currentState) {
    case ST_IDLE:        animIdle(); break;
    case ST_SCANNING:    animScanning(); break;
    case ST_ERROR:       animError(); break;
    case ST_DONE:        animDone(); break;
    case ST_CALIBRATING: animCalibrating(); break;
    case ST_BUSY:        animBusy(); break;
    case ST_PROGRESS:    break;  // Statisch, kein Update noetig
    case ST_SOLID:       break;  // Statisch
    case ST_MANUAL:      break;  // Manuell
  }
}
