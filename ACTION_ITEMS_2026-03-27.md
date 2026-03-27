# ACTION ITEMS — Panda Status LED Firmware

Datum: 2026-03-27

## Status Firmware V2.0

- [x] Fade-System (Doppelpuffer mit Interpolation, 60fps)
- [x] Zustaende: IDLE, PROGRESS, DONE, WAITING, ERROR, OFF, DISCONNECTED
- [x] Heartbeat-Watchdog
- [x] Flash-Effekt bei DONE
- [x] Python Controller V2.0
- [x] Protokoll-Dokument (PROTOKOLL_V2.md)
- [ ] V2.0 auf ESP32-C3 flashen und testen

---

## Firmware V2.1 — Neue Zustaende (implementiert)

### 1. STATE:SAVE — EEPROM wird geschrieben
- [x] `ST_SAVE` zum State-Enum hinzugefuegt
- [x] `computeSaveTargets()` — Weiss/Gold Atmen (~2s Periode)
- [x] Python: `led.saving()`

### 2. STATE:CALIBRATED — Alle Frequenzen kalibriert (Belohnung)
- [x] `ST_CALIBRATED` zum State-Enum hinzugefuegt
- [x] `computeCalibratedTargets()` — Regenbogen-Sweep (~3s/Durchlauf)
- [x] Python: `led.calibrated()`

### 3. STATE:CONNECT — Handstueck erkannt
- [x] `ST_CONNECT` zum State-Enum hinzugefuegt
- [x] `computeConnectTargets()` — Gruener Flash (~800ms) → auto IDLE
- [x] Python: `led.connected_flash()`

---

## Firmware V2.1 — Konfigurierbare Farben & Flip (implementiert)

- [x] State-Farben-Arrays (stR/stG/stB) mit Defaults
- [x] `computeBreathingForState()` Helper — alle Atmen-Animationen nutzen State-Farben
- [x] `STATECOLOR:<state>,<r>,<g>,<b>` Befehl
- [x] `FLIP:<0|1>` Befehl (Montage oben/unten am Monitor)
- [x] Progress-Balken Default auf Hellblau/Cyan (0,120,255) geaendert
- [x] Python: `led.set_state_color()`, `led.set_flip()`, `colors`-Parameter im Konstruktor
- [x] Protokoll-Dokument und CLAUDE.md aktualisiert

## Naechste Schritte

- [ ] V2.1 auf ESP32-C3 flashen und alle Zustaende testen
- [ ] Farben im Serial Monitor testen (z.B. `STATECOLOR:PROGRESS,255,0,0`)
- [ ] Integration in Kalibriersoftware (main.py)
- [ ] `led_settings` Block in settings.json der Kalibriersoftware anlegen

## Integration in Kalibriersoftware

```python
# Handstueck-Erkennung
handpiece_connected.connect(lambda: led.connected_flash())

# Scan-Workflow
scan_progress.connect(lambda cur, total, _:
    led.set_progress(int(cur / total * 100)))
scan_finished.connect(lambda _: led.done())
scan_error.connect(lambda _: led.error())

# EEPROM speichern
before_eeprom_write.connect(lambda: led.saving())
after_eeprom_write.connect(lambda: led.done())

# Alle Frequenzen kalibriert
all_frequencies_done.connect(lambda: led.calibrated())
```
