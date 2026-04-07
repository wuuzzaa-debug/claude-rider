# Claude Rider

> Knight Rider meets Claude Code — a physical LED status indicator
> for your AI coding assistant.

Claude Rider turns a cheap LED bar into a Knight-Rider-inspired status display
that shows you at a glance what Claude Code is doing:

- **Thinking:** Red KITT scanner races back and forth
- **Waiting for input:** Amber pulsing
- **Error:** Red breathing
- **Done:** Green flash
- **Idle:** Soft blue breathing

## Hardware

You need a **BIGTREETECH Panda Status LED bar** (~15 EUR):
- ESP32-C3-MINI with 25 WS2812 LEDs
- USB-C connection
- Simply attach to your monitor

## Quick Start

### 1. Flash firmware (once)

```bash
cd firmware
pip install platformio
pio run -t upload
```

> Tip: If upload fails, hold the button while plugging in USB (bootloader mode).

### 2. Start

Double-click `start_claude_rider.bat` — done!

The script:
- Installs pyserial if needed
- Finds the LED bar automatically (PING/PONG)
- Starts the daemon in the background

### 3. Set up Claude Code Hooks

Add this to your `~/.claude/settings.json`:

```json
{
  "hooks": {
    "UserPromptSubmit": [
      {
        "hooks": [{ "type": "command", "command": "python PATH/python/claude_rider.py --event thinking", "timeout": 3 }]
      }
    ],
    "Stop": [
      {
        "hooks": [{ "type": "command", "command": "python PATH/python/claude_rider.py --event task_done", "timeout": 3 }]
      }
    ],
    "PreToolUse": [
      {
        "hooks": [{ "type": "command", "command": "python PATH/python/claude_rider.py --event tool_pending", "timeout": 3 }]
      }
    ],
    "PostToolUse": [
      {
        "hooks": [{ "type": "command", "command": "python PATH/python/claude_rider.py --event tool_running", "timeout": 3 }]
      }
    ],
    "PostToolUseFailure": [
      {
        "hooks": [{ "type": "command", "command": "python PATH/python/claude_rider.py --event error", "timeout": 3 }]
      }
    ],
    "Notification": [
      {
        "matcher": "idle_prompt",
        "hooks": [{ "type": "command", "command": "python PATH/python/claude_rider.py --event idle", "timeout": 3 }]
      },
      {
        "matcher": "permission_prompt",
        "hooks": [{ "type": "command", "command": "python PATH/python/claude_rider.py --event waiting", "timeout": 3 }]
      }
    ],
    "SessionStart": [
      {
        "hooks": [{ "type": "command", "command": "python PATH/python/claude_rider.py --event session_start", "timeout": 3 }]
      }
    ]
  }
}
```

Replace `PATH` with the actual path to your Claude Rider folder.

## Customization

Copy `config.json.example` to `config.json` and customize:

```json
{
  "brightness": 80,
  "knight_rider_speed": 3,
  "colors": {
    "KNIGHT_RIDER": [255, 10, 0],
    "IDLE": [0, 0, 50]
  }
}
```

**Colors:** RGB values (0-255) per state
**Speed:** 1 (slow) to 5 (turbo)
**Brightness:** 0 (off) to 255 (full power)

## CLI

```bash
python claude_rider.py --daemon          # Start daemon
python claude_rider.py --event thinking  # Send event
python claude_rider.py --status          # Check status
python claude_rider.py --stop            # Stop daemon
```

## Stop

Double-click `stop_claude_rider.bat` or:

```bash
python claude_rider.py --stop
```

## License

MIT
