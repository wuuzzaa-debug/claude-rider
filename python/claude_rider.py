"""
Claude Rider LED Controller
============================
Controls the Claude Rider custom firmware (V3.0) on a 25-LED WS2812
status bar via USB-Serial (CH340K).

Remote status display for operators — driven by events from external
software or a local daemon process.

Architecture:
  - Config system with JSON file support and deep-merge defaults
  - Event mapping: translates application events to firmware commands
  - Serial communication with auto-detection and thread-safe access
  - Daemon mode: TCP socket server for multi-client event delivery
  - CLI interface for scripting and manual control

Usage (direct):
    from claude_rider import ClaudeRiderSerial, load_config, map_event
    cfg = load_config()
    ser = ClaudeRiderSerial()
    ser.connect()
    ser.send(map_event(cfg, "thinking"))
    ser.close()

Usage (daemon):
    python claude_rider.py --daemon
    python claude_rider.py --event thinking
    python claude_rider.py --event progress --value 75
    python claude_rider.py --stop
"""

import json
import os
import sys
import socket
import argparse
import threading
import time
import logging
from copy import deepcopy
from typing import Optional, Any

# pyserial may not be installed — guard the import
try:
    import serial
    import serial.tools.list_ports
    HAS_SERIAL = True
except ImportError:
    HAS_SERIAL = False

logger = logging.getLogger("claude_rider")

# ============================================================
#  Constants
# ============================================================

DAEMON_HOST = "127.0.0.1"
DAEMON_PORT = 17177
FW_ID = "CLAUDE_RIDER"

# ============================================================
#  Task 3: Config and Event Mapping
# ============================================================

DEFAULT_CONFIG: dict = {
    "serial": {
        "port": "auto",
        "baud": 115200,
        "timeout": 1.0,
    },
    "brightness": 255,
    "flip": False,
    "knight_rider_speed": 9,
    "knight_rider_tail": 5,
    "knight_rider_glow": 8,
    "heartbeat_interval": 4,
    "heartbeat_timeout": 15,
    "daemon": {
        "host": DAEMON_HOST,
        "port": DAEMON_PORT,
    },
    "events": {
        "session_start": "STATE:CONNECT",
        "thinking":      "STATE:KNIGHT_RIDER",
        "tool_pending":  "STATE:WAITING",
        "tool_running":  "STATE:KNIGHT_RIDER",
        "error":         "STATE:ERROR",
        "task_done":     "STATE:DONE",
        "idle":          "STATE:IDLE",
        "waiting":       "STATE:WAITING",
        "progress":      "PROGRESS:{value}",
    },
    "colors": {
        "KNIGHT_RIDER":  [255, 10, 0],
        "IDLE":          [0, 0, 50],
        "WAITING":       [255, 160, 0],
        "ERROR":         [255, 0, 0],
        "DONE":          [0, 255, 0],
        "PROGRESS":      [0, 120, 255],
    },
}


def _deep_merge(base: dict, override: dict) -> dict:
    """Recursively merge *override* into a copy of *base*.

    - Dict values are merged recursively.
    - Non-dict values in *override* replace those in *base*.
    - Keys present only in *base* are preserved.
    """
    result = deepcopy(base)
    for key, value in override.items():
        if key in result and isinstance(result[key], dict) and isinstance(value, dict):
            result[key] = _deep_merge(result[key], value)
        else:
            result[key] = deepcopy(value)
    return result


def load_config(path: Optional[str] = None) -> dict:
    """Load configuration from a JSON file and deep-merge with defaults.

    If *path* is ``None`` or the file does not exist the plain defaults
    are returned.
    """
    if path and os.path.isfile(path):
        with open(path, "r", encoding="utf-8") as fh:
            user_cfg = json.load(fh)
        return _deep_merge(DEFAULT_CONFIG, user_cfg)
    return deepcopy(DEFAULT_CONFIG)


def map_event(config: dict, event: str, value: Any = None) -> Optional[str]:
    """Map an event name to a firmware command string.

    Returns ``None`` for unknown events.
    If the event template contains ``{value}`` the caller must supply
    *value*; it is substituted into the string.
    """
    template = config.get("events", {}).get(event)
    if template is None:
        return None
    if value is not None:
        return template.format(value=value)
    # If template still has {value} placeholder but no value given,
    # return None to signal incomplete command.
    if "{value}" in template:
        return None
    return template


# ============================================================
#  Task 4: Serial Communication
# ============================================================

