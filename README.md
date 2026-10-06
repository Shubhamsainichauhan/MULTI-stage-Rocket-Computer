# MULTI-stage-Rocket-Computer

Flight computer software for a multi-stage water rocket, powered by an ESP32.

## Hardware Components
* **ESP32** (Microcontroller)
* **LoRa RA-02** (Telemetry Radio - 434.5 MHz)
* **BMP280** (Barometric Pressure / Altitude Sensor)
* **SG90 Servo** (Parachute Deployment)

## Wiring Connections

### 1. Parachute Servo (SG90)
* **Signal (Orange/Yellow):** GPIO 25
* **VCC (Red):** 5V / VIN (Requires robust power, do NOT use 3.3V)
* **GND (Brown/Black):** GND

### 2. Barometer (BMP280) - I2C
* **SDA:** GPIO 21
* **SCL:** GPIO 22
* **VCC:** 3.3V
* **GND:** GND

### 3. Telemetry Radio (LoRa RA-02) - SPI
* **MOSI:** GPIO 23
* **MISO:** GPIO 19
* **SCK:** GPIO 18
* **NSS / SS:** GPIO 5
* **RST:** GPIO 14
* **DIO0:** GPIO 26
* **VCC:** 3.3V
* **GND:** GND

### 4. Launch Ready Jumper
* **OUT Pin:** GPIO 13
* **IN Pin:** GPIO 4
* *How it works:* Short these two pins together before powering on. The system will wait for the connection to be broken (jumper pulled) to detect "Launch", which starts the 3-second deployment timer.

## How to Operate
1. Securely connect the GPIO 13 to GPIO 4 jumper wire.
2. Power on the ESP32.
3. The parachute servo will automatically move to its closed position (0 degrees).
4. The system will initialize the BMP280 and LoRa module.
5. Pull the jumper wire to trigger the launch sequence.
6. The ESP32 starts transmitting altitude telemetry over LoRa at ~2Hz.
7. Exactly 3 seconds after pulling the jumper, the parachute servo will deploy (90 degrees).
