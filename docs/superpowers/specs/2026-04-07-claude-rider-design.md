# Claude Rider — Design Spec

**Datum:** 2026-04-07
**Status:** Entwurf
**Autor:** Claude Rider Team

## Zusammenfassung

Claude Rider macht den Status von Claude Code physisch sichtbar: eine LED-Leiste
zeigt durch Animationen an, ob Claude denkt (Knight Rider Scanner), auf Input wartet,
einen Fehler gefunden hat oder eine Aufgabe abgeschlossen hat.

Das Projekt verwandelt die BIGTREETECH Panda Status LED-Leiste in eine
Knight-Rider-inspirierte Statusanzeige fuer Entwickler, gesteuert ueber
Claude Code Hooks.

**Ziel:** Open Source Projekt mit klarem "Kauf die Leiste, flash, starte Bat, fertig"-Erlebnis.

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

## Architektur

Drei Schichten, lose gekoppelt ueber Serial:

```
Claude Code (Hooks)
    │ Shell: python claude_rider.py --event <event>
    ▼
Claude Rider Python Controller (Daemon)
    │ USB-Serial 115200 Baud
    ▼
Claude Rider Firmware (ESP32-C3, 25x WS2812)
```

### Datenfluss

1. Claude Code feuert Hook (z.B. `PreToolUse`)
2. Hook ruft `python claude_rider.py --event tool_pending` auf
3. CLI erkennt laufenden Daemon, sendet Event dorthin
4. Daemon liest `config.json`, mappt Event auf Firmware-Befehl
5. Daemon sendet Befehl ueber Serial an ESP32
6. Firmware fadet sanft zur neuen Animation (Doppelpuffer, 60fps)

## Firmware (ESP32-C3)

### Basis

Neugeschriebene Firmware basierend auf dem bewaehrten Doppelpuffer-Fade-System.
Alle Animationen mit sanften Uebergaengen via Float-Interpolation bei 60fps.

- Firmware-ID: `CLAUDE_RIDER`
- Version: `V3.0`
- Framework: Arduino + Adafruit NeoPixel
- Build: PlatformIO

### LED-Zustaende

| State           | Animation                          | Verwendung                    |
|-----------------|------------------------------------|-------------------------------|
| KNIGHT_RIDER    | Classic KITT Scanner (aggressiv)   | Claude denkt / arbeitet       |
| IDLE            | Sanftes blaues Atmen               | System bereit                 |
| PROGRESS        | Fortschrittsbalken                 | Langer Task (Build, Tests)    |
| DONE            | Gruener Flash + Atmen              | Task erfolgreich              |
| WAITING         | Amber/Gelb Pulsieren               | Tool braucht Genehmigung      |
| ERROR           | Rotes Atmen (schnell)              | Fehler aufgetreten            |
| SAVE            | Weiss/Gold Atmen                   | Frei belegbar via Config      |
| CONNECT         | Gruener Flash → auto IDLE          | Session gestartet             |
| DISCONNECTED    | Orange, Mitte pulsiert             | Heartbeat-Timeout             |
| OFF             | Dunkel                             | Alles aus                     |

### Knight Rider Animation (ST_KNIGHT_RIDER)

- Classic KITT: Ein roter Punkt wandert hin und her mit langem, verblassendem Schweif
- Aggressiv schnelle Geschwindigkeit (~3x klassisch)
- Schweif-Laenge: 8 LEDs mit exponentieller Abnahme (pow 2.5)
- Farbe konfigurierbar via STATECOLOR-Befehl
- Geschwindigkeit konfigurierbar via SPEED-Befehl (1-5)

### Serielles Protokoll (115200 Baud)

Kompatibel mit V2.1, erweitert um:

