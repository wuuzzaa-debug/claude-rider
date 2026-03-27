"""
Panda Status LED Controller V2.1
=================================
Steuert die Custom-Firmware V2.1 auf der Panda Status LED-Leiste
ueber serielle Verbindung (CH340K USB-Serial).

Fernstatus-Anzeige fuer Kalibrier-Operator:
  - Gruener Fortschrittsbalken waehrend Scan
  - Gruenes Atmen wenn fertig (naechstes Geraet einlegen)
  - Gelbes Atmen wenn Eingabe noetig
  - Rotes Atmen bei Fehler
  - Weiss/Gold Atmen beim Speichern (EEPROM)
  - Regenbogen-Sweep wenn komplett kalibriert
  - Gruener Flash bei Handstueck-Erkennung

Verwendung:
    from panda_led_controller import PandaLED

    led = PandaLED()                  # Auto-Erkennung CH340
    led.set_brightness(80)            # Helligkeit aus Settings
    led.set_progress(50)              # Fortschrittsbalken 50%
    led.done()                        # Scan fertig
    led.saving()                      # EEPROM wird geschrieben
    led.calibrated()                  # Komplett kalibriert (Belohnung!)
    led.connected_flash()             # Handstueck erkannt
    led.waiting()                     # Eingabe noetig
    led.error()                       # Fehler
    led.idle()                        # Zurueck zu Standby
    led.close()

Integration mit Kalibriersoftware:
    scan_progress.connect(lambda cur, total, _:
        led.set_progress(int(cur / total * 100)))
    scan_finished.connect(lambda _: led.done())
    scan_error.connect(lambda _: led.error())
    handpiece_connected.connect(lambda: led.connected_flash())
    all_frequencies_done.connect(lambda: led.calibrated())
"""

import serial
import serial.tools.list_ports
import time
import threading
from typing import Optional


