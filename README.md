# 6WD Face-Tracking Robot

An ESP32-controlled 6-wheel-drive robot with two BTS7960 motor drivers, controllable over WiFi via a web dashboard, with an optional two-phone face-tracking system for autonomous following (e.g. award-distribution / chief-guest tracking use case).

## Features

- **6WD drivetrain** — 3 motors per side, driven by 2× BTS7960 H-bridge drivers
- **Web control dashboard** — served directly from the ESP32, works on any phone browser
- **Smooth acceleration ramp** — prevents tipping on a tall/heavy chassis
- **Active braking** — reverse-pulse braking to stop precisely instead of coasting
- **Independent turn speed** — separate speed setting for turning vs forward/backward
- **Two-phone camera relay** — mount one phone as a camera "eye" on the robot, control everything from a second phone

## Repository structure

```
firmware/
  esp32_6wd_car.ino       Arduino sketch — motor control + web server
web/
  robot_phone.html         Phone A — camera-only relay (mount on robot)
  controller_phone.html    Phone B — receives camera feed, sends commands
docs/
  appendix_b_circuit_schematic.png   Full wiring diagram
```

## Hardware

| Component | Qty |
|---|---|
| ESP32 Dev Module | 1 |
| BTS7960 motor driver | 2 |
| DC gear motors | 6 |
| Battery (motor power) | 1 |
| Android phone (camera + control) | 2 (optional, for face tracking) |

See [`docs/appendix_b_circuit_schematic.png`](docs/appendix_b_circuit_schematic.png) for full wiring.

### Pin map

| Signal | GPIO |
|---|---|
| Left RPWM | 25 |
| Left LPWM | 26 |
| Left R_EN | 18 |
| Left L_EN | 21 |
| Right RPWM | 27 |
| Right LPWM | 14 |
| Right R_EN | 13 |
| Right L_EN | 12 |

## Getting started

### 1. Flash the ESP32

1. Open `firmware/esp32_6wd_car.ino` in Arduino IDE
2. Set your WiFi credentials:
   ```cpp
   const char* ssid     = "your_wifi_name";
   const char* password = "your_wifi_password";
   ```
3. Board: **ESP32 Dev Module**
4. Upload, then open the Serial Monitor to see the assigned IP address

### 2. Basic control

Visit `http://<ESP32-IP>/` in any browser on the same network — a touch-friendly dashboard with a D-pad and speed slider will load.

### 3. Two-phone camera + control setup (optional)

1. Host `web/robot_phone.html` and `web/controller_phone.html` (e.g. GitHub Pages)
2. **Phone A** (mount on robot, rear camera facing forward): open `robot_phone.html`, allow camera access, copy the Peer ID shown
3. **Phone B** (in your hand): open `controller_phone.html`, paste the Peer ID → **Connect Cam**, enter the ESP32 IP → **Set IP**, then drive using the D-pad while watching the live feed

> **Note:** Both phones need a WiFi network with internet access for the initial peer handshake (WebRTC signalling). After connecting, video streams directly phone-to-phone.

## HTTP API

| Route | Effect |
|---|---|
| `GET /F` | Move forward |
| `GET /B` | Move backward |
| `GET /L` | Turn left |
| `GET /R` | Turn right |
| `GET /S` | Stop (active brake) |
| `GET /SPD?v=0-255` | Set forward/backward speed |
| `GET /TURNSPD?v=0-255` | Set turning speed |

## Tuning

| Constant | Location | Effect |
|---|---|---|
| `RAMP_STEP` / `RAMP_TICK_MS` | firmware | Acceleration smoothness |
| `BRAKE_PWM` / `BRAKE_MS` | firmware | Braking strength/duration |
| `targetSpeed` / `turnSpeed` | firmware | Default speeds on boot |

## License

MIT — see [LICENSE](LICENSE)
