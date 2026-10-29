"""Desk Overlord - laptop side (Windows).

Reads the title of the window you are looking at, decides whether it is a
distraction, keeps an anger score, and tells the Arduino which mood to show.
Everything runs on this laptop. Nothing goes to the internet.

Setup:         py -m pip install pyserial matplotlib
Test:          py overlord.py --dry-run               (no Arduino needed)
Run:           py overlord.py                         (finds the Arduino by itself)
               py overlord.py --port COM5             (if it cannot find it)
               py overlord.py --real                  (slow, realistic timing)
Quit:          Ctrl+C          (saves session.csv and session_report.png)
"""
import argparse
import csv
import ctypes
import math
import time

try:
    import serial
    from serial.tools import list_ports
except ImportError:
    serial = None


# ---------- Edit these ----------
BAUD = 9600
TICK = 0.5               # seconds between checks

# keyword in the window title (lowercase) -> name shown on the LCD (9 letters max)
DISTRACTIONS = {
    "youtube": "YOUTUBE", "reddit": "REDDIT", "netflix": "NETFLIX",
    "tiktok": "TIKTOK", "instagram": "INSTAGRAM", "twitch": "TWITCH",
    "facebook": "FACEBOOK", "discord": "DISCORD", "9gag": "9GAG",
}

LEVEL_AT = (10, 30, 60)   # anger needed for level 1, 2, 3 (anger tops out at 100)

# (anger gained per second when distracted, lost per second when focused, truce seconds)
DEMO_TIMING = (4.0, 6.0, 15)     # fast: level 3 after 15 s. Good for the video
REAL_TIMING = (1.0, 2.0, 300)    # slow: level 3 after 60 s. For real use


# ---------- Window detection (Windows only, no extra packages) ----------
def active_window_title():
    if not hasattr(ctypes, "windll"):
        return ""
    user32 = ctypes.windll.user32
    hwnd = user32.GetForegroundWindow()
    length = user32.GetWindowTextLengthW(hwnd)
    buf = ctypes.create_unicode_buffer(length + 1)
    user32.GetWindowTextW(hwnd, buf, length + 1)
    return buf.value


def match_distraction(title):
    t = title.lower()
    for keyword, name in DISTRACTIONS.items():
        if keyword in t:
            return name
    return None


# ---------- Mood logic ----------
def level_for(anger):
    return sum(1 for limit in LEVEL_AT if anger >= limit)


def lcd_text(level, site, truce_left):
    """Second LCD line: ASCII only, 16 characters max. Edit the taunts here."""
    if truce_left > 0:
        text = f"TRUCE {truce_left}s"
    elif level == 0:
        text = "GOOD HUMAN"
    elif site is None:
        text = "STILL WATCHING"
    elif level == 1:
        text = f"{site}?"
    elif level == 2:
        text = f"{site} AGAIN?"
    else:
        text = "CLOSE IT. NOW."
    return text.encode("ascii", "ignore").decode()[:16]


# ---------- Serial link ----------
def find_arduino_port():
    for p in list_ports.comports():
        info = f"{p.description} {p.manufacturer}".lower()
        if "arduino" in info or "ch340" in info or "usb serial" in info:
            return p.device
    return None


def open_port(port_arg):
    while True:
        port = find_arduino_port() if port_arg == "auto" else port_arg
        if port:
            try:
                ser = serial.Serial(port, BAUD, timeout=0.05)
                time.sleep(2.5)   # the Uno restarts when the port opens
                print(f"Connected on {port}")
                return ser
            except (serial.SerialException, OSError) as err:
                print(f"Could not open {port}: {err}")
                print("   Close Serial Monitor and any other program using the port.")
        else:
            seen = [f"{p.device} ({p.description})" for p in list_ports.comports()]
            print("No Arduino found. Ports I can see:", seen or "none")
        print("Retrying in 2 s (Ctrl+C to quit)...")
        time.sleep(2)


def exchange(ser, message):
    """Send one line to the Arduino. Returns True if its button was pressed."""
    ser.write(message.encode("ascii"))
    pressed = False
    while ser.in_waiting:
        if ser.readline().decode("ascii", "ignore").strip() == "P":
            pressed = True
    return pressed


# ---------- Session report (titles are never saved) ----------
def save_report(log):
    if len(log) < 2:
        return
    with open("session.csv", "w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(["seconds", "anger", "on_distraction"])
        writer.writerows(log)
    pct = 100.0 * sum(1 for row in log if row[2]) / len(log)
    print(f"Session: {log[-1][0]:.0f} s, {pct:.0f}% of checks were on a distraction. Saved session.csv")
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except ImportError:
        print("For a chart too: py -m pip install matplotlib")
        return
    t = [row[0] for row in log]
    fig, ax = plt.subplots(figsize=(9, 3.5))
    ax.fill_between(t, 0, 100, where=[row[2] for row in log], step="post",
                    alpha=0.2, label="on a distraction")
    ax.plot(t, [row[1] for row in log], linewidth=2, label="Overlord anger")
    ax.set_ylim(0, 100)
    ax.set_xlabel("seconds")
    ax.set_ylabel("anger (0-100)")
    ax.set_title(f"{pct:.0f}% of this session was spent on distractions")
    ax.legend(loc="upper right")
    fig.tight_layout()
    fig.savefig("session_report.png", dpi=150)
    print("Saved session_report.png")


# ---------- Main loop ----------
def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", default="auto", help="e.g. COM5 (default: find it)")
    ap.add_argument("--dry-run", action="store_true",
                    help="no Arduino: print what would be sent (also prints window titles)")
    ap.add_argument("--real", action="store_true", help="slow, realistic timing")
    args = ap.parse_args()
    gain, decay, truce_len = REAL_TIMING if args.real else DEMO_TIMING

    ser = None
    if not args.dry_run:
        if serial is None:
            raise SystemExit("pyserial is missing. Run:   py -m pip install pyserial")
        ser = open_port(args.port)

    anger, truce_until = 0.0, 0.0
    log = []
    start = last = time.time()
    print("Overlord is watching. Press Ctrl+C to stop.")
    try:
        while True:
            now = time.time()
            dt, last = now - last, now
            title = active_window_title()
            site = match_distraction(title)
            distracted = site is not None
            truce_left = max(0, math.ceil(truce_until - now))
            if truce_left > 0:
                anger = 0.0
            else:
                anger += (gain if distracted else -decay) * dt
                anger = max(0.0, min(100.0, anger))
            level = level_for(anger)
            text = lcd_text(level, site, truce_left)

            if ser is None:
                shown = title[:40].encode("ascii", "replace").decode()
                print(f"{now - start:6.1f}s   anger {anger:5.1f}   level {level}          {text:<16}        [{shown}]")
            else:
                try:
                    if exchange(ser, f"{level}|{text}\n"):
                        truce_until = now + truce_len
                        print("Button pressed: truce granted")
                except (serial.SerialException, OSError):
                    print("Lost the Arduino. Reconnecting...")
                    ser = open_port(args.port)

            log.append((round(now - start, 1), round(anger, 1), distracted))
            time.sleep(TICK)
    except KeyboardInterrupt:
        print("\nStopping.")
    finally:
        if ser is not None:
            ser.close()
        save_report(log)


if __name__ == "__main__":
    main()
