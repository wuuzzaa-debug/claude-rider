"""
Panda Status LED Controller
============================
Steuert die Custom-Firmware auf der Panda Status LED-Leiste
ueber serielle Verbindung (CH340K USB-Serial).

Verwendung:
    from panda_led_controller import PandaLED

    led = PandaLED()          # Auto-Erkennung des COM-Ports
    led = PandaLED("COM15")   # Oder manuell angeben

    led.set_progress(50)            # Fortschrittsbalken 50%
    led.set_state("SCANNING")       # Animationszustand
    led.set_color(255, 0, 0)        # Basisfarbe Rot
    led.set_brightness(100)         # Helligkeit
    led.solid(0, 255, 0)            # Alle LEDs gruen
    led.set_led(0, 255, 0, 0)      # LED 0 rot
    led.clear()                     # Alles aus

    led.close()
"""

import serial
import serial.tools.list_ports
import time
import threading
from typing import Optional


class PandaLED:
    """Controller fuer die Panda Status LED-Leiste mit Custom Firmware."""

    NUM_LEDS = 25
    BAUD_RATE = 115200
    TIMEOUT = 1.0  # Sekunden

    def __init__(self, port: Optional[str] = None, auto_connect: bool = True):
        self._serial: Optional[serial.Serial] = None
        self._lock = threading.Lock()
        self._connected = False
        self._port = port

        if auto_connect:
            self.connect(port)

    def connect(self, port: Optional[str] = None) -> bool:
        """Verbindung zur LED-Leiste herstellen."""
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
            time.sleep(0.5)  # Warten auf ESP32 Reset nach Verbindung

            # Eingangspuffer leeren (inkl. READY-Meldung)
            self._serial.reset_input_buffer()

            # Verbindungstest
            if self._ping():
                self._connected = True
                print(f"[PandaLED] Verbunden auf {port}")
                return True
            else:
                print(f"[PandaLED] Geraet auf {port} antwortet nicht.")
                self._serial.close()
                return False

        except serial.SerialException as e:
            print(f"[PandaLED] Verbindungsfehler: {e}")
            return False

    def _find_port(self) -> Optional[str]:
        """Automatische Erkennung des Panda Status COM-Ports (CH340)."""
        for port_info in serial.tools.list_ports.comports():
            desc = (port_info.description or "").lower()
            hwid = (port_info.hwid or "").lower()
            # CH340 USB-Serial Adapter suchen
            if "ch340" in desc or "ch340" in hwid:
                print(f"[PandaLED] CH340 gefunden: {port_info.device} - {port_info.description}")
                return port_info.device
        return None

    def _send(self, command: str) -> str:
        """Befehl senden und Antwort lesen."""
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
        response = self._send("PING")
        return response == "PONG"

    @property
    def connected(self) -> bool:
        """True wenn verbunden und erreichbar."""
        return self._connected and self._serial is not None and self._serial.is_open

    # === Hauptbefehle ===

    def set_progress(self, percent: int) -> bool:
        """Fortschrittsbalken anzeigen (0-100%)."""
        percent = max(0, min(100, int(percent)))
        return self._send(f"PROGRESS:{percent}") == "OK"

    def set_state(self, state: str) -> bool:
        """Animationszustand setzen.

        Gueltige Zustaende:
            IDLE        - Sanftes blaues Atmen
            SCANNING    - Laufender Punkt (Frequenzscan)
            ERROR       - Rotes Blinken
            DONE        - Gruenes Leuchten
            CALIBRATING - Orange Wellenbewegung
            BUSY        - Lila Rotation
        """
        return self._send(f"STATE:{state.upper()}") == "OK"

    def set_color(self, r: int, g: int, b: int) -> bool:
        """Basisfarbe setzen (fuer animierte Zustaende)."""
        return self._send(f"COLOR:{r},{g},{b}") == "OK"

    def set_brightness(self, value: int) -> bool:
        """Helligkeit setzen (0-255)."""
        value = max(0, min(255, int(value)))
        return self._send(f"BRIGHTNESS:{value}") == "OK"

    def solid(self, r: int, g: int, b: int) -> bool:
        """Alle LEDs auf eine Farbe setzen."""
        return self._send(f"SOLID:{r},{g},{b}") == "OK"

    def set_led(self, index: int, r: int, g: int, b: int) -> bool:
        """Einzelne LED setzen (0-24)."""
        if not 0 <= index < self.NUM_LEDS:
            return False
        return self._send(f"LED:{index},{r},{g},{b}") == "OK"

    def clear(self) -> bool:
        """Alle LEDs ausschalten."""
        return self._send("CLEAR") == "OK"

    def info(self) -> str:
        """Geraeteinfo abfragen."""
        return self._send("INFO")

    def close(self):
        """Verbindung trennen."""
        if self._serial and self._serial.is_open:
            try:
                self.clear()
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
        self.close()


# === Schnelltest ===
if __name__ == "__main__":
    print("=== Panda Status LED Test ===\n")

    with PandaLED() as led:
        if not led.connected:
            print("Kein Geraet gefunden. Bitte COM-Port pruefen.")
            exit(1)

        print(f"Info: {led.info()}\n")

        # Zustaende durchtesten
        tests = [
            ("IDLE",        "Idle (blaues Atmen)",    3),
            ("SCANNING",    "Scanning (Laufpunkt)",   3),
            ("CALIBRATING", "Kalibrierung (Orange)",  3),
            ("BUSY",        "Beschaeftigt (Lila)",    3),
            ("ERROR",       "Fehler (Rot blinkt)",    3),
            ("DONE",        "Fertig (Gruen)",         3),
        ]

        for state, desc, duration in tests:
            print(f"  {desc}...")
            led.set_state(state)
            time.sleep(duration)

        # Fortschrittsbalken testen
        print("\n  Fortschrittsbalken 0-100%...")
        for p in range(0, 101, 2):
            led.set_progress(p)
            time.sleep(0.05)
        time.sleep(1)

        # Einzelne LEDs
        print("  Einzelne LEDs (RGB)...")
        led.clear()
        for i in range(led.NUM_LEDS):
            r = 255 if i % 3 == 0 else 0
            g = 255 if i % 3 == 1 else 0
            b = 255 if i % 3 == 2 else 0
            led.set_led(i, r, g, b)
            time.sleep(0.08)
        time.sleep(1)

        led.clear()
        print("\n=== Test abgeschlossen ===")