class ClaudeRiderSerial:
    """Thread-safe serial connection to the Claude Rider LED firmware."""

    def __init__(self, port: Optional[str] = None, baud: int = 115200):
        self._port_name: Optional[str] = port
        self._baud = baud
        self._serial: Optional[Any] = None
        self._lock = threading.Lock()

    # -- public API ------------------------------------------------

    def connect(self) -> bool:
        """Open serial port and validate via PING/PONG + INFO check.

        If *port* was ``None`` or ``"auto"`` at init time the method
        scans all available COM ports to find the device.

        Returns ``True`` on success, ``False`` otherwise.
        """
        if not HAS_SERIAL:
            logger.error("pyserial is not installed — cannot connect")
            return False

        if self._port_name and self._port_name != "auto":
            return self._try_connect(self._port_name)

        found = self._find_device()
        if found:
            self._port_name = found
            return True
        return False

    def send(self, command: str) -> Optional[str]:
        """Send a command and return the response line (thread-safe).

        Returns ``None`` when not connected or on error.
        """
        if not self._serial or not self._serial.is_open:
            return None
        with self._lock:
            try:
                line = command.strip() + "\n"
                self._serial.write(line.encode("utf-8"))
                self._serial.flush()
                resp = self._serial.readline().decode("utf-8").strip()
                return resp if resp else None
            except Exception as exc:
                logger.warning("Serial send error: %s", exc)
                return None

    def ping(self) -> bool:
        """Return ``True`` if firmware responds with PONG."""
        return self.send("PING") == "PONG"

    @property
    def connected(self) -> bool:
        """``True`` when the serial port is open."""
        return self._serial is not None and self._serial.is_open

    @property
    def port(self) -> Optional[str]:
        """Currently used port name (e.g. ``COM5``)."""
        return self._port_name

    def close(self) -> None:
        """Send STATE:OFF, then close the serial port."""
        if self._serial and self._serial.is_open:
            try:
                self.send("STATE:OFF")
            except Exception:
                pass
            try:
                self._serial.close()
            except Exception:
                pass
        self._serial = None

    # -- internal --------------------------------------------------

    def _try_connect(self, port_name: str) -> bool:
        """Open *port_name*, verify PING/PONG and INFO header."""
        try:
            ser = serial.Serial(port_name, self._baud, timeout=1.0)
            time.sleep(0.1)  # let firmware boot
            ser.reset_input_buffer()

            # PING check
            ser.write(b"PING\n")
            ser.flush()
            resp = ser.readline().decode("utf-8").strip()
            if resp != "PONG":
                ser.close()
                return False

            # INFO check
            ser.write(b"INFO\n")
            ser.flush()
            info = ser.readline().decode("utf-8").strip()
            if not info.startswith("INFO:" + FW_ID):
                ser.close()
                return False

            self._serial = ser
            self._port_name = port_name
            logger.info("Connected to %s on %s", FW_ID, port_name)
            return True
        except Exception as exc:
            logger.debug("Port %s failed: %s", port_name, exc)
            return False

    def _find_device(self) -> Optional[str]:
        """Scan all COM ports for a device that answers PING/PONG + INFO."""
        if not HAS_SERIAL:
            return None
        for port_info in serial.tools.list_ports.comports():
            if self._try_connect(port_info.device):
                return port_info.device
        return None


# ============================================================
#  Task 5: Daemon and CLI
# ============================================================

def build_daemon_message(event: str, value: Any = None) -> str:
    """Build a JSON message string for sending to the daemon."""
    msg: dict = {"event": event}
    if value is not None:
        msg["value"] = value
    return json.dumps(msg)


def send_to_daemon(
    event: str,
    value: Any = None,
    host: str = DAEMON_HOST,
    port: int = DAEMON_PORT,
) -> Optional[str]:
    """Send an event to a running daemon via TCP and return its response."""
    msg = build_daemon_message(event, value)
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
            sock.settimeout(5.0)
            sock.connect((host, port))
            sock.sendall((msg + "\n").encode("utf-8"))
            data = sock.recv(4096)
            return data.decode("utf-8").strip()
        return None
    except Exception as exc:
        logger.debug("send_to_daemon failed: %s", exc)
        return None


def is_daemon_running(host: str = DAEMON_HOST, port: int = DAEMON_PORT) -> bool:
    """Check whether a daemon is listening by sending a ping."""
    resp = send_to_daemon("ping", host=host, port=port)
    return resp is not None and "pong" in resp.lower()


