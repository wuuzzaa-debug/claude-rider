# Panda Status Custom LED Controller

## Projekt
Custom Firmware (V2.1) fuer die BIGTREETECH Panda Status LED-Leiste.
Fernstatus-Anzeige fuer Kalibrier-Operator — gesteuert ueber USB-Serial
von der Wellcomet Kalibriersoftware (Python/PyQt5).

Alle Animationen mit sanften Fades (Doppelpuffer mit Interpolation).

## Hardware (durch Firmware Reverse Engineering ermittelt)

| Parameter       | Wert                          |
|-----------------|-------------------------------|
| MCU             | ESP32-C3-MINI (RISC-V)        |
| LED Data Pin    | **GPIO 5** (ueber RMT)        |
| LED Typ         | WS2812 (GRB, 800kHz)          |
| Anzahl LEDs     | **25**                         |
| Button          | **GPIO 9** (aktiv LOW, Pullup) |
| USB-Serial      | CH340K                         |
| Audio Codec     | ES8311 (I2C/I2S, Mikrofon)     |
| Stromversorgung | USB-C, 5V 3A                   |

## Projektstruktur

```
Status LED Leiste Pandastatus/
├── CLAUDE.md                              # Diese Datei
├── PROTOKOLL_V2.md                        # Detailliertes Protokoll-Dokument
├── ANLEITUNG.md                           # Schritt-fuer-Schritt Anleitung
├── panda_status_v1.0.2.bin                # Original BTT Firmware (Backup)
├── firmware/
│   ├── platformio.ini                     # PlatformIO Build-Konfiguration
│   ├── src/
│   │   └── main.cpp                       # Custom Firmware V2.1
│   └── panda_custom/
│       └── panda_custom.ino               # V1.0 (veraltet, Backup)
├── python/
│   ├── panda_led_controller.py            # Python LED Controller V2.1
│   └── integration_beispiel.py            # Integration in Kalibriersoftware
└── archiv/                                # Backups (V1 Firmware + Controller)
```

## Status
- Firmware V2.1 mit Fade-System (Doppelpuffer-Interpolation, 60fps)
- 10 Zustaende inkl. SAVE, CALIBRATED, CONNECT
- Konfigurierbare Farben pro Zustand (STATECOLOR-Befehl)
- LED-Richtung spiegelbar (FLIP) fuer Montage oben/unten
- Heartbeat-Watchdog fuer Verbindungsueberwachung
- V1.0 erfolgreich gebaut und geflasht (PlatformIO, ESP32-C3)
- Build V1: 4.3% RAM, 20.9% Flash

## Serielles Protokoll V2.1 (115200 Baud)

```
PING                           -> PONG
INFO                           -> INFO:PANDA_CUSTOM,25,V2.1
PROGRESS:<0-100>               -> OK    Fortschrittsbalken (Default: Hellblau)
STATE:<zustand>                -> OK    IDLE|DONE|WAITING|ERROR|SAVE|CALIBRATED|CONNECT|OFF
BRIGHTNESS:<0-255>             -> OK    Helligkeit (sanfter Uebergang)
STATECOLOR:<st>,<r>,<g>,<b>   -> OK    Farbe pro Zustand setzen
FLIP:<0|1>                     -> OK    LED-Richtung spiegeln
TIMEOUT:<sekunden>             -> OK    Heartbeat-Timeout (0=aus)
HEARTBEAT                      -> OK    Watchdog zuruecksetzen
CLEAR                          -> OK    Alles aus
```

Detaillierte Protokoll-Dokumentation: siehe `PROTOKOLL_V2.md`

## LED-Zustaende (Operator-Sicht)

| Zustand | Farbe | Animation | Bedeutung |
|---------|-------|-----------|-----------|
| IDLE | Blau | Atmen (gedimmt) | System bereit |
| PROGRESS | Hellblau/Cyan | Balken fuellt sich | Scan laeuft |
| DONE | Gruen | Atmen (hell) | Fertig, naechstes Geraet |
| WAITING | Gelb/Amber | Atmen | Eingabe am PC noetig |
| ERROR | Rot | Atmen (schnell) | Fehler aufgetreten |
| SAVE | Weiss/Gold | Atmen | EEPROM wird geschrieben |
| CALIBRATED | Regenbogen | Sweep | Komplett kalibriert! |
| CONNECT | Gruen | Flash → auto IDLE | Handstueck erkannt |
| DISCONNECTED | Orange | Mitte pulsiert | Verbindung verloren |

## Flashen

```bash
# Bootloader-Modus: Button (GPIO9) halten + USB einstecken
# Flash mit PlatformIO: pio run -t upload
```

## Verwandte Projekte

- Kalibriersoftware: `J:\Liquidbeam\10_WELLCOMET\Labview Kallibriersoftware Python\Python_Calibration_Software\`
  - Hauptdatei: `main.py` (PyQt5, 7060 Zeilen)
  - Signal `scan_progress(current, total, text)` fuer LED-Fortschritt nutzen
  - Signal `scan_finished(dict)` / `scan_error(str)` fuer Zustandswechsel
  - Settings: `settings.json` — `led_settings` Block fuer Helligkeit/Port/Flip/Farben

## Konventionen
- Sprache: Deutsch (Code-Kommentare duerfen Englisch sein)
- Framework: Arduino + Adafruit NeoPixel (Custom Firmware)
- Python: pyserial fuer Kommunikation
- Immer Backup vor Firmware-Flash