```
# Bestehende Befehle (unveraendert):
PING                           → PONG
INFO                           → INFO:CLAUDE_RIDER,25,V3.0
PROGRESS:<0-100>               → OK
STATE:<zustand>                → OK    (alle bisherigen + KNIGHT_RIDER)
BRIGHTNESS:<0-255>             → OK
STATECOLOR:<st>,<r>,<g>,<b>   → OK
FLIP:<0|1>                     → OK
TIMEOUT:<sekunden>             → OK
HEARTBEAT                      → OK
CLEAR                          → OK

# Neue Befehle:
SPEED:<1-5>                    → OK    Knight Rider Scan-Geschwindigkeit
```

### Startup-Sequenz

Boot → Knight Rider Scan (einmal hin und her) → IDLE
Anstatt des bisherigen Regenbogen-Sweeps.
Sendet `READY:CLAUDE_RIDER,25,V3.0` nach Boot.

## Python Controller

### Daemon-Architektur

Der Controller laeuft als Hintergrund-Daemon und haelt die Serial-Verbindung offen.
CLI-Aufrufe aus den Hooks kommunizieren mit dem Daemon ueber ein lokales Socket.

```
claude_rider.py --daemon        Startet Daemon (Hintergrund)
claude_rider.py --event <name>  Sendet Event an Daemon
claude_rider.py --stop          Faehrt Daemon herunter
claude_rider.py --status        Zeigt ob Daemon laeuft + COM-Port
```

### Port-Erkennung (Protokoll-basiert)

Kein Suchen nach "CH340" im Geraete-Namen. Stattdessen:

1. Alle verfuegbaren COM-Ports auflisten
2. Jeden Port oeffnen (115200 Baud, kurzer Timeout)
3. `PING\n` senden
4. Wer mit `PONG` antwortet → Kandidat
5. `INFO\n` senden → muss mit `INFO:CLAUDE_RIDER` beginnen
6. Erster Treffer wird verwendet

Fallback: Expliziter Port in `config.json`.

### Abhaengigkeiten

- Python 3.8+
- `pyserial` (einzige externe Abhaengigkeit)

## Config (config.json)

Wird beim ersten Start mit Defaults erstellt. Alle Felder optional —
fehlende Felder nehmen Default-Werte.

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
    "session_start":    { "action": "STATE:CONNECT" },
    "thinking":         { "action": "STATE:KNIGHT_RIDER" },
    "tool_pending":     { "action": "STATE:WAITING" },
    "tool_running":     { "action": "STATE:KNIGHT_RIDER" },
    "error":            { "action": "STATE:ERROR" },
    "task_done":        { "action": "STATE:DONE" },
    "idle":             { "action": "STATE:IDLE" },
    "progress":         { "action": "PROGRESS:{value}" }
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

## Claude Code Hook Integration

Validiert gegen aktuelle Claude Code Dokumentation (April 2026).

### Hook-Event-Mapping

| Claude Code Hook            | Matcher            | Claude Rider Event | LED-Animation              |
|-----------------------------|--------------------|--------------------|----------------------------|
| `SessionStart`              |                    | session_start      | Connect Flash → IDLE       |
| `UserPromptSubmit`          |                    | thinking           | Knight Rider Scan          |
| `PreToolUse`                |                    | tool_pending       | Amber Pulsieren            |
| `PostToolUse`               |                    | tool_running       | Knight Rider Scan          |
| `PostToolUseFailure`        |                    | error              | Rotes Atmen                |
| `Stop`                      |                    | task_done          | Gruener Flash → IDLE       |
| `Notification`              | `idle_prompt`      | idle               | Blaues Atmen               |
| `Notification`              | `permission_prompt`| waiting            | Amber Pulsieren            |

### Hook-Format in settings.json

Hooks nutzen ein verschachteltes Format mit `hooks`-Array:

