# technical architecture: Dual Ultrasonic Speed Trap

This document details the firmware state machine and the Python visualization UI for the dual ultrasonic speed trap.

## 1. Firmware Math & Logic (`main.ino`)

The firmware avoids using `delay()` loops for checking the sensors and instead relies on a state machine to track the passing object logically.

### The Math
The core physics calculation runs on the basic kinematic equation:
$$v = \frac{\Delta d}{\Delta t}$$

In our C++ implementation:
```cpp
elapsedTime = (time2 - time1) / 1000.0; // millis to seconds
passingSpeed = SENSOR_DISTANCE_METERS / elapsedTime; // m/s
```
*Note: We assume constant velocity between the two sensors for this calculation to hold true.*

### The State Machine
The system operates across three distinct states:
1. `WAITING`: The system continuously pings **only** Sensor 1. Once an object violates the `TRIGGER_THRESHOLD` (e.g. gets closer than 15cm), the system records `time1` and transitions to the `TIMING` state.
2. `TIMING`: The system stops checking Sensor 1 and now aggressively pings **only** Sensor 2. Once the object sweeps past Sensor 2, it records `time2`.
3. `FINISHED`: The system computes the difference, pushes it to the LCD and Serial bus, and enters a 3-second lockout to prevent double-triggering before returning to `WAITING`.

### Pin Assignments
| Component  | Pins                 | Function   |
|------------|----------------------|------------|
| SENSOR 1   | 3, 4                 | Entry Gate |
| SENSOR 2   | 5, 6                 | Exit Gate  |
| BUZZER     | A0                   | Alert      |
| LED        | A1                   | Alert      |
| LCD        | 7, 8, 9, 10, 11, 12  | Status     |

## 2. Professional Implementation (Pro Edition)

To handle high-speed objects (toy cars, fans, projectiles), the system has been upgraded to a **Hardware Interrupt Architecture**.

### A. Firmware (AVR Interrupts)
Instead of standard Arduino polling, we use **Direct Register Access** and **External Interrupts (INT0 and INT1)** on the ATMega328P (Nano/Uno).
- **Pin 2 (INT0)**: Echo from Sensor 1. Triggers the start of the timing window.
- **Pin 3 (INT1)**: Echo from Sensor 2. Triggers the end of the timing window.
- **Direct Port Access**: We use `PIND` registers for sub-microsecond state checking, bypassing the slower `digitalRead()` overhead.

### B. Synchronization (Eventlet & WebSocket)
To solve the "Real-Time Website" issue, the Python backend uses an **asynchronous event loop** (via `eventlet`).
- **Monkey Patching**: The server patches standard libraries to be non-blocking.
- **Green Threads**: The Serial reader runs in a dedicated lightweight thread, pushing data to the browser the microsecond it arrives.
- **115200 Baud**: The communication speed between Arduino and PC has been increased to reduce telemetry latency.

### C. Web Visualization
The dashboard is built using **Socket.io**.
- When `P1_CROSSED` is received, the CSS Animation starts.
- When `P2_CROSSED` is received, the animation instantly "snaps" to the physical sensor position and displays the calculated speed.
