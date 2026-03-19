# Technical Architecture — Smart Door Lock v2

## 🏗️ System Overview
The system uses an **Arduino Mega 2560** as the central security controller, managing three authentication inputs and driving a servo-based locking bolt. A Python backend bridges the hardware to a web dashboard via WebSockets.

## 🔐 Security Logic
1. **RFID Scanning**: Uses the MFRC522 module (SPI). The scanned UID is compared against a hardcoded Master UID. The antenna gain is maximized for better read range.
2. **Keypad PIN**: A state-based buffer collects digits. `#` commits the entry; `*` clears it. A 10-second inactivity timeout auto-clears the buffer to prevent leftover input from interfering.
3. **IR Remote**: Decodes hex signals via IRremote v4.x. Specific codes trigger `grantAccess()` or `manualRelock()`.
4. **Servo Bolt Mechanism**: On access grant, the servo writes to `OPEN_ANGLE` (90°). After 5 seconds (non-blocking `millis()` timer), it returns to `LOCKED_ANGLE` (0°). The `manualRelock()` function overrides this at any time.

## 📟 LCD Feedback (16×2 Parallel)
The LCD gives local real-time status at every stage:

| Event | Line 1 | Line 2 |
|---|---|---|
| Boot | `SMART LOCK v2` | `INITIALIZING...` |
| Idle / Locked | `** LOCKED **` | `Scan / PIN / Web` |
| PIN entry | `Enter PIN:` | `****_` |
| Access Granted | `ACCESS GRANTED` | Method (e.g. `RFID`) |
| Door Open | `** OPEN **` | `Auto-lock: 5s` |
| Access Denied | `ACCESS DENIED` | Method + ` rejected` |
| Auto-locking | `AUTO LOCKING` | `Please wait...` |
| Manual lock | `MANUAL LOCK` | `Locking...` |

## 📡 Communication Protocol
- **Baud Rate**: 115200
- **Arduino → Python (Serial Telemetry)**:
  - `STATE:LOCKED` / `STATE:OPEN` / `STATE:LOCKING` / `STATE:LOCKING_MANUAL`
  - `STATE:ACCESS_GRANTED_BY_[METHOD]` / `STATE:ACCESS_DENIED`
  - `RFID_SCANNED:[UID]` / `PIN_ENTERED:[PIN]` / `KEY_PRESSED:[KEY]`
  - `IR_RAW:[HEX]` / `INFO:[MSG]` / `WARNING:[MSG]`
- **Python → Arduino (Commands)**:
  - `UNLOCK_CMD\n` — triggers `grantAccess("WEB")`
  - `LOCK_CMD\n` — triggers `manualRelock()`

## 🌐 Dashboard (Python + Flask-SocketIO)
- **Eventlet Async**: Serial reader runs in a greenthread; never blocks the web server.
- **Thread-Safe Writes**: A `threading.Lock()` prevents simultaneous serial writes from crashing the Eventlet hub.
- **Live Logging**: Every telemetry tag is forwarded to the browser log panel in real time.
- **Remote Override**: Web dashboard sends `UNLOCK_CMD` / `LOCK_CMD` back to hardware.

## 📌 Pin Reference (Quick View)
| Component | Pins |
|---|---|
| RFID (SPI) | 50, 51, 52, 53, RST=5 |
| Keypad Rows | 30, 31, 32, 33 |
| Keypad Cols | 34, 35, 36, 37 |
| Servo Signal | 9 |
| LCD (RS,E,D4-D7) | 38, 39, 40, 41, 42, 43 |
| IR Receiver | 2 (INT0) | Moved for stability |
| Buzzer / LED | 10, 11 |
