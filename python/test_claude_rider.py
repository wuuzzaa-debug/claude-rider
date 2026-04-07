"""
Tests for Claude Rider LED Controller
======================================
Covers: config system, event mapping, serial communication (mocked),
        and daemon socket communication.
"""

import json
import os
import socket
import tempfile
import threading
import time
import pytest
from unittest.mock import MagicMock, patch, PropertyMock

from claude_rider import (
    DEFAULT_CONFIG,
    _deep_merge,
    load_config,
    map_event,
    ClaudeRiderSerial,
    build_daemon_message,
    send_to_daemon,
    is_daemon_running,
    find_config_path,
    DAEMON_HOST,
    DAEMON_PORT,
)


# ============================================================
#  Task 3: Config and Event Mapping Tests
# ============================================================


class TestDefaultConfig:
    """Verify that DEFAULT_CONFIG contains all required keys."""

    def test_default_config_has_serial(self):
        assert "serial" in DEFAULT_CONFIG
        assert "port" in DEFAULT_CONFIG["serial"]
        assert "baud" in DEFAULT_CONFIG["serial"]
        assert DEFAULT_CONFIG["serial"]["baud"] == 115200

    def test_default_config_has_brightness(self):
        assert "brightness" in DEFAULT_CONFIG
        assert isinstance(DEFAULT_CONFIG["brightness"], int)

    def test_default_config_has_flip(self):
        assert "flip" in DEFAULT_CONFIG
        assert DEFAULT_CONFIG["flip"] is False

    def test_default_config_has_knight_rider_speed(self):
        assert "knight_rider_speed" in DEFAULT_CONFIG
        assert 1 <= DEFAULT_CONFIG["knight_rider_speed"] <= 5

    def test_default_config_has_events(self):
        assert "events" in DEFAULT_CONFIG
        events = DEFAULT_CONFIG["events"]
        assert "thinking" in events
        assert "progress" in events
        assert "done" in events
        assert "error" in events
        assert "idle" in events

    def test_default_config_has_colors(self):
        assert "colors" in DEFAULT_CONFIG
        colors = DEFAULT_CONFIG["colors"]
        assert "idle" in colors
        assert "knight_rider" in colors
        assert "error" in colors
        for name, rgb in colors.items():
            assert len(rgb) == 3, f"Color {name} must be [r, g, b]"

    def test_default_config_has_daemon(self):
        assert "daemon" in DEFAULT_CONFIG
        assert DEFAULT_CONFIG["daemon"]["host"] == "127.0.0.1"
        assert DEFAULT_CONFIG["daemon"]["port"] == 17177


class TestDeepMerge:
    """Test the recursive deep-merge helper."""

    def test_shallow_override(self):
        result = _deep_merge({"a": 1, "b": 2}, {"b": 99})
        assert result == {"a": 1, "b": 99}

    def test_nested_merge(self):
        base = {"serial": {"port": "auto", "baud": 115200}}
        override = {"serial": {"port": "COM5"}}
        result = _deep_merge(base, override)
        assert result["serial"]["port"] == "COM5"
        assert result["serial"]["baud"] == 115200

    def test_does_not_mutate_base(self):
        base = {"a": {"x": 1}}
        override = {"a": {"x": 2}}
        _deep_merge(base, override)
        assert base["a"]["x"] == 1

    def test_new_keys_added(self):
        result = _deep_merge({"a": 1}, {"b": 2})
        assert result == {"a": 1, "b": 2}


class TestLoadConfig:
    """Test loading and merging of config files."""

    def test_load_config_no_file_returns_defaults(self):
        cfg = load_config(None)
        assert cfg == DEFAULT_CONFIG

    def test_load_config_nonexistent_file_returns_defaults(self):
        cfg = load_config("/nonexistent/path/config.json")
        assert cfg == DEFAULT_CONFIG

    def test_load_config_from_file(self):
        override = {"brightness": 200, "serial": {"port": "COM7"}}
        with tempfile.NamedTemporaryFile(
            mode="w", suffix=".json", delete=False
        ) as fh:
            json.dump(override, fh)
            tmp_path = fh.name

        try:
            cfg = load_config(tmp_path)
            # Overridden values
            assert cfg["brightness"] == 200
            assert cfg["serial"]["port"] == "COM7"
            # Defaults preserved
            assert cfg["serial"]["baud"] == 115200
            assert "events" in cfg
            assert "colors" in cfg
        finally:
            os.unlink(tmp_path)