class ClaudeRiderDaemon:
    """Persistent daemon that owns the serial connection and listens
    for events on a TCP socket."""

    def __init__(self, config: Optional[dict] = None):
        self._config = config or deepcopy(DEFAULT_CONFIG)
        self._serial = ClaudeRiderSerial(
            port=self._config["serial"]["port"],
            baud=self._config["serial"]["baud"],
        )
        self._server_sock: Optional[socket.socket] = None
        self._running = False
        self._heartbeat_thread: Optional[threading.Thread] = None
        self._accept_thread: Optional[threading.Thread] = None

    # -- public API ------------------------------------------------

    def start(self) -> bool:
        """Connect serial, send initial config, start socket server.

        Returns ``True`` when everything is up.
        """
        if not self._serial.connect():
            logger.error("Could not connect to device")
            return False

        # Send initial configuration
        cfg = self._config
        self._serial.send(f"BRIGHTNESS:{cfg['brightness']}")
        self._serial.send(f"SPEED:{cfg['knight_rider_speed']}")
        self._serial.send(f"TAIL:{cfg.get('knight_rider_tail', 5)}")
        self._serial.send(f"GLOW:{cfg.get('knight_rider_glow', 8)}")
        flip_val = 1 if cfg.get("flip") else 0
        self._serial.send(f"FLIP:{flip_val}")

        timeout_s = cfg.get("heartbeat_timeout", 15)
        self._serial.send(f"TIMEOUT:{timeout_s}")

        # Send custom colors
        for state_name, rgb in cfg.get("colors", {}).items():
            state_upper = state_name.upper()
            r, g, b = rgb
            self._serial.send(f"STATECOLOR:{state_upper},{r},{g},{b}")

        self._serial.send("STATE:IDLE")

        # Socket server
        self._running = True
        self._server_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._server_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        host = cfg["daemon"]["host"]
        port = cfg["daemon"]["port"]
        self._server_sock.bind((host, port))
        self._server_sock.listen(5)
        self._server_sock.settimeout(1.0)
        logger.info("Daemon listening on %s:%d", host, port)

        # Heartbeat thread
        self._heartbeat_thread = threading.Thread(
            target=self._heartbeat_loop, daemon=True
        )
        self._heartbeat_thread.start()

        # Accept thread
        self._accept_thread = threading.Thread(
            target=self._accept_loop, daemon=True
        )
        self._accept_thread.start()

        return True

    def stop(self) -> None:
        """Shut down daemon, send STATE:OFF, close everything."""
        self._running = False
        if self._serial.connected:
            self._serial.send("STATE:OFF")
            self._serial.close()
        if self._server_sock:
            try:
                self._server_sock.close()
            except Exception:
                pass
        logger.info("Daemon stopped")

    def run_forever(self) -> None:
        """Block the main thread until interrupted."""
        try:
            while self._running:
                time.sleep(0.5)
        except KeyboardInterrupt:
            pass
        finally:
            self.stop()

    # -- internal --------------------------------------------------

    def _accept_loop(self) -> None:
        """Accept incoming TCP connections."""
        while self._running:
            try:
                conn, addr = self._server_sock.accept()
                t = threading.Thread(
                    target=self._handle_client, args=(conn,), daemon=True
                )
                t.start()
            except socket.timeout:
                continue
            except Exception:
                if self._running:
                    logger.exception("Accept error")
                break

    def _handle_client(self, conn: socket.socket) -> None:
        """Parse a single JSON message from *conn* and act on it."""
        try:
            conn.settimeout(5.0)
            raw = conn.recv(4096).decode("utf-8").strip()
            if not raw:
                conn.close()
                return

            msg = json.loads(raw)
            event = msg.get("event", "")
            value = msg.get("value")

            if event == "ping":
                response = json.dumps({"status": "pong"})
            elif event == "stop":
                response = json.dumps({"status": "stopping"})
                conn.sendall((response + "\n").encode("utf-8"))
                conn.close()
                self.stop()
                return
            elif event == "status":
                response = json.dumps({
                    "status": "running",
                    "port": self._serial.port,
                    "connected": self._serial.connected,
                })
            else:
                cmd = map_event(self._config, event, value)
                if cmd:
                    # Cancel pending IDLE timer if new event comes in
                    if hasattr(self, '_idle_timer') and self._idle_timer is not None:
                        self._idle_timer.cancel()
                        self._idle_timer = None
                    fw_resp = self._serial.send(cmd)
                    response = json.dumps({"status": "ok", "command": cmd, "response": fw_resp})
                    # Auto-transition: DONE -> IDLE after 3 seconds
                    if event == "task_done":
                        self._schedule_idle(3.0)
                else:
                    response = json.dumps({"status": "error", "message": f"Unknown event: {event}"})

            conn.sendall((response + "\n").encode("utf-8"))
        except Exception as exc:
            logger.debug("Client handler error: %s", exc)
            try:
                err = json.dumps({"status": "error", "message": str(exc)})
                conn.sendall((err + "\n").encode("utf-8"))
            except Exception:
                pass
        finally:
            try:
                conn.close()
            except Exception:
                pass

    def _schedule_idle(self, delay: float) -> None:
        """After *delay* seconds, switch to IDLE (unless another event came in)."""
        if hasattr(self, '_idle_timer') and self._idle_timer is not None:
            self._idle_timer.cancel()
        def _go_idle():
            self._serial.send("STATE:IDLE")
        self._idle_timer = threading.Timer(delay, _go_idle)
        self._idle_timer.daemon = True
        self._idle_timer.start()

    def _heartbeat_loop(self) -> None:
        """Send HEARTBEAT to firmware periodically."""
        interval = self._config.get("heartbeat_interval", 4)
        while self._running:
            time.sleep(interval)
            if self._running and self._serial.connected:
                self._serial.send("HEARTBEAT")


