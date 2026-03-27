"""
Integration der Panda Status LED-Leiste in die Wellcomet Kalibriersoftware
==========================================================================

Dieses Modul zeigt, wie die LED-Leiste in main.py integriert wird.
Die Aenderungen koennen direkt in die bestehende Kalibriersoftware
uebernommen werden.

Benoetigte Aenderungen in main.py:
  1. Import hinzufuegen
  2. PandaLED in __init__ initialisieren
  3. Signal-Handler fuer scan_progress verbinden
  4. Zustandswechsel an relevanten Stellen einfuegen
"""

import sys
import os

# Pfad zum LED-Controller hinzufuegen
sys.path.insert(0, os.path.join(
    os.path.dirname(__file__),
    r"J:\Liquidbeam\10_WELLCOMET\Status LED Leiste Pandastatus\python"
))

from panda_led_controller import PandaLED


class CalibrationLEDIntegration:
    """
    Wrapper fuer die LED-Integration in die Kalibriersoftware.

    Verwendung in CalibrationMainWindow.__init__():
        self.led_status = CalibrationLEDIntegration()

    Dann an den relevanten Stellen aufrufen:
        self.led_status.on_scan_start()
        self.led_status.on_scan_progress(current, total)
        self.led_status.on_scan_finished()
        self.led_status.on_scan_error(message)
        self.led_status.on_calibration_start()
        self.led_status.on_calibration_done()
        self.led_status.on_device_reading()
    """

    def __init__(self, port=None):
        self.led = PandaLED(port=port, auto_connect=True)
        if self.led.connected:
            self.led.set_state("IDLE")

    def on_scan_start(self):
        """Aufruf wenn Frequenzscan startet."""
        if self.led.connected:
            self.led.set_state("SCANNING")

    def on_scan_progress(self, current: int, total: int):
        """Aufruf bei jedem Scan-Fortschritt.

        Direkt mit dem scan_progress Signal verbinden:
            engine.scan_progress.connect(
                lambda cur, tot, txt: self.led_status.on_scan_progress(cur, tot)
            )
        """
        if self.led.connected and total > 0:
            percent = int(current / total * 100)
            self.led.set_progress(percent)

    def on_scan_finished(self):
        """Aufruf wenn Scan abgeschlossen."""
        if self.led.connected:
            self.led.set_progress(100)
            import time
            time.sleep(0.5)
            self.led.set_state("DONE")

    def on_scan_error(self, message: str = ""):
        """Aufruf bei Scan-Fehler."""
        if self.led.connected:
            self.led.set_state("ERROR")

    def on_calibration_start(self):
        """Aufruf wenn Kalibrierung gestartet wird."""
        if self.led.connected:
            self.led.set_state("CALIBRATING")

    def on_calibration_done(self):
        """Aufruf nach erfolgreicher Kalibrierung."""
        if self.led.connected:
            self.led.set_state("DONE")

    def on_device_reading(self):
        """Aufruf waehrend Geraetedaten gelesen werden."""
        if self.led.connected:
            self.led.set_state("BUSY")

    def on_idle(self):
        """Aufruf wenn nichts passiert."""
        if self.led.connected:
            self.led.set_state("IDLE")

    def close(self):
        """Verbindung trennen."""
        if self.led.connected:
            self.led.set_state("IDLE")
            self.led.close()


# ============================================================
# KONKRETE AENDERUNGEN FUER main.py
# ============================================================
#
# 1) Am Anfang von main.py (bei den Imports):
#
#    sys.path.insert(0, r"J:\Liquidbeam\10_WELLCOMET\Status LED Leiste Pandastatus\python")
#    from panda_led_controller import PandaLED
#
#
# 2) In CalibrationMainWindow.__init__() (nach GUI-Setup):
#
#    # LED-Statusanzeige initialisieren
#    try:
#        self.panda_led = PandaLED()
#        if self.panda_led.connected:
#            self.panda_led.set_state("IDLE")
#            self.statusBar().showMessage("Panda Status LED verbunden", 3000)
#    except Exception:
#        self.panda_led = None
#
#
# 3) In _on_scan_progress() (ca. Zeile 4251):
#
#    # Nach dem statusbar Update:
#    if self.panda_led and self.panda_led.connected:
#        pct = int(current / total * 100) if total > 0 else 0
#        self.panda_led.set_progress(pct)
#
#
# 4) In start_frequency_scan() (ca. Zeile 4168):
#
#    # Vor engine.start_scan():
#    if self.panda_led and self.panda_led.connected:
#        self.panda_led.set_state("SCANNING")
#
#
# 5) In _on_scan_finished() :
#
#    if self.panda_led and self.panda_led.connected:
#        self.panda_led.set_state("DONE")
#
#
# 6) In _on_scan_error():
#
#    if self.panda_led and self.panda_led.connected:
#        self.panda_led.set_state("ERROR")
#
#
# 7) In closeEvent() (Programmende):
#
#    if self.panda_led:
#        self.panda_led.close()
#
# ============================================================