class TestEventMapping:
    """Test event-name to firmware-command translation."""

    def test_thinking_maps_to_knight_rider(self):
        cfg = load_config()
        assert map_event(cfg, "thinking") == "STATE:KNIGHT_RIDER"

    def test_error_maps_to_state_error(self):
        cfg = load_config()
        assert map_event(cfg, "error") == "STATE:ERROR"

    def test_done_maps_to_state_done(self):
        cfg = load_config()
        assert map_event(cfg, "done") == "STATE:DONE"

    def test_idle_maps_to_state_idle(self):
        cfg = load_config()
        assert map_event(cfg, "idle") == "STATE:IDLE"

    def test_off_maps_to_state_off(self):
        cfg = load_config()
        assert map_event(cfg, "off") == "STATE:OFF"

    def test_event_mapping_with_value(self):
        cfg = load_config()
        result = map_event(cfg, "progress", value=75)
        assert result == "PROGRESS:75"

    def test_progress_without_value_returns_none(self):
        cfg = load_config()
        result = map_event(cfg, "progress")
        assert result is None

    def test_unknown_event_returns_none(self):
        cfg = load_config()
        assert map_event(cfg, "totally_unknown_event") is None


# ============================================================
#  Task 4: Serial Communication Tests (Mocked)
# ============================================================


class TestClaudeRiderSerial:
    """Tests for the serial communication class using mocked serial."""

    def _make_mock_serial(self, responses):
        """Create a mock serial object that returns *responses* in order."""
        mock_ser = MagicMock()
        mock_ser.is_open = True
        # Each readline() call returns the next response
        mock_ser.readline.side_effect = [
            (r + "\n").encode("utf-8") for r in responses
        ]
        return mock_ser

    def test_send_command_formats_correctly(self):
        """send() should write the command with a newline and return response."""
        rider = ClaudeRiderSerial(port="COM99")
        mock_ser = self._make_mock_serial(["OK"])
        rider._serial = mock_ser

        result = rider.send("STATE:IDLE")

        mock_ser.write.assert_called_once_with(b"STATE:IDLE\n")
        mock_ser.flush.assert_called_once()
        assert result == "OK"

    def test_send_strips_existing_newline(self):
        """send() should not double the newline."""
        rider = ClaudeRiderSerial(port="COM99")
        mock_ser = self._make_mock_serial(["OK"])
        rider._serial = mock_ser

        rider.send("BRIGHTNESS:100\n")
        mock_ser.write.assert_called_once_with(b"BRIGHTNESS:100\n")

    def test_ping_returns_true_on_pong(self):
        rider = ClaudeRiderSerial(port="COM99")
        mock_ser = self._make_mock_serial(["PONG"])
        rider._serial = mock_ser

        assert rider.ping() is True

    def test_ping_returns_false_on_wrong_response(self):
        rider = ClaudeRiderSerial(port="COM99")
        mock_ser = self._make_mock_serial(["NOPE"])
        rider._serial = mock_ser

        assert rider.ping() is False

    def test_send_returns_none_when_disconnected(self):
        rider = ClaudeRiderSerial(port="COM99")
        # _serial is None — not connected
        assert rider.send("PING") is None

    def test_connected_property(self):
        rider = ClaudeRiderSerial()
        assert rider.connected is False

        mock_ser = MagicMock()
        mock_ser.is_open = True
        rider._serial = mock_ser
        assert rider.connected is True

    def test_port_property(self):
        rider = ClaudeRiderSerial(port="COM3")
        assert rider.port == "COM3"

    def test_close_sends_off(self):
        rider = ClaudeRiderSerial(port="COM99")
        mock_ser = self._make_mock_serial(["OK"])
        rider._serial = mock_ser

        rider.close()

        # Should have written STATE:OFF
        written = mock_ser.write.call_args_list
        assert any(b"STATE:OFF\n" in call.args for call in written)
        mock_ser.close.assert_called_once()


