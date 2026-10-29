# Desk Overlord

A creature on your desk that turns, flashes and beeps when your laptop is on a distraction, and calms down when you get back to work. A Python script on the laptop watches the active window; an Arduino Uno is the creature's body.

Built for a hackathon (playbook dated Oct 24, 2026).

## How it works

1. `overlord.py` reads the title of the front window twice a second (Windows only) and checks it against a keyword list (YouTube, Reddit, Netflix, ...).
2. An anger score from 0 to 100 rises while you are distracted and falls while you work. Thresholds turn it into four moods: calm, watching, angry, furious.
3. The laptop sends one line over USB, such as `2|YOUTUBE AGAIN?`. The Arduino turns the servo head, sets the RGB eyes, drives the buzzer and writes the LCD.
4. The creature's button sends `P` back, which grants a short truce.
5. If the laptop goes quiet for 5 seconds, the creature shows `NO SIGNAL` and glows blue.

Everything runs on the laptop. Nothing goes to the internet, and window titles are never saved. The session report (`session.csv` and `session_report.png`) records only time, anger score and a yes/no per check.

## Repo layout

| Path | What it is |
| --- | --- |
| `overlord.py` | Laptop side: window watcher, anger score, serial link, session report |
| `arduino/overlord/overlord.ino` | Creature firmware (upload this for the real thing) |
| `arduino/overlord_selftest/overlord_selftest.ino` | Runs servo, RGB, buzzer and button one at a time, to check wiring |
| `docs/Desk_Overlord_Hackathon_Playbook.pdf` | Full plan: parts, wiring, schedule, troubleshooting, demo script, judge Q&A |
| `docs/Claude_API_Integration_Plan.pdf` | Optional plan to add Claude as a task-aware focus judge. **Not merged into `overlord.py`** |

## Quick start

Hardware: Arduino Uno, SG90 servo, RGB LED, active buzzer, push button, LCD1602 with a 10K pot, 220 ohm resistors. Pin map is in the playbook; in short, servo D9, RGB D3/D5/D6, buzzer D10, button A0, LCD on D12/D11/D8/D7/D4/D2.

1. Wire the parts one stage at a time, uploading `overlord_selftest` after each stage.
2. Upload `arduino/overlord/overlord.ino` from the Arduino IDE.
3. Close Serial Monitor (only one program can hold the port), then on the laptop:

```powershell
py -m pip install pyserial matplotlib
py overlord.py                 # finds the Arduino by itself
py overlord.py --port COM5     # if it cannot find it
py overlord.py --dry-run       # no Arduino: prints what it would send (and window titles)
py overlord.py --real          # slow, realistic timing (demo timing is the default)
```

Press Ctrl+C to stop; it saves the session report in the current folder.

To test the creature without Python, open Serial Monitor at 9600 baud with the line ending set to Newline and send `2|HELLO`.

## Tuning

- `overlord.py`: `DISTRACTIONS` (keywords), `LEVEL_AT`, `DEMO_TIMING` / `REAL_TIMING`, and the taunts in `lcd_text` (16 ASCII characters max).
- `overlord.ino`: the Tuning block at the top (`ANGLE_*`, `SOUND_ON`, `COMMON_ANODE`).

## Limits

It only sees window titles, not page content, so a keyword can miss a site or hit the wrong window. It is rule-based on purpose: explainable and works offline. It has not been tested for whether it actually improves focus.
