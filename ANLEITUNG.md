# Panda Status - Custom Firmware Anleitung

## Hardware-Details (durch Firmware Reverse Engineering ermittelt)

| Parameter       | Wert                    |
|-----------------|-------------------------|
| MCU             | ESP32-C3-MINI (RISC-V)  |
| LED Data Pin    | **GPIO 5**              |
| LED Typ         | WS2812 (GRB, 800kHz)   |
| Anzahl LEDs     | **25**                  |
| Button          | GPIO 9 (aktiv LOW)      |
| USB-Serial      | CH340K                  |
| Stromversorgung | USB-C, 5V 3A            |

---

## Schritt 1: Original-Firmware sichern

**Wichtig: Immer zuerst ein Backup machen!**

1. Python und pip installieren (falls nicht vorhanden)
2. esptool installieren:
   ```
   pip install esptool
   ```
3. Panda Status per USB-C anschliessen (COM15)
4. Original-Firmware auslesen:
   ```
   esptool.py --chip esp32c3 --port COM15 --baud 460800 read_flash 0x0 0x400000 archiv/backup_original_firmware.bin
   ```

Falls der Befehl "Failed to connect" meldet: siehe Schritt 2 (Bootloader-Modus).

---

## Schritt 2: Bootloader-Modus aktivieren

Der ESP32-C3 muss in den Bootloader-Modus versetzt werden zum Flashen:

### Methode A: Button + USB
1. USB-Kabel vom Panda Status **trennen**
2. **Button auf der Platine gedrueckt halten** (GPIO 9 = BOOT)
3. USB-Kabel **einstecken** (waehrend Button gedrueckt)
4. Button **loslassen** nach 1 Sekunde
5. Der ESP32 ist jetzt im Bootloader-Modus

### Methode B: Automatisch (meistens)
Viele CH340-basierte Boards unterstuetzen automatisches DTR/RTS-Signaling.
In dem Fall reicht ein normaler Flash-Befehl - esptool setzt den Chip
automatisch in den Bootloader.

### Kontrolle
```
esptool.py --chip esp32c3 --port COM15 chip_id
```
Sollte die Chip-ID anzeigen (z.B. "ESP32-C3 (QFN32)").

---

## Schritt 3: Entwicklungsumgebung einrichten

### Option A: PlatformIO (empfohlen)

1. **VS Code** installieren (falls nicht vorhanden)
2. **PlatformIO Extension** in VS Code installieren
3. Projekt oeffnen:
   - VS Code oeffnen
   - Datei > Ordner oeffnen > `J:\Liquidbeam\10_WELLCOMET\Status LED Leiste Pandastatus\firmware`
4. PlatformIO installiert automatisch:
   - ESP32-C3 Toolchain
   - Arduino Framework
   - Adafruit NeoPixel Bibliothek

### Option B: Arduino IDE

1. **Arduino IDE 2.x** installieren
2. ESP32 Board-Support hinzufuegen:
   - Datei > Einstellungen > Zusaetzliche Boardverwalter-URLs:
     `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
   - Werkzeuge > Board > Boardverwalter > "esp32" suchen und installieren
3. Board auswaehlen:
   - Werkzeuge > Board > ESP32C3 Dev Module
4. Bibliothek installieren:
   - Werkzeuge > Bibliotheken verwalten > "Adafruit NeoPixel" installieren
5. Sketch oeffnen:
   - `firmware/panda_custom/panda_custom.ino`

---

## Schritt 4: Firmware flashen

### Mit PlatformIO
1. In VS Code das PlatformIO-Projekt oeffnen
2. COM-Port in `platformio.ini` anpassen (falls nicht COM15)
3. Klick auf den Upload-Pfeil (oder `pio run -t upload` im Terminal)

### Mit Arduino IDE
1. Port auswaehlen: Werkzeuge > Port > COM15
2. Board-Einstellungen:
   - Board: ESP32C3 Dev Module
   - Upload Speed: 460800
   - Flash Mode: DIO
   - Flash Size: 4MB
3. Hochladen-Button klicken (Pfeil nach rechts)

### Mit esptool (nur kompilierte .bin)
```
esptool.py --chip esp32c3 --port COM15 --baud 460800 \
  write_flash -z 0x0 firmware.bin
