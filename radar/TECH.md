# Technical Architecture — Arduino Radar System v2

## System Overview
A servo-mounted ultrasonic sensor sweeps 150° (15°–165°), measuring distances at each degree. Data is streamed to a Python matplotlib radar display. A buzzer and LED alert on any obstacle detection. A 16×2 LCD shows the current angle and distance locally.

## Pin Reference (Arduino Uno)
| Component | Pin(s) | Notes |
|---|---|---|
| HC-SR04 Trig | 10 | Ultrasonic trigger |
| HC-SR04 Echo | 11 | Ultrasonic response |
| Servo Signal | 9 | PWM sweep control |
| LCD RS/E | 2, 3 | Control |
| LCD D4–D7 | 4, 5, 6, 7 | Data (4-bit mode) |
| Buzzer | 8 | Alert tone |
| Red LED | A5 | Via 220Ω resistor |

## Serial Protocol
- **Baud:** 9600
- **Format:** `angle,distance\n` (e.g. `90,25`)

## Alert Logic
Any valid echo (distance > 0 and < 400cm) triggers buzzer + LED.

## Software Stack
- **Arduino:** `Servo.h`, `LiquidCrystal.h`
- **Python:** `serial`, `matplotlib`, `numpy`