# ============================================================
#  Config Path Finder
# ============================================================

def find_config_path() -> Optional[str]:
    """Search for a config file in standard locations.

    Order:
      1. ``./config.json`` (current working directory)
      2. ``<script_dir>/../config.json``
      3. ``~/.claude_rider.json``
    """
    candidates = [
        os.path.join(os.getcwd(), "config.json"),
        os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "config.json"),
        os.path.join(os.path.expanduser("~"), ".claude_rider.json"),
    ]
    for path in candidates:
        full = os.path.normpath(path)
        if os.path.isfile(full):
            return full
    return None


# ============================================================
#  CLI
# ============================================================

def main() -> None:
    """Command-line interface for Claude Rider LED control."""
    parser = argparse.ArgumentParser(
        description="Claude Rider LED Controller — daemon and CLI"
    )
    parser.add_argument("--daemon", action="store_true", help="Start as daemon")
    parser.add_argument("--event", type=str, help="Send event (e.g. thinking, progress, done)")
    parser.add_argument("--value", type=str, default=None, help="Value for event (e.g. 75 for progress)")
    parser.add_argument("--stop", action="store_true", help="Stop running daemon")
    parser.add_argument("--status", action="store_true", help="Query daemon status")
    parser.add_argument("--config", type=str, default=None, help="Path to config.json")
    parser.add_argument("--verbose", action="store_true", help="Enable debug logging")
    args = parser.parse_args()

    level = logging.DEBUG if args.verbose else logging.INFO
    logging.basicConfig(level=level, format="%(asctime)s [%(levelname)s] %(message)s")

    # Resolve config
    config_path = args.config or find_config_path()
    config = load_config(config_path)

    host = config["daemon"]["host"]
    port = config["daemon"]["port"]

    # --stop
    if args.stop:
        resp = send_to_daemon("stop", host=host, port=port)
        if resp:
            print(f"Daemon response: {resp}")
        else:
            print("No daemon running or no response.")
        return

    # --status
    if args.status:
        resp = send_to_daemon("status", host=host, port=port)
        if resp:
            print(f"Daemon status: {resp}")
        else:
            print("No daemon running.")
            sys.exit(1)
        return

    # --daemon
    if args.daemon:
        daemon = ClaudeRiderDaemon(config)
        if daemon.start():
            print(f"Daemon running on {host}:{port}")
            daemon.run_forever()
        else:
            print("Failed to start daemon (device not found?).")
            sys.exit(1)
        return

    # --event
    if args.event:
        value = args.value
        # Try numeric conversion for value
        if value is not None:
            try:
                value = int(value)
            except ValueError:
                try:
                    value = float(value)
                except ValueError:
                    pass  # keep as string

        # Try daemon first
        if is_daemon_running(host, port):
            resp = send_to_daemon(args.event, value, host, port)
            if resp:
                print(f"Daemon: {resp}")
            else:
                print("Daemon did not respond.")
            return

        # Fallback: direct serial
        cmd = map_event(config, args.event, value)
        if not cmd:
            print(f"Unknown event: {args.event}")
            sys.exit(1)

        ser = ClaudeRiderSerial(
            port=config["serial"]["port"],
            baud=config["serial"]["baud"],
        )
        if ser.connect():
            resp = ser.send(cmd)
            print(f"Firmware: {resp}")
            ser.close()
        else:
            print("Could not connect to device.")
            sys.exit(1)
        return

    # No action specified
    parser.print_help()


if __name__ == "__main__":
    main()