# ============================================================
#  Task 5: Daemon Message and Socket Tests
# ============================================================


class TestDaemonMessages:
    """Test daemon message building."""

    def test_daemon_message_format(self):
        msg = build_daemon_message("thinking")
        parsed = json.loads(msg)
        assert parsed == {"event": "thinking"}

    def test_daemon_message_with_value(self):
        msg = build_daemon_message("progress", 75)
        parsed = json.loads(msg)
        assert parsed == {"event": "progress", "value": 75}

    def test_daemon_message_with_string_value(self):
        msg = build_daemon_message("error", "timeout")
        parsed = json.loads(msg)
        assert parsed == {"event": "error", "value": "timeout"}


class TestDaemonSocket:
    """Test real TCP socket communication with a mini server."""

    def _start_mini_server(self, host, port, response_fn):
        """Start a tiny TCP server that calls *response_fn* per client."""
        server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        server.bind((host, port))
        server.listen(5)
        server.settimeout(5.0)

        def accept_loop():
            while True:
                try:
                    conn, _ = server.accept()
                    data = conn.recv(4096).decode("utf-8").strip()
                    resp = response_fn(data)
                    conn.sendall((resp + "\n").encode("utf-8"))
                    conn.close()
                except socket.timeout:
                    break
                except Exception:
                    break
            server.close()

        t = threading.Thread(target=accept_loop, daemon=True)
        t.start()
        time.sleep(0.1)  # let server start
        return server, t

    def test_send_to_daemon_and_receive(self):
        """Full round-trip: send event to mini server, receive response."""
        test_port = 17277  # avoid conflict with real daemon

        def handler(raw):
            msg = json.loads(raw)
            if msg.get("event") == "ping":
                return json.dumps({"status": "pong"})
            if msg.get("event") == "thinking":
                return json.dumps({"status": "ok", "command": "STATE:KNIGHT_RIDER"})
            return json.dumps({"status": "error"})

        server, thread = self._start_mini_server("127.0.0.1", test_port, handler)
        try:
            # Test ping
            resp = send_to_daemon("ping", host="127.0.0.1", port=test_port)
            assert resp is not None
            parsed = json.loads(resp)
            assert parsed["status"] == "pong"

            # Test event
            resp = send_to_daemon("thinking", host="127.0.0.1", port=test_port)
            assert resp is not None
            parsed = json.loads(resp)
            assert parsed["status"] == "ok"
            assert parsed["command"] == "STATE:KNIGHT_RIDER"
        finally:
            server.close()

    def test_is_daemon_running_false_when_no_server(self):
        """When nothing is listening the check should return False."""
        assert is_daemon_running(host="127.0.0.1", port=17377) is False

    def test_is_daemon_running_true_with_server(self):
        test_port = 17477

        def handler(raw):
            return json.dumps({"status": "pong"})

        server, thread = self._start_mini_server("127.0.0.1", test_port, handler)
        try:
            assert is_daemon_running(host="127.0.0.1", port=test_port) is True
        finally:
            server.close()


class TestFindConfigPath:
    """Test config file discovery."""

    def test_returns_none_when_no_config_found(self):
        # Patch os.path.isfile to always return False
        with patch("claude_rider.os.path.isfile", return_value=False):
            assert find_config_path() is None

    def test_finds_cwd_config(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            cfg_path = os.path.join(tmpdir, "config.json")
            with open(cfg_path, "w") as fh:
                json.dump({"brightness": 42}, fh)

            with patch("claude_rider.os.getcwd", return_value=tmpdir):
                result = find_config_path()
                # Should find config.json in CWD
                assert result is not None
                assert result.endswith("config.json")
