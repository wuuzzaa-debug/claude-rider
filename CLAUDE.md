# Claude Rider

## Project
Open source LED status indicator for Claude Code.
BIGTREETECH Panda Status LED bar (ESP32-C3, 25x WS2812) shows
Claude Code state through animations — Knight Rider scanner
while thinking, color animations for different states.

## Hardware

| Parameter    | Value                          |
|--------------|--------------------------------|
| MCU          | ESP32-C3-MINI (RISC-V)        |
| LED Data Pin | GPIO 5 (via RMT)              |
| LED Type     | WS2812 (GRB, 800kHz)          |
| LED Count    | 25                             |
| Button       | GPIO 9 (active LOW, pullup)    |
| USB-Serial   | CH340K                         |
| Power        | USB-C, 5V 3A                   |

## Project Structure

```
claude-rider/
├── README.md
├── CLAUDE.md
├── LICENSE
├── config.json.example
├── firmware/
│   ├── platformio.ini
│   └── src/
│       └── main.cpp
├── python/
│   ├── claude_rider.py
│   └── test_claude_rider.py
├── start_claude_rider.bat
└── stop_claude_rider.bat
```

## Serial Protocol (115200 Baud)

```
PING                           -> PONG
INFO                           -> INFO:CLAUDE_RIDER,25,V3.0
STATE:<state>                  -> OK
PROGRESS:<0-100>               -> OK
BRIGHTNESS:<0-255>             -> OK
SPEED:<1-10>                   -> OK     Knight Rider scan speed
TAIL:<4-25>                    -> OK     Knight Rider tail length (LEDs)
GLOW:<0-100>                   -> OK     Knight Rider base glow percent
STATECOLOR:<st>,<r>,<g>,<b>   -> OK     Color per state
FLIP:<0|1>                     -> OK     Mirror LED direction
TIMEOUT:<seconds>              -> OK     Heartbeat timeout (0=off)
HEARTBEAT                      -> OK     Reset watchdog
CLEAR                          -> OK     All off
```

## LED States

| State        | Color      | Animation              | Meaning                |
|--------------|------------|------------------------|------------------------|
| KNIGHT_RIDER | Red        | KITT scanner (fast)    | Claude thinking/working|
| IDLE         | Blue       | Soft breathing         | System ready           |
| WAITING      | Amber      | Pulsing                | Input needed           |
| ERROR        | Red        | Fast breathing         | Error occurred         |
| DONE         | Green      | Flash + breathing      | Task completed (auto→IDLE after 3s) |
| PROGRESS     | Cyan       | Progress bar           | Long running task      |

## Claude Code Hook Mapping

| Hook                | Event          | LED State    |
|---------------------|----------------|--------------|
| UserPromptSubmit    | thinking       | KNIGHT_RIDER |
| PreToolUse          | tool_pending   | WAITING      |
| PostToolUse         | tool_running   | KNIGHT_RIDER |
| PostToolUseFailure  | error          | ERROR        |
| Stop                | task_done      | DONE → IDLE  |
| Notification(idle)  | idle           | IDLE         |
| Notification(perm)  | waiting        | WAITING      |
| SessionStart        | session_start  | CONNECT      |

## Conventions
- Language: English
- Framework: Arduino + Adafruit NeoPixel (firmware)
- Python: pyserial, no other dependencies
- Tests: pytest
- Daemon communicates via TCP socket on 127.0.0.1:17177
- Port detection: PING/PONG protocol scan (not CH340 name matching)
