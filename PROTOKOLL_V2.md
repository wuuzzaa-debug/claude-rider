# Panda Status LED — Serielles Protokoll V2.1

## Uebersicht

Die Panda Status LED-Leiste dient als **Fernstatus-Anzeige** fuer den Kalibrier-Operator.
Der Operator muss nicht am Monitor sitzen — er sieht aus der Ferne:

| Farbe | Bedeutung |
|-------|-----------|
| Hellblau fuellt sich | Scan laeuft |
| Gruen atmet | Fertig — naechstes Geraet einlegen |
| Gelb atmet | Eingabe am PC noetig |
| Rot atmet | Fehler aufgetreten |
| Weiss/Gold atmet | Daten werden gespeichert (EEPROM) |
| Regenbogen wandert | Komplett kalibriert! (Belohnung) |
| Gruener Flash | Handstueck erkannt |
| Blau atmet (gedimmt) | System bereit, nichts aktiv |
| Orange pulsiert (Mitte) | Verbindung zum PC verloren |

Alle Animationen sind **sanfte Fades** (Sinus-basiertes Atmen, keine harten Blinker).

---

## Verbindungsparameter

| Parameter | Wert |
|-----------|------|
| Baudrate | 115200 |
| Datenbits | 8 |
| Stoppbits | 1 |
| Paritaet | Keine |
| Zeilenende | `\n` (LF) |
| USB-Chip | CH340K |
| Timeout empfohlen | 1 Sekunde |

---

## Initialisierung (Python-Seite)

### Schritt-fuer-Schritt

```
 1. COM-Port finden       → CH340 im Geraete-Manager suchen
 2. Port oeffnen          → 115200 Baud, Timeout 1s
 3. 500ms warten          → ESP32 Reset nach USB-Verbindung abwarten
 4. Puffer leeren         → READY-Meldung verwerfen
 5. PING senden           → Antwort muss "PONG" sein
 6. INFO senden           → Antwort: "INFO:PANDA_CUSTOM,25,V2.1"
 7. BRIGHTNESS setzen     → Aus Benutzer-Einstellungen laden
 8. FLIP setzen           → Je nach Montageposition (0 oder 1)
 9. STATECOLOR setzen     → Farben aus Settings laden (optional)
10. TIMEOUT setzen        → Heartbeat aktivieren (empfohlen: 10s)
11. STATE:IDLE senden     → Startposition bestaetigen
```

### Startup-Meldung der Firmware

Nach Reset/Power-On sendet die Firmware automatisch:
```
READY:PANDA_CUSTOM,25,V2.1
```
Diese Meldung dient zur Erkennung — **nicht als Antwort auf einen Befehl**.

---

## Befehlsreferenz

### Format

```
BEFEHL\n              → Ohne Parameter
BEFEHL:WERT\n         → Mit Parameter
```

Antworten kommen immer als eine Zeile mit `\n`.

---

### PING — Verbindungstest

```
Senden:   PING
Antwort:  PONG
```

Einfachster Test ob die Firmware erreichbar ist.

---

### INFO — Geraeteinfo

```
Senden:   INFO
Antwort:  INFO:PANDA_CUSTOM,25,V2.1
```

Format: `INFO:<geraet>,<led_anzahl>,V<version>`

Nutzbar zur Firmware-Versions-Pruefung bei Programmstart.

---

### PROGRESS:\<0-100\> — Fortschrittsbalken

```
Senden:   PROGRESS:0     (Balken leer)
Senden:   PROGRESS:50    (Balken halb voll)
Senden:   PROGRESS:100   (Balken komplett)
Antwort:  OK
```

- Hellblauer Balken fuellt sich von links nach rechts (LED 0 → LED 24)
- **Farbe konfigurierbar** per `STATECOLOR:PROGRESS,r,g,b` (Default: 0,120,255 = Hellblau/Cyan)
- **Sanfte Interpolation**: Sprung von 10 auf 50 wird als weicher Uebergang dargestellt
- Fuehrungs-LED hat einen sanften Glow (kein harter Rand)
- Dezenter Schimmer auf dem gefuellten Bereich (wirkt lebendig)
- Beim ersten `PROGRESS`-Befehl wird automatisch in den Progress-Zustand gewechselt

**Typische Verwendung im Scan:**
```python
# Scan mit 120 Messpunkten
for i in range(120):
    measure_point(i)
    led.set_progress(int(i / 120 * 100))
```

---

### STATE:\<zustand\> — Zustand wechseln

