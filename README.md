# DecentIoT — Smart Water Tank Controller

Single-node firmware for an **ESP32** that reads tank water level with an **HC-SR04** ultrasonic sensor, publishes level to **[DecentIoT](https://docs.decentiot.cloud)** over MQTT, and controls a **simulated pump** (servo) plus **status LEDs**. Designed for coursework, labs, and Wokwi simulation.

---

## What this project demonstrates

1. Reading distance with an ultrasonic sensor (trigger / echo pins).
2. Converting raw distance into a **tank fill percentage** (configurable calibration).
3. **Auto mode**: pump hysteresis based on measured distance (low water vs high water).
4. **Manual mode**: dashboard control of the pump when auto mode is off.
5. Clean use of **DecentIoT** patterns: `DECENTIOT_RECEIVE` for cloud commands and **`DECENTIOT_SEND` with an interval** for periodic telemetry (same idea as the `LED_DHTsensor` sample project).

---

## Simulation vs real hardware

| In the sketch / simulation | Meaning |
|---------------------------|--------|
| HC-SR04 | Measures distance from sensor to water surface |
| Larger distance | Water surface farther away → tank **emptier** |
| Smaller distance | Water surface closer → tank **fuller** |
| Servo | Stands in for **pump** (continuous sweep while “running”) |
| Red / green LEDs | Pump / idle **indicators** |

Your `diagram.json` matches the GPIO numbers used in firmware (see pin table below).

---

## Circuit Diagram Image


![Circuit Diagram](/images/circuit_diagram.png)


## Wiring (diagram summary)

Pins are defined in `src/main.cpp` and should match **`diagram.json`**:

| Signal | ESP32 GPIO | Notes |
|--------|------------|--------|
| Ultrasonic TRIG | 33 | Output |
| Ultrasonic ECHO | 34 | Input only safe pin on many ESP32 modules |
| Red LED (via resistor) | 12 | |
| Green LED (via resistor) | 14 | |
| Servo signal | 18 | 5 V / GND as in diagram |

---

## DecentIoT — virtual pins (dashboard)

Configure matching **datastreams / widgets** in the DecentIoT web app for this device:

| Pin | Direction | Purpose |
|-----|-----------|---------|
| **P0** | Device → cloud | Tank **fill %** (0–100), published on an interval via `DECENTIOT_SEND` |
| **P1** | Cloud → device | **Manual pump** on/off (`true`/`false`) when auto mode is off |
| **P2** | Cloud → device (and echoed from device) | **Auto mode** on/off |

**Behavior:**

- **Auto mode ON (`P2`)**: pump follows sensor rules (distance thresholds with hysteresis in code).
- **Auto mode OFF (`P2`)**: pump follows **`P1`** manual command only.

The firmware also calls `publishStatus` with `"motor_on"` / `"motor_off"` on each P0 send tick so the dashboard can show activity.

---

## Prerequisites

### Required

- **[PlatformIO](https://platformio.org/)** (VS Code extension or CLI)
- **[DecentIoT](https://docs.decentiot.cloud)** account: project, device, MQTT credentials, and datastreams aligned with **P0 / P1 / P2**

### Optional (simulation)

- **[Wokwi for VS Code](https://marketplace.visualstudio.com/items?itemName=wokwi.wokwi-vscode)**
- **`diagram.json`** and **`wokwi.toml`** in this folder (already included)

---

## Quick start — configure firmware

Open **`src/main.cpp`** and edit the block at the top:

1. **WiFi**: `WIFI_SSID`, `WIFI_PASS` — use your network (e.g. Wokwi’s `Wokwi-GUEST` for simulation).
2. **MQTT & DecentIoT IDs**: Replace with values from your DecentIoT project:
   - `MQTT_BROKER`, `MQTT_PORT` (typically **8883** for TLS)
   - `MQTT_USERNAME`, `MQTT_PASSWORD`
   - `PROJECT_ID`, `USER_ID`, `DEVICE_ID`

3. **Tank calibration** (optional tuning):
   - `TANK_DIST_FULL_CM` — expected reading when the tank is **full** (surface near sensor).
   - `TANK_DIST_EMPTY_CM` — upper range used for **0 %** (often near sensor max range, e.g. 400 cm for HC-SR04 class devices).
   - `PUMP_ON_DISTANCE_CM` / `PUMP_OFF_DISTANCE_CM` — hysteresis thresholds for auto pump (distance in cm).

Never commit real passwords or API secrets to **public** repositories. Prefer environment-specific config or `.gitignore` for local overrides if you extend the project later.

---

## Build and upload (hardware)

From this project directory:

```bash
pio run -e esp32dev
```

Upload (connect ESP32 via USB):

```bash
pio run -e esp32dev -t upload
```

Serial monitor (**115200** baud):

```bash
pio device monitor -b 115200
```

---

## Wokwi simulation (VS Code)

1. Install the Wokwi extension and sign in if required.
2. Build the firmware so binaries exist:

   ```bash
   pio run -e esp32dev
   ```

3. **`wokwi.toml`** points to:

   - `firmware = ".pio/build/esp32dev/firmware.bin"`
   - `elf = ".pio/build/esp32dev/firmware.elf"`

4. Start the simulator from the **`diagram.json`** (or use Command Palette → Wokwi: Start Simulator).
5. Open the serial output in the simulator; you should see WiFi connection, MQTT, and periodic `[Tank]` / `[P0]` lines once the simulated sensor runs.

---

## Code structure (for students)

| Area | Role |
|------|------|
| `DECENTIOT_SEND(P0, 1000)` | Every **1 second**: read sensor → update auto logic → `DecentIoT.write(P0, …)` and status |
| `readUltrasonicAndUpdateControl()` | One ultrasonic **pulse + echo**, convert to cm and **%**, apply auto pump hysteresis and LED/servo outputs |
| `DECENTIOT_RECEIVE(P1)` / `(P2)` | React to dashboard switches |
| `updateServoMotion()` | Animates servo while pump is “on” |

`readUltrasonicAndUpdateControl()` is not redundant: it is the **single place** that performs one measurement cycle and refreshes outputs before telemetry is published. Keeping it separate avoids duplicating pulse code if you later add more send handlers.

---

## Dependencies (`platformio.ini`)

- **[DecentIoT MQTT](https://github.com/DecentIoT/DecentIoT_mqtt_Lib)** — MQTT helpers, macros, TLS
- **ESP32Servo** — servo driving on ESP32  
- HC-SR04 library is listed in `lib_deps` for compatibility/tooling; the reference sketch uses **`pulseIn`** directly for the ultrasonic module.

---

## Troubleshooting

| Issue | Things to check |
|--------|----------------|
| Blank serial monitor | Correct **COM port**, baud **115200**, USB cable supports data |
| MQTT never connects | Broker host, port **8883**, username/password; device clock (NTP) may be required for TLS — see serial logs |
| Dashboard switches do nothing | **P1/P2** datastreams bound to correct device ID; MQTT connected (`DecentIoT.run()` in loop) |
| Wokwi: no firmware | Run `pio run` first so `.pio/build/esp32dev/` exists |
| Strange % readings | Tune `TANK_DIST_FULL_CM` / `TANK_DIST_EMPTY_CM` to your mounting height |

---

## Project layout

```text
DecentIoT- Motor Controller/
├── README.md           ← this file
├── platformio.ini      ← board + libraries
├── wokwi.toml          ← simulator firmware paths (forward slashes OK)
├── diagram.json        ← Wokwi circuit
├── src/
│   └── main.cpp        ← application — start here + edit credentials
├── include/
└── lib/
```

---

## License and credits

- **DecentIoT MQTT library**: see the upstream repository ([DecentIoT_mqtt_Lib](https://github.com/DecentIoT/DecentIoT_mqtt_Lib)) for licensing.
- Simulation diagram uses **[Wokwi](https://wokwi.com/)** conventions.

---

## Suggested coursework extensions

1. Replace servo sweep with fixed angle or PWM motor driver abstraction.
2. Add a second **`DECENTIOT_SEND`** for raw distance (cm) on **P3** while **P0** stays as percent only.
3. Add Wi-Fi reconnect logic like in `LED_DHTsensor`.
4. Log fill level to CSV or MQTT topic for a simple “analytics” demo.

Questions about DecentIoT setup should follow the official docs: **[https://docs.decentiot.cloud](https://docs.decentiot.cloud)**.
