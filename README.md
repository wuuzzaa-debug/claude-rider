# Claude Rider

![Claude Rider Demo](demo.gif)

> Knight Rider meets Claude Code — a physical LED status indicator
> for your AI coding assistant.

Claude Rider turns a cheap LED bar into a Knight-Rider-inspired status display
that shows you at a glance what Claude Code is doing:

- **Thinking:** Red KITT scanner races back and forth
- **Waiting for input:** Amber pulsing
- **Error:** Red breathing
- **Done:** Green flash, then soft blue breathing
- **Idle:** Soft blue breathing

## Hardware

You need a **BIGTREETECH Panda Status LED bar**:

- **Buy here:** https://biqu.equipment/products/biqu-panda-status-magnetic-mount-customizable-rgb
- ~25 USD
- ESP32-C3-MINI microcontroller (RISC-V)
- 25x WS2812 RGB LEDs
- USB-C connection
- Magnetic mount — sticks to your monitor frame
- Originally designed for Bambu 3D printers, but we flash custom firmware

> **Note:** Any BIGTREETECH Panda Status bar will work. The magnetic mount
> makes it easy to attach to your monitor bezel or desk lamp.

## Setup (Step by Step)

### Step 1: Install PlatformIO

You need PlatformIO to flash the firmware. Install it once:

```bash
pip install platformio
```

### Step 2: Flash the firmware

1. **DO NOT plug in the LED bar yet**
2. **Hold the BOOT button** on the LED bar (small button on the PCB)
3. **While holding the button, plug in the USB-C cable**
4. Release the button — the LED bar is now in bootloader mode
5. Flash the firmware:

```bash
cd firmware
pio run -t upload
```

6. **Unplug and replug** the USB cable (normal mode now)
7. The LED bar should show a red Knight Rider scan, then soft blue breathing

> **Tip:** If upload fails with "port not found", make sure you're holding
> the BOOT button while plugging in. The CH340K serial chip needs the
> ESP32 in bootloader mode to flash.
>
> On Windows you may need the [CH340 driver](https://www.wch-ic.com/downloads/CH341SER_EXE.html)
> if the device doesn't show up.

### Step 3: Install Python dependencies

```bash
pip install pyserial
```

### Step 4: Start Claude Rider

**Windows:** Double-click `start_claude_rider.bat` — done!

**Manual start:**
```bash
python python/claude_rider.py --daemon
```

The daemon automatically finds the LED bar by scanning all COM ports
(sends PING, waits for PONG — no driver name guessing).

You should see:
```
[INFO] Connected to CLAUDE_RIDER on COM15
[INFO] Daemon listening on 127.0.0.1:17177
```

### Step 5: Set up Claude Code Hooks

Add this to your `~/.claude/settings.json` (create the file if it doesn't exist).
If you already have hooks, add the Claude Rider entries to each hook's `hooks` array.

```json
{
  "hooks": {
    "UserPromptSubmit": [
      {
        "hooks": [
          {
            "type": "command",
            "command": "python PATH/python/claude_rider.py --event thinking",
            "timeout": 3
          }
        ]
      }
    ],
    "PreToolUse": [
      {
        "hooks": [
          {
            "type": "command",
            "command": "python PATH/python/claude_rider.py --event tool_pending",
            "timeout": 3
          }
        ]
      }
    ],
    "PostToolUse": [
      {
        "hooks": [
          {
            "type": "command",
            "command": "python PATH/python/claude_rider.py --event tool_running",
            "timeout": 3
          }
        ]
      }
    ],
    "PostToolUseFailure": [
      {
        "hooks": [
          {
            "type": "command",
            "command": "python PATH/python/claude_rider.py --event error",
            "timeout": 3
          }
        ]
      }
    ],
    "Stop": [
      {
        "hooks": [
          {
            "type": "command",
            "command": "python PATH/python/claude_rider.py --event task_done",
            "timeout": 3
          }
        ]
      }
    ],
    "Notification": [
      {
        "matcher": "idle_prompt",
        "hooks": [
          {
            "type": "command",
            "command": "python PATH/python/claude_rider.py --event idle",
            "timeout": 3
          }
        ]
      },
      {
        "matcher": "permission_prompt",
        "hooks": [
          {
            "type": "command",
            "command": "python PATH/python/claude_rider.py --event waiting",
            "timeout": 3
          }
        ]
      }
    ],
    "SessionStart": [
      {
        "hooks": [
          {
            "type": "command",
            "command": "python PATH/python/claude_rider.py --event session_start",
            "timeout": 3
          }
        ]
      }
    ]
  }
}
```

**Replace `PATH` with the full path to your Claude Rider folder**, e.g.:
- Windows: `C:/Users/you/claude-rider`
- Mac/Linux: `/home/you/claude-rider`

### Step 6: Test it

Open Claude Code and type something. The LED bar should:
1. Show red KITT scanner while Claude is thinking
2. Flash green when done
3. Fade to blue breathing after 3 seconds

## Customization

Copy `config.json.example` to `config.json` and tweak:

```json
{
  "brightness": 255,
  "knight_rider_speed": 9,
  "knight_rider_tail": 5,
  "knight_rider_glow": 8,
  "colors": {
    "KNIGHT_RIDER": [255, 10, 0],
    "IDLE": [0, 0, 50],
    "WAITING": [255, 160, 0],
    "ERROR": [255, 0, 0],
    "DONE": [0, 255, 0]
  }
}
```

| Setting | Range | Description |
|---------|-------|-------------|
| `brightness` | 0-255 | LED brightness (255 = full) |
| `knight_rider_speed` | 1-10 | Scanner speed (1=slow, 10=insane) |
| `knight_rider_tail` | 4-25 | Number of lit LEDs in the tail |
| `knight_rider_glow` | 0-100 | Base red glow on all LEDs (0=off, like original KITT: ~8) |

All settings are applied at daemon startup. Change `config.json` and restart the daemon.

## CLI Reference

```bash
python claude_rider.py --daemon          # Start daemon
python claude_rider.py --event thinking  # Send event (KITT scanner)
python claude_rider.py --event idle      # Send event (blue breathing)
python claude_rider.py --status          # Check if daemon is running
python claude_rider.py --stop            # Stop daemon
```

## Stop

Double-click `stop_claude_rider.bat` or:

```bash
python claude_rider.py --stop
```

## Troubleshooting

**LED bar not found:**
- Is the USB cable plugged in?
- On Windows: Check Device Manager for "USB-SERIAL CH340K" under Ports
- Install [CH340 driver](https://www.wch-ic.com/downloads/CH341SER_EXE.html) if not detected

**Flash fails:**
- Hold BOOT button WHILE plugging in USB — this is required
- The ESP32 won't accept firmware in normal mode

**Daemon starts but LEDs don't react to Claude Code:**
- Check that hooks are in `~/.claude/settings.json`
- Make sure PATH in hooks points to the correct `claude_rider.py`
- Restart Claude Code after editing settings.json

**LEDs stuck on one color:**
- Restart daemon: `python claude_rider.py --stop` then `--daemon`
- Or press the button on the LED bar to reset to IDLE

## License

MIT