```
Senden:   STATE:IDLE
Senden:   STATE:DONE
Senden:   STATE:WAITING
Senden:   STATE:ERROR
Senden:   STATE:SAVE
Senden:   STATE:CALIBRATED
Senden:   STATE:CONNECT
Senden:   STATE:OFF
Antwort:  OK
```

Bei ungueltigem Zustand: `ERR:UNKNOWN_STATE`

#### Zustandsbeschreibungen

| Zustand | Animation | Tempo | Verwendung |
|---------|-----------|-------|------------|
| **IDLE** | Blaues Atmen, gedimmt | ~2.5s Periode | System bereit, kein Scan aktiv |
| **DONE** | Gruenes Atmen, hell | ~2.0s Periode | Scan abgeschlossen, naechstes Geraet einlegen |
| **WAITING** | Gelb/Amber Atmen | ~2.5s Periode | Benutzereingabe am PC erforderlich |
| **ERROR** | Rotes Atmen, schnell | ~1.4s Periode | Fehler aufgetreten, zum PC gehen |
| **SAVE** | Weiss/Gold Atmen | ~2.0s Periode | EEPROM wird geschrieben, bitte warten |
| **CALIBRATED** | Regenbogen-Sweep | ~3s/Durchlauf | Alle Frequenzen kalibriert — Belohnung! |
| **CONNECT** | Gruener Flash → auto IDLE | ~800ms | Handstueck erkannt, kurze Bestaetigung |
| **OFF** | Alles dunkel (Fade-Out) | ~1s Fade | LEDs ausschalten |

#### Uebergangseffekte

- **Jeder Zustandswechsel** hat einen sanften Fade-Uebergang (~500ms)
- **Wechsel zu DONE**: Zusaetzlicher kurzer heller Blitz (500ms) mit Weiss-Anteil,
  gefolgt vom gruenen Atmen — signalisiert "Geschafft!"
- **Wechsel zu CONNECT**: Heller gruener Flash (~800ms), dann automatisch zurueck zu IDLE
- **Wechsel zu OFF**: Langsames Ausfaden (~1s)

---

### BRIGHTNESS:\<0-255\> — Helligkeit

```
Senden:   BRIGHTNESS:60    (moderate Helligkeit, Standard)
Senden:   BRIGHTNESS:150   (hell, fuer helle Umgebung)
Senden:   BRIGHTNESS:30    (gedimmt, fuer dunkle Umgebung)
Antwort:  OK
```

- Uebergang ist **sanft** (kein Sprung)
- Wird sofort auf alle LEDs angewendet
- Empfehlung: In Python-Settings speichern, beim Start laden

---

### STATECOLOR:\<zustand\>,\<r\>,\<g\>,\<b\> — Farbe pro Zustand

```
Senden:   STATECOLOR:PROGRESS,0,120,255    (Hellblau fuer Scan)
Senden:   STATECOLOR:DONE,0,255,0          (Gruen fuer Fertig)
Senden:   STATECOLOR:ERROR,255,0,0         (Rot fuer Fehler)
Antwort:  OK
```

Konfigurierbare Zustaende: IDLE, PROGRESS, DONE, WAITING, ERROR, SAVE, CONNECT, DISCONNECTED.
CALIBRATED ist nicht konfigurierbar (immer Regenbogen).

**Default-Farben:**

| Zustand | R | G | B | Farbe |
|---------|---|---|---|-------|
| IDLE | 0 | 0 | 50 | Dunkelblau |
| PROGRESS | 0 | 120 | 255 | Hellblau/Cyan |
| DONE | 0 | 255 | 0 | Gruen |
| WAITING | 255 | 160 | 0 | Gelb/Amber |
| ERROR | 255 | 0 | 0 | Rot |
| SAVE | 255 | 220 | 120 | Weiss/Gold |
| CONNECT | 0 | 255 | 0 | Gruen |
| DISCONNECTED | 255 | 100 | 0 | Orange |

- Farben werden beim Start auf Default gesetzt
- Per Python-Settings beim Verbindungsaufbau konfigurierbar
- Aenderungen wirken sofort (mit Fade-Uebergang)

---

### FLIP:\<0|1\> — LED-Richtung spiegeln

```
Senden:   FLIP:0     (Normal: Links nach rechts)
Senden:   FLIP:1     (Gespiegelt: Rechts nach links)
Antwort:  OK
```

Fuer die Montage der Leiste oben oder unten am Monitor:
- **Unten montiert** (Standard): `FLIP:0` — Fortschritt laeuft links→rechts
- **Oben montiert** (Leiste gedreht): `FLIP:1` — Richtung wird gespiegelt

Alle Animationen werden automatisch gespiegelt (Progress, Breathing, Disconnected-Mitte, etc.).

---

