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
├── README.md
├── CLAUDE.md
├── LICENSE
├── config.json.example
├── firmware/
│   ├── platformio.ini
│   └── src/
│       └── main.cpp
├── python/
│   ├── claude_rider.py
│   └── test_claude_rider.py
├── start_claude_rider.bat
└── stop_claude_rider.bat
```

## Serielles Protokoll (115200 Baud)

```
PING                           -> PONG
INFO                           -> INFO:CLAUDE_RIDER,25,V3.0
STATE:<zustand>                -> OK
PROGRESS:<0-100>               -> OK
BRIGHTNESS:<0-255>             -> OK
SPEED:<1-5>                    -> OK
STATECOLOR:<st>,<r>,<g>,<b>   -> OK
FLIP:<0|1>                     -> OK
TIMEOUT:<sekunden>             -> OK
HEARTBEAT                      -> OK
CLEAR                          -> OK
```

## LED-Zustaende

| Zustand      | Farbe      | Animation               | Bedeutung               |
|--------------|------------|-------------------------|-------------------------|
| KNIGHT_RIDER | Rot        | KITT Scanner (aggressiv) | Claude denkt / arbeitet |
| IDLE         | Blau       | Sanftes Atmen           | System bereit           |
| WAITING      | Amber/Gelb | Pulsieren               | Input noetig            |
| ERROR        | Rot        | Schnelles Atmen         | Fehler aufgetreten      |
| DONE         | Gruen      | Flash + Atmen           | Task abgeschlossen      |
| PROGRESS     | Cyan       | Fortschrittsbalken      | Langer Task             |

## Konventionen
- Framework: Arduino + Adafruit NeoPixel (Firmware)
- Python: pyserial, keine weiteren Abhaengigkeiten
- Tests: pytest