```json
{
  "hooks": {
    "UserPromptSubmit": [
      {
        "hooks": [
          {
            "type": "command",
            "command": "python claude_rider.py --event thinking",
            "timeout": 3
          }
        ]
      }
    ],
    "Stop": [
      {
        "hooks": [
          {
            "type": "command",
            "command": "python claude_rider.py --event task_done",
            "timeout": 3
          }
        ]
      }
    ],
    "Notification": [
      {
        "matcher": "idle_prompt",
        "hooks": [
          {
            "type": "command",
            "command": "python claude_rider.py --event idle",
            "timeout": 3
          }
        ]
      },
      {
        "matcher": "permission_prompt",
        "hooks": [
          {
            "type": "command",
            "command": "python claude_rider.py --event waiting",
            "timeout": 3
          }
        ]
      }
    ],
    "PostToolUseFailure": [
      {
        "hooks": [
          {
            "type": "command",
            "command": "python claude_rider.py --event error",
            "timeout": 3
          }
        ]
      }
    ]
  }
}
```

### Hook-Payload

Jeder Hook erhaelt JSON auf stdin:
```json
{
  "session_id": "abc123",
  "cwd": "/project",
  "hook_event_name": "PreToolUse",
  "tool_name": "Bash",
  "tool_input": { "command": "npm test" }
}
```

Hooks muessen Exit Code 0 zurueckgeben (sonst wird die Aktion blockiert bei Code 2).

### Konfigurationsort

Hooks koennen global (`~/.claude/settings.json`) oder per-Projekt
(`.claude/settings.json`) konfiguriert werden. Fuer Claude Rider
empfohlen: global, da die LED-Leiste projektuebergreifend genutzt wird.

## Batch-Scripts (Windows)

### start_claude_rider.bat

1. Prueft ob Python installiert ist
2. Installiert `pyserial` falls noetig (`pip install pyserial`)
3. Startet den Daemon (`claude_rider.py --daemon`)
4. Daemon erkennt COM-Port automatisch (PING/PONG Protokoll)
5. Registriert Claude Code Hooks falls noch nicht vorhanden
6. Zeigt Status: "Claude Rider laeuft auf COM7"

### stop_claude_rider.bat

1. Sendet Stop-Signal an Daemon (`claude_rider.py --stop`)
2. Daemon faehrt LEDs sanft runter (STATE:OFF)
3. Schliesst Serial-Verbindung
4. Zeigt "Claude Rider gestoppt"

## Datenschutz / Open Source Bereinigung

Das Projekt wird komplett neu aufgesetzt — keine Kundendaten:

**Entfernt:**
- Alle Referenzen zu Panda, Wellcomet, Kalibrier-Operator, Handstueck
- Kundenspezifische COM-Port Nummern
- Kundenspezifische Dateipfade
- Original BTT Firmware Binary
- Integrations-Beispiel (kundenspezifisch)
- Archiv-Ordner
- Alte Git-History

**Neu geschrieben:**
- Firmware als "Claude Rider" ohne Vorgeschichte
- Python Controller als generischer `claude_rider.py`
- README.md mit Setup-Anleitung
- CLAUDE.md fuer das neue Projekt

**Beibehalten (nur generische Technik):**
- Doppelpuffer-Fade-System (generischer Algorithmus)
- Animations-Mathematik (Atmen, Progress-Balken, etc.)
- Hardware-Pinning (GPIO 5, 25 LEDs — oeffentliche Hardware-Specs)

## Projektstruktur (Ziel)

```
claude-rider/
├── README.md                    # Setup-Anleitung, Screenshots
├── CLAUDE.md                    # Projekt-Kontext fuer Claude Code
├── LICENSE                      # Open Source Lizenz (MIT)
├── config.json.example          # Beispiel-Konfiguration
├── firmware/
│   ├── platformio.ini           # Build-Konfiguration
│   └── src/
│       └── main.cpp             # Claude Rider Firmware V3.0
├── python/
│   └── claude_rider.py          # Controller + Daemon + CLI
├── start_claude_rider.bat       # Windows: Alles starten
└── stop_claude_rider.bat        # Windows: Alles stoppen
```

## Offene Punkte

- Daemon-Kommunikation: Localhost TCP Socket (Port 17177) — einfachste Cross-Platform Loesung
- Lizenz-Wahl (MIT vorgeschlagen)