### TIMEOUT:\<sekunden\> — Heartbeat-Watchdog

```
Senden:   TIMEOUT:10     (10 Sekunden Timeout)
Senden:   TIMEOUT:0      (Watchdog deaktivieren)
Antwort:  OK
```

- Maximalwert: 300 Sekunden
- Wenn innerhalb der Timeout-Zeit **kein Befehl** empfangen wird:
  → Firmware wechselt zu DISCONNECTED-Anzeige (oranges Pulsieren, nur mittlere LEDs)
- **Jeder Befehl** (auch HEARTBEAT, PING) setzt den Timer zurueck
- Bei Reconnect wird der vorherige Zustand automatisch wiederhergestellt

**Warum?** Wenn die Python-Software abstuerzt, zeigt die LED-Leiste nicht
ewig einen eingefrorenen Fortschrittsbalken, sondern signalisiert "Verbindung verloren".

---

### HEARTBEAT — Watchdog zuruecksetzen

```
Senden:   HEARTBEAT
Antwort:  OK
```

Dedizierter Befehl zum Zuruecksetzen des Watchdog-Timers.
Alternativ setzt auch jeder andere Befehl den Timer zurueck.

**Typische Verwendung:**
```python
# Heartbeat-Thread (alle 5s bei 10s Timeout)
while running:
    led.heartbeat()
    time.sleep(5)
```

---

### CLEAR — Alles aus

```
Senden:   CLEAR
Antwort:  OK
```

Entspricht `STATE:OFF`. Alle LEDs werden sanft ausgeblendet.

---

## Fehlerbehandlung

### Fehlerantworten

```
ERR:UNKNOWN_CMD       Unbekannter Befehl
ERR:UNKNOWN_STATE     Ungueltiger Zustandsname
ERR:INVALID_ARGS      Fehlende oder ungueltige Parameter
ERR:INVALID_COLOR     Ungueltige Farbwerte (R,G,B erwartet)
```

### Robustheit

- Unbekannte Befehle werden ignoriert (mit Fehlermeldung)
- Leere Zeilen werden ignoriert
- `\r\n` und `\n` werden beide akzeptiert
- Puffergroesse: 64 Zeichen (reicht fuer alle Befehle)
- Werte werden automatisch auf gueltigen Bereich begrenzt (Clamping)

---

## Hardware-Button (GPIO 9)

Der physische Button auf der LED-Leiste kann vom Operator gedrueckt werden:

| Zustand | Button-Aktion |
|---------|---------------|
| DONE | → Wechselt zu IDLE |
| ERROR | → Wechselt zu IDLE |
| WAITING | → Wechselt zu IDLE |
| Andere | Keine Aktion |

Nützlich wenn der Operator direkt an der Leiste steht und den Zustand
quittieren moechte, ohne zum PC zu gehen.

---

## Typischer Kalibrierungs-Workflow

```
Python-Software                    LED-Leiste
──────────────                    ──────────
                                  [Startup: Regenbogen-Sweep]
                                  [Automatisch: IDLE (blaues Atmen)]

PING                    ───────►  PONG
BRIGHTNESS:80           ───────►  OK
TIMEOUT:10              ───────►  OK

--- Operator legt Geraet ein ---

PROGRESS:0              ───────►  OK  [Hellblau: 0%]
PROGRESS:8              ───────►  OK  [Hellblau: 8%]
PROGRESS:25             ───────►  OK  [Hellblau: 25%]
...                                   [Balken fuellt sich sanft]
PROGRESS:100            ───────►  OK  [Hellblau: 100%]

STATE:DONE              ───────►  OK  [Blitz → Gruenes Atmen]

--- Operator sieht gruen, legt naechstes Geraet ein ---

PROGRESS:0              ───────►  OK  [Neuer Scan startet]
PROGRESS:15             ───────►  OK  [Hellblau: 15%]
...

--- Fehler beim Scan ---

STATE:ERROR             ───────►  OK  [Rotes Atmen]

--- Operator geht zum PC, behebt Fehler ---

STATE:IDLE              ───────►  OK  [Blaues Atmen]

--- Eingabe noetig ---

STATE:WAITING           ───────►  OK  [Gelbes Atmen]

--- Operator gibt am PC ein ---

PROGRESS:0              ───────►  OK  [Neuer Scan]
...
PROGRESS:100            ───────►  OK  [Hellblau: 100%]

--- Daten speichern ---

STATE:SAVE              ───────►  OK  [Weiss/Gold Atmen]

--- EEPROM geschrieben ---

STATE:DONE              ───────►  OK  [Blitz → Gruenes Atmen]

--- Alle Frequenzen (F1+F2+F3) kalibriert ---

STATE:CALIBRATED        ───────►  OK  [Regenbogen-Sweep!]

--- Neues Handstueck angeschlossen ---

STATE:CONNECT           ───────►  OK  [Gruener Flash → auto IDLE]
```

