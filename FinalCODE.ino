    #include <Wire.h>
    #include <SPI.h>
    #include <LoRa.h>
    #include <Adafruit_BMP280.h>
    #include <ESP32Servo.h>

    // -------------------------------------------------------------------
    //  I²C (BMP280)
    // -------------------------------------------------------------------
    #define BMP_SDA      21
    #define BMP_SCL      22
    #define BMP_ADDR1    0x76   // SDO = GND
    #define BMP_ADDR2    0x77   // SDO = VCC

    Adafruit_BMP280 bmp;
    bool bmpReady = false;

    // -------------------------------------------------------------------
    //  SPI (LoRa RA‑02)
    // -------------------------------------------------------------------
    #define LORA_MOSI    23
    #define LORA_MISO    19
    #define LORA_SCK     18
    #define LORA_SS      5
    #define LORA_RST     14
    #define LORA_DIO0    26
    #define LORA_FREQ    434500000UL   // 434.5 MHz

    // -------------------------------------------------------------------
    //  Servo (SG90 – parachute)
    // -------------------------------------------------------------------
    #define SERVO_SG_PIN    25   // SG90 – parachute (Pin 25)
    Servo servoSG;
    bool sgDeployed = false;

    // -------------------------------------------------------------------
    //  Launch‑ready detector (GPIO13 <-> GPIO4)
    // -------------------------------------------------------------------
    #define TRIG_OUT_PIN    13   // OUTPUT – forced HIGH
    #define TRIG_IN_PIN     4    // INPUT  – pull‑down, reads HIGH only while short present

    bool launchReady   = false;        // becomes true after short removal
    unsigned long startTime = 0;       // timer base
    bool prevTrigState = true;         // assume short present at boot

    // -------------------------------------------------------------------
    //  BMP280 helper – try both possible I²C addresses
    // -------------------------------------------------------------------
    bool initBMP280() {
      Serial.print("BMP280 init … ");
      if (bmp.begin(BMP_ADDR1)) {
        Serial.println("found at 0x76");
        return true;
      }
      if (bmp.begin(BMP_ADDR2)) {
        Serial.println("found at 0x77");
        return true;
      }
      Serial.println("NOT FOUND!");
      return false;
    }

    // -------------------------------------------------------------------
    //  Setup
    // -------------------------------------------------------------------
    void setup() {
      Serial.begin(115200);
      delay(2000);   // give the Serial Monitor time to attach

      Serial.println("\n=== POWER‑UP DETECTED ===");
      Serial.println("Waiting for launch‑ready **removal** of short (GPIO13 <-> GPIO4)…");

      // ---------- Launch‑ready pins ----------
      pinMode(TRIG_OUT_PIN, OUTPUT);
      digitalWrite(TRIG_OUT_PIN, HIGH);          // keep HIGH forever
      pinMode(TRIG_IN_PIN, INPUT_PULLDOWN);      // LOW by default, HIGH only while jumper present

      // ---------- BMP280 ----------
      Wire.begin(BMP_SDA, BMP_SCL);
      bmpReady = initBMP280();

      // ---------- Servo ----------
      ESP32PWM::allocateTimer(0);
      servoSG.setPeriodHertz(50);
      servoSG.attach(SERVO_SG_PIN, 500, 2400);
      delay(10);
      servoSG.write(0);      // start CLOSED (0°)
      Serial.println("SG90 attached – forced to CLOSED (0°).");

      // ---------- LoRa ----------
      SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_SS);
      LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);

      // Optional reset pulse
      pinMode(LORA_RST, OUTPUT);
      digitalWrite(LORA_RST, LOW);
      delay(10);
      digitalWrite(LORA_RST, HIGH);
      delay(10);

      Serial.print("Starting LoRa @ ");
      Serial.print(LORA_FREQ / 1e6, 1);
      Serial.println(" MHz … ");

      if (!LoRa.begin(LORA_FREQ)) {
        Serial.println("❌ LoRa.begin() FAILED – check wiring, antenna, and 3.3 V supply!");
      } else {
        Serial.println("✅ LoRa init OK");
        LoRa.setSignalBandwidth(125E3);
        LoRa.setSpreadingFactor(7);
        LoRa.setCodingRate4(5);
        LoRa.setPreambleLength(8);
        LoRa.enableCrc();
      }

      // ---------- Summary ----------
      Serial.println("\n=== SETTINGS ===");
      Serial.println("Frequency  : 434.5 MHz");
      Serial.println("Servo SG90 : GPIO25");
      Serial.println("Launch pins: OUT 13 <-> IN 4");
      Serial.println("================\n");
    }

    // -------------------------------------------------------------------
    //  Loop
    // -------------------------------------------------------------------
    void loop() {
      // ---------------------------------------------------------------
      //  0️⃣ Detect removal of the short (HIGH → LOW transition)
      // ---------------------------------------------------------------
      bool curTrigState = digitalRead(TRIG_IN_PIN);   // HIGH while short present, LOW after removal
      if (!launchReady && prevTrigState && !curTrigState) {
        launchReady = true;
        startTime   = millis();                     // start counting for servo
        Serial.println("\n>>> LAUNCH‑READY DETECTED (short removed) – TIMER STARTED <<<\n");
      }
      prevTrigState = curTrigState;                 // remember for next loop

      // ---------- BMP280 reading ----------
      float temperatureC = 0.0, pressurePa = 0.0, altitudeM = 0.0;
      static float seaLevelRef = 0.0;
      if (bmpReady) {
        temperatureC = bmp.readTemperature();
        pressurePa   = bmp.readPressure();
        if (seaLevelRef == 0.0) seaLevelRef = pressurePa;
        altitudeM    = bmp.readAltitude(seaLevelRef / 100.0);
      }

      // ---------- Telemetry ----------
      Serial.print("T: "); Serial.print(temperatureC, 1);
      Serial.print("°C  P: "); Serial.print(pressurePa / 100.0, 1);
      Serial.print("hPa  Alt: "); Serial.print(altitudeM, 1);
      Serial.print(" m  LaunchReady: "); Serial.println(launchReady ? "YES" : "NO");

      // ---------- LoRa payload (altitude) ----------
      static uint16_t packetSeq = 0;
      packetSeq++;
      uint16_t altitude10 = (uint16_t)constrain(round(altitudeM * 10.0), 0, 65535);
      uint8_t binPayload[6] = {
        1,                       // ROCKET_ID
        0x01,                    // TYPE = altitude
        (packetSeq >> 8) & 0xFF, // seq high byte
        packetSeq & 0xFF,        // seq low  byte
        (altitude10 >> 8) & 0xFF,// altitude high byte
        altitude10 & 0xFF        // altitude low  byte
      };
      LoRa.beginPacket();
      LoRa.write(binPayload, sizeof(binPayload));
      LoRa.endPacket();

      // ---------- SG90 deployment (3 s after launchReady) ----------
      if (launchReady && !sgDeployed) {
        unsigned long now = millis() - startTime;
        if (now >= 3000) { // 3 seconds after removing the 13<->4 wire
          servoSG.write(90);               // open ~90°
          sgDeployed = true;
          Serial.println(">>> SG90 DEPLOYED (3 s) <<<");
        }
      }

      // Keep loop running at ~2 Hz
      delay(500);
    }