class PandaLED:
    """Controller fuer die Panda Status LED-Leiste V2.0."""

    NUM_LEDS = 25
    BAUD_RATE = 115200
    TIMEOUT = 1.0
    FW_PREFIX = "INFO:PANDA_CUSTOM"

    def __init__(self, port: Optional[str] = None, brightness: int = 60,
                 heartbeat_timeout: int = 10, flip: bool = False,
                 colors: Optional[dict] = None, auto_connect: bool = True):
        self._serial: Optional[serial.Serial] = None
        self._lock = threading.Lock()
        self._connected = False
        self._port = port
        self._brightness = brightness
        self._heartbeat_timeout = heartbeat_timeout
        self._flip = flip
        self._colors = colors or {}  # z.B. {"PROGRESS": (0,120,255)}
        self._heartbeat_thread: Optional[threading.Thread] = None
        self._heartbeat_running = False
        self._fw_version = ""

        if auto_connect:
            self.connect(port)

    # ================================================================
    #                        VERBINDUNG
    # ================================================================

    def connect(self, port: Optional[str] = None) -> bool:
        """Verbindung zur LED-Leiste herstellen und initialisieren."""
        if port is None:
            port = self._find_port()
        if port is None:
            print("[PandaLED] Kein Panda Status Geraet gefunden.")
            return False

        try:
            self._serial = serial.Serial(
                port=port,
                baudrate=self.BAUD_RATE,
                timeout=self.TIMEOUT,
                write_timeout=self.TIMEOUT
            )
            self._port = port
            time.sleep(0.5)  # ESP32 Reset abwarten
            self._serial.reset_input_buffer()

            # Verbindungstest
            if not self._ping():
                print(f"[PandaLED] Geraet auf {port} antwortet nicht.")
                self._serial.close()
                return False

            # Firmware-Version pruefen
            info = self._send("INFO")
            if not info.startswith(self.FW_PREFIX):
                print(f"[PandaLED] Unbekanntes Geraet: {info}")
                self._serial.close()
                return False

            self._fw_version = info.split(",")[-1] if "," in info else ""
            self._connected = True
            print(f"[PandaLED] Verbunden auf {port} ({info})")

            # Konfiguration anwenden
            self._send(f"BRIGHTNESS:{self._brightness}")
            if self._flip:
                self._send("FLIP:1")
            for state, rgb in self._colors.items():
                r, g, b = rgb
                self._send(f"STATECOLOR:{state.upper()},{r},{g},{b}")
            if self._heartbeat_timeout > 0:
                self._send(f"TIMEOUT:{self._heartbeat_timeout}")
                self._start_heartbeat()
            self._send("STATE:IDLE")

            return True

        except serial.SerialException as e:
            print(f"[PandaLED] Verbindungsfehler: {e}")
            return False

    def _find_port(self) -> Optional[str]:
        """CH340 USB-Serial Adapter automatisch finden."""
        for port_info in serial.tools.list_ports.comports():
            desc = (port_info.description or "").lower()
            hwid = (port_info.hwid or "").lower()
            if "ch340" in desc or "ch340" in hwid:
                print(f"[PandaLED] CH340 gefunden: {port_info.device}")
                return port_info.device
        return None

    def _send(self, command: str) -> str:
        """Befehl senden und Antwort lesen (thread-safe)."""
        if not self._serial or not self._serial.is_open:
            return "ERR:NOT_CONNECTED"

        with self._lock:
            try:
                self._serial.reset_input_buffer()
                self._serial.write(f"{command}\n".encode('ascii'))
                self._serial.flush()
                response = self._serial.readline().decode('ascii').strip()
                return response
            except (serial.SerialException, UnicodeDecodeError) as e:
                return f"ERR:{e}"

    def _ping(self) -> bool:
        """Verbindungstest."""
        return self._send("PING") == "PONG"

    @property
    def connected(self) -> bool:
        """True wenn verbunden und erreichbar."""
        return (self._connected and
                self._serial is not None and
                self._serial.is_open)

    @property
    def firmware_version(self) -> str:
        """Firmware-Versionsstring (z.B. 'V2.0')."""
        return self._fw_version

    # ================================================================
    #                    HEARTBEAT-WATCHDOG
    # ================================================================

    def _start_heartbeat(self):
        """Heartbeat-Thread starten."""
        if self._heartbeat_running:
            return
        self._heartbeat_running = True
        self._heartbeat_thread = threading.Thread(
            target=self._heartbeat_loop, daemon=True
        )
        self._heartbeat_thread.start()

    def _stop_heartbeat(self):
        """Heartbeat-Thread stoppen."""
        self._heartbeat_running = False
        if self._heartbeat_thread:
            self._heartbeat_thread.join(timeout=2)
            self._heartbeat_thread = None

    def _heartbeat_loop(self):
        """Sendet regelmaessig HEARTBEAT um Watchdog-Timeout zu verhindern."""
        interval = max(1, self._heartbeat_timeout // 2)
        while self._heartbeat_running and self.connected:
            time.sleep(interval)
            if self._heartbeat_running and self.connected:
                self._send("HEARTBEAT")

    # ================================================================
    #                      HAUPTBEFEHLE
    # ================================================================

    def set_progress(self, percent: int) -> bool:
        """Fortschrittsbalken anzeigen (0-100%).

        Der Balken fuellt sich gruen von links nach rechts.
        Uebergaenge sind sanft interpoliert.
        """
        percent = max(0, min(100, int(percent)))
        return self._send(f"PROGRESS:{percent}") == "OK"

    def done(self) -> bool:
        """Scan abgeschlossen — gruenes Atmen.

        Signalisiert dem Operator: naechstes Geraet einlegen.
        Beginnt mit kurzem hellem Blitz, dann sanftes gruenes Atmen.
        """
        return self._send("STATE:DONE") == "OK"

    def waiting(self) -> bool:
        """Benutzereingabe erforderlich — gelbes Atmen.

        Signalisiert dem Operator: zum PC gehen, Eingabe machen.
        """
        return self._send("STATE:WAITING") == "OK"

    def error(self) -> bool:
        """Fehler aufgetreten — rotes Atmen.

        Signalisiert dem Operator: zum PC gehen, Fehler beheben.
        """
        return self._send("STATE:ERROR") == "OK"

    def idle(self) -> bool:
        """Standby — sanftes blaues Atmen.

        System bereit, kein Scan aktiv.
        """
        return self._send("STATE:IDLE") == "OK"

    def saving(self) -> bool:
        """Daten werden gespeichert — weiss/gold Atmen.

        Signalisiert dem Operator: EEPROM wird geschrieben, bitte warten.
        """
        return self._send("STATE:SAVE") == "OK"

    def calibrated(self) -> bool:
        """Komplett kalibriert — Regenbogen-Sweep!

        Belohnungs-Animation: alle Frequenzen erfolgreich kalibriert.
        """
        return self._send("STATE:CALIBRATED") == "OK"

    def connected_flash(self) -> bool:
        """Handstueck erkannt — kurzer gruener Flash.

        Kurze Bestaetigung (~800ms), dann automatisch zurueck zu IDLE.
        """
        return self._send("STATE:CONNECT") == "OK"

    def off(self) -> bool:
        """LEDs ausschalten (sanftes Ausfaden)."""
        return self._send("STATE:OFF") == "OK"

    def set_brightness(self, value: int) -> bool:
        """Helligkeit setzen (0-255). Uebergang ist sanft."""
        value = max(0, min(255, int(value)))
        self._brightness = value
        return self._send(f"BRIGHTNESS:{value}") == "OK"

    def set_flip(self, flipped: bool) -> bool:
        """LED-Richtung spiegeln (fuer Montage oben/unten am Monitor).

        flipped=False: Links nach rechts (Standard, Montage unten)
        flipped=True:  Rechts nach links (Montage oben, Leiste gedreht)
        """
        return self._send(f"FLIP:{1 if flipped else 0}") == "OK"

    def set_state_color(self, state: str, r: int, g: int, b: int) -> bool:
        """Farbe fuer einen Zustand setzen.

        state: IDLE, PROGRESS, DONE, WAITING, ERROR, SAVE, CONNECT, DISCONNECTED
        r, g, b: 0-255

        Beispiel: led.set_state_color("PROGRESS", 0, 120, 255)  # Hellblau
        """
        return self._send(f"STATECOLOR:{state.upper()},{r},{g},{b}") == "OK"

    def clear(self) -> bool:
        """Alle LEDs ausschalten."""
        return self._send("CLEAR") == "OK"

    def info(self) -> str:
        """Geraeteinfo abfragen."""
        return self._send("INFO")

    # ================================================================
    #                     VERBINDUNG TRENNEN
    # ================================================================

    def close(self):
        """Verbindung trennen und LEDs ausschalten."""
        self._stop_heartbeat()
        if self._serial and self._serial.is_open:
            try:
                self._send("STATE:OFF")
                time.sleep(0.3)  # Kurz warten fuer Fade-Out
                self._serial.close()
            except serial.SerialException:
                pass
        self._connected = False
        print("[PandaLED] Verbindung getrennt.")

    def __enter__(self):
        return self

    def __exit__(self, *args):
        self.close()

    def __del__(self):
        try:
            self.close()
        except Exception:
            pass


# ================================================================
#                        SCHNELLTEST
# ================================================================

if __name__ == "__main__":
    print("=== Panda Status LED Test V2.1 ===\n")

    with PandaLED(brightness=60) as led:
        if not led.connected:
            print("Kein Geraet gefunden. COM-Port pruefen.")
            exit(1)

        print(f"Firmware: {led.firmware_version}\n")

        # Zustaende testen (mit sanften Uebergaengen)
        tests = [
            ("idle",    "IDLE (blaues Atmen)",          3),
            ("waiting", "WAITING (gelbes Atmen)",       3),
            ("error",   "ERROR (rotes Atmen)",          3),
            ("saving",  "SAVE (weiss/gold Atmen)",      3),
            ("done",    "DONE (gruenes Atmen)",         3),
        ]

        for method, desc, duration in tests:
            print(f"  {desc}...")
            getattr(led, method)()
            time.sleep(duration)

        # Fortschrittsbalken
        print("\n  Fortschrittsbalken 0-100%...")
        for p in range(0, 101, 2):
            led.set_progress(p)
            time.sleep(0.08)
        time.sleep(1)

        # Fertig-Effekt
        print("  DONE-Effekt (Blitz + Atmen)...")
        led.done()
        time.sleep(4)

        # Connect-Flash testen
        print("  CONNECT (gruener Flash -> auto IDLE)...")
        led.connected_flash()
        time.sleep(2)

        # Calibrated testen
        print("  CALIBRATED (Regenbogen-Sweep)...")
        led.calibrated()
        time.sleep(5)

        # Helligkeit testen
        print("  Helligkeit: dunkel -> hell -> normal...")
        led.set_brightness(20)
        time.sleep(1.5)
        led.set_brightness(200)
        time.sleep(1.5)
        led.set_brightness(60)
        time.sleep(1.5)

        led.idle()
        time.sleep(1)
        print("\n=== Test abgeschlossen ===")