---

## Python Initialisierungs-Code

```python
import serial
import serial.tools.list_ports

def find_panda_port():
    """CH340 USB-Serial Adapter finden."""
    for port in serial.tools.list_ports.comports():
        if "ch340" in (port.description or "").lower():
            return port.device
    return None

def init_panda_led(port=None, brightness=60, timeout=10):
    """LED-Leiste initialisieren und verbinden.

    Returns:
        serial.Serial oder None bei Fehler
    """
    if port is None:
        port = find_panda_port()
    if port is None:
        print("Kein Panda Status Geraet gefunden")
        return None

    ser = serial.Serial(port, 115200, timeout=1)
    import time
    time.sleep(0.5)           # ESP32 Reset abwarten
    ser.reset_input_buffer()  # READY-Meldung verwerfen

    # Verbindungstest
    ser.write(b"PING\n")
    if ser.readline().strip() != b"PONG":
        ser.close()
        return None

    # Firmware-Version pruefen
    ser.write(b"INFO\n")
    info = ser.readline().strip().decode()
    if not info.startswith("INFO:PANDA_CUSTOM"):
        ser.close()
        return None

    # Konfiguration
    ser.write(f"BRIGHTNESS:{brightness}\n".encode())
    ser.readline()  # OK

    ser.write(f"TIMEOUT:{timeout}\n".encode())
    ser.readline()  # OK

    ser.write(b"STATE:IDLE\n")
    ser.readline()  # OK

    return ser


# Alternativ: PandaLED-Klasse verwenden (empfohlen)
from panda_led_controller import PandaLED

led = PandaLED(
    brightness=80,
    flip=True,
    colors={"PROGRESS": (0, 120, 255), "DONE": (0, 255, 0)},
    heartbeat_timeout=10
)
# led.set_progress(50)     # Scan-Fortschritt
# led.done()               # Scan fertig
# led.saving()             # EEPROM schreiben
# led.calibrated()         # Komplett kalibriert (Regenbogen!)
# led.connected_flash()    # Handstueck erkannt
# led.waiting()            # Eingabe noetig
# led.error()              # Fehler
```

---

## settings.json Erweiterung

Empfohlene Eintraege fuer die Kalibriersoftware:

```json
{
    "led_settings": {
        "enabled": true,
        "port": "auto",
        "brightness": 60,
        "flip": false,
        "heartbeat_timeout": 10,
        "colors": {
            "PROGRESS": [0, 120, 255],
            "DONE": [0, 255, 0],
            "WAITING": [255, 160, 0],
            "ERROR": [255, 0, 0],
            "IDLE": [0, 0, 50],
            "SAVE": [255, 220, 120]
        }
    }
}
```

| Feld | Typ | Standard | Beschreibung |
|------|-----|----------|-------------|
| enabled | bool | true | LED-Leiste aktivieren |
| port | string | "auto" | COM-Port oder "auto" fuer CH340-Erkennung |
| brightness | int | 60 | Helligkeit 0-255 |
| flip | bool | false | LED-Richtung spiegeln (Montage oben) |
| heartbeat_timeout | int | 10 | Heartbeat-Timeout in Sekunden, 0=aus |
| colors | object | {} | Farben pro Zustand als [R, G, B] Arrays |

---

## Technische Details

### Timing

- Display-Refresh: 60fps (16ms pro Frame)
- Fade-Dauer Standard: ~500ms (8% pro Frame)
- Fade-Dauer langsam: ~1s (3% pro Frame, fuer OFF/DISCONNECTED)
- Fade-Dauer schnell: ~200ms (20% pro Frame, fuer Flash-Effekt)
- Fortschritts-Interpolation: 12% pro Frame

### Speicherverbrauch

- RAM: ~4.3% (14KB von 320KB)
- Flash: ~20.9% (274KB von 1310KB)
- Display-Puffer: 600 Bytes (25 LEDs x 6 Floats x 4 Bytes)

### Hardware

| Parameter | Wert |
|-----------|------|
| MCU | ESP32-C3-MINI (RISC-V, 160MHz) |
| LED Data Pin | GPIO 5 |
| LED Typ | WS2812 (GRB, 800kHz) |
| Anzahl LEDs | 25 |
| Button | GPIO 9 (aktiv LOW, interner Pullup) |
| USB-Serial | CH340K |
| Stromversorgung | USB-C, 5V 3A |