```

### Ueberpruefen
Nach dem Flashen:
1. Seriellen Monitor oeffnen (115200 Baud)
2. Beim Start erscheint: `READY:PANDA_CUSTOM,25,V1.0`
3. `PING` eingeben -> Antwort: `PONG`
4. Regenbogen-Animation beim Start = Firmware laeuft korrekt

---

## Schritt 5: Python-Controller testen

1. pyserial installieren:
   ```
   pip install pyserial
   ```

2. Test ausfuehren:
   ```
   python "J:\Liquidbeam\10_WELLCOMET\Status LED Leiste Pandastatus\python\panda_led_controller.py"
   ```

3. Der Test zeigt nacheinander:
   - Alle Animationszustaende (je 3 Sekunden)
   - Fortschrittsbalken 0-100%
   - Einzelne LEDs in RGB

---

## Schritt 6: In Kalibriersoftware integrieren

Siehe `python/integration_beispiel.py` fuer die konkreten Code-Aenderungen.

Kurzfassung der noetige Aenderungen in `main.py`:

```python
# 1. Import (am Anfang)
sys.path.insert(0, r"J:\Liquidbeam\10_WELLCOMET\Status LED Leiste Pandastatus\python")
from panda_led_controller import PandaLED

# 2. Init (in __init__)
self.panda_led = PandaLED()

# 3. Scan-Fortschritt (in _on_scan_progress)
if self.panda_led and self.panda_led.connected:
    self.panda_led.set_progress(int(current / total * 100))

# 4. Scan-Start (in start_frequency_scan)
if self.panda_led and self.panda_led.connected:
    self.panda_led.set_state("SCANNING")

# 5. Fertig / Fehler (in _on_scan_finished / _on_scan_error)
self.panda_led.set_state("DONE")   # oder "ERROR"
```

---

## Serielles Protokoll - Referenz

| Befehl                  | Beschreibung                     | Antwort |
|-------------------------|----------------------------------|---------|
| `PING`                  | Verbindungstest                  | `PONG`  |
| `INFO`                  | Geraeteinfo                      | `INFO:PANDA_CUSTOM,25,V1.0` |
| `PROGRESS:<0-100>`      | Fortschrittsbalken               | `OK`    |
| `STATE:IDLE`            | Blaues Atmen                     | `OK`    |
| `STATE:SCANNING`        | Laufender Punkt                  | `OK`    |
| `STATE:ERROR`           | Rotes Blinken                    | `OK`    |
| `STATE:DONE`            | Gruenes Leuchten                 | `OK`    |
| `STATE:CALIBRATING`     | Orange Welle                     | `OK`    |
| `STATE:BUSY`            | Lila Rotation                    | `OK`    |
| `COLOR:<r>,<g>,<b>`     | Basisfarbe setzen                | `OK`    |
| `BRIGHTNESS:<0-255>`    | Helligkeit                       | `OK`    |
| `SOLID:<r>,<g>,<b>`     | Alle LEDs eine Farbe             | `OK`    |
| `LED:<nr>,<r>,<g>,<b>`  | Einzelne LED (0-24)              | `OK`    |
| `CLEAR`                 | Alle LEDs aus                    | `OK`    |

---

## Original-Firmware wiederherstellen

Falls noetig, kann die Original-Firmware zurueckgeflasht werden:

```
esptool.py --chip esp32c3 --port COM15 --baud 460800 \
  write_flash 0x0 archiv/backup_original_firmware.bin
```

Oder die BTT-Firmware von GitHub:
```
esptool.py --chip esp32c3 --port COM15 --baud 460800 \
  write_flash 0x0 panda_status_v1.0.2.bin
```

---

## Troubleshooting

| Problem | Loesung |
|---------|---------|
| "Failed to connect" | Bootloader-Modus aktivieren (Schritt 2) |
| Kein COM-Port sichtbar | CH340 Treiber installieren: [CH340 Driver](https://www.wch-ic.com/downloads/CH341SER_EXE.html) |
| LEDs leuchten nicht | GPIO-Pin pruefen (muss 5 sein), Stromversorgung pruefen |
| Python findet Port nicht | `python -m serial.tools.list_ports` zum Auflisten aller Ports |
| Falsche Farbreihenfolge | NEO_GRB in der Firmware pruefen (Standard fuer WS2812) |
