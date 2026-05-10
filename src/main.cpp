#include <Arduino.h>
#include <WiFi.h>
#include <ESP32Servo.h>
#include <DecentIoT.h>
#include <cmath>

// ==========================
// User configuration
// ==========================
#define WIFI_SSID "Wokwi-GUEST"
#define WIFI_PASS ""

#define MQTT_BROKER "9f5be305ec5d42c3b3e6801185bb173f.s1.eu.hivemq.cloud"
#define MQTT_PORT 8883
#define MQTT_USERNAME "Testuser123"
#define MQTT_PASSWORD "Testuser123"
#define PROJECT_ID "69f87f22b8a662cdfcdb006b"
#define USER_ID "68c3b91037e678f91fbb6d2b"
#define DEVICE_ID "69f87ffab8a662cdfcdb0091"

// ==========================
// Hardware pins (align with diagram.json)
// ==========================
#define ULTRASONIC_TRIG_PIN 33
#define ULTRASONIC_ECHO_PIN 34
#define RED_LED_PIN         12
#define GREEN_LED_PIN       14
#define SERVO_PUMP_PIN      18

// Auto-mode pump hysteresis (cm echo distance: high = low water, low = high water)
#define PUMP_ON_DISTANCE_CM   100.0f
#define PUMP_OFF_DISTANCE_CM  60.0f

// Tank % mapping: near echo = full tank, far echo = empty (HC-SR04 ~2–400 cm typical)
#define TANK_DIST_FULL_CM   2.0f
#define TANK_DIST_EMPTY_CM  400.0f

Servo gateServo;

bool motorRunning = false;
bool autoModeEnabled = true;
bool manualPumpOn = false;
bool autoPumpDemand = false;
float latestDistance = 0.0f;
float latestFillPercent = 0.0f;
int servoAngle = 0;
int sweepStep = 2;
unsigned long lastServoStepAt = 0;
const unsigned long servoStepIntervalMs = 20;

float waterLevelPercent(float distanceCm) {
  if (distanceCm <= 0.0f || distanceCm > TANK_DIST_EMPTY_CM * 1.1f) {
    return NAN;
  }
  if (distanceCm <= TANK_DIST_FULL_CM) {
    return 100.0f;
  }
  if (distanceCm >= TANK_DIST_EMPTY_CM) {
    return 0.0f;
  }
  const float span = TANK_DIST_EMPTY_CM - TANK_DIST_FULL_CM;
  return 100.0f * (TANK_DIST_EMPTY_CM - distanceCm) / span;
}

void applyOutputs() {
  if (motorRunning) {
    digitalWrite(RED_LED_PIN, HIGH);
    digitalWrite(GREEN_LED_PIN, LOW);
  } else {
    servoAngle = 0;
    gateServo.write(servoAngle);
    digitalWrite(RED_LED_PIN, LOW);
    digitalWrite(GREEN_LED_PIN, HIGH);
  }
}

void updateServoMotion() {
  if (!motorRunning) {
    return;
  }

  if (millis() - lastServoStepAt < servoStepIntervalMs) {
    return;
  }
  lastServoStepAt = millis();

  servoAngle += sweepStep;
  if (servoAngle >= 180 || servoAngle <= 0) {
    sweepStep = -sweepStep;
    servoAngle = constrain(servoAngle, 0, 180);
  }
  gateServo.write(servoAngle);
}

void evaluateMotorState() {
  motorRunning = autoModeEnabled ? autoPumpDemand : manualPumpOn;
  applyOutputs();
}

void updateAutoDemandFromDistance(float distanceCm) {
  if (distanceCm > PUMP_ON_DISTANCE_CM) {
    autoPumpDemand = true;
  } else if (distanceCm < PUMP_OFF_DISTANCE_CM) {
    autoPumpDemand = false;
  }
}

void readUltrasonicAndUpdateControl() {
  digitalWrite(ULTRASONIC_TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(ULTRASONIC_TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(ULTRASONIC_TRIG_PIN, LOW);

  const long duration = pulseIn(ULTRASONIC_ECHO_PIN, HIGH, 30000);
  latestDistance = duration * 0.034f / 2.0f;
  latestFillPercent = waterLevelPercent(latestDistance);

  updateAutoDemandFromDistance(latestDistance);
  evaluateMotorState();

  Serial.print("[Tank] raw ");
  Serial.print(latestDistance, 2);
  Serial.print(" cm | fill ");
  if (std::isnan(latestFillPercent)) {
    Serial.println("% (invalid)");
  } else {
    Serial.print(latestFillPercent, 1);
    Serial.println(" %");
  }
}

DECENTIOT_RECEIVE(P1) {
  manualPumpOn = static_cast<bool>(value);
  evaluateMotorState();

  Serial.print("[DecentIoT] Manual pump command ");
  Serial.println(manualPumpOn ? "ON" : "OFF");
}

DECENTIOT_RECEIVE(P2) {
  autoModeEnabled = static_cast<bool>(value);
  evaluateMotorState();

  Serial.print("[DecentIoT] Auto mode ");
  Serial.println(autoModeEnabled ? "ON" : "OFF");
}

DECENTIOT_SEND(P0, 1000) {
  readUltrasonicAndUpdateControl();

  if (!std::isnan(latestFillPercent)) {
    DecentIoT.write(P0, latestFillPercent);
    Serial.printf("[P0] Published level = %.1f %%\n", latestFillPercent);
  }
}

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  Serial.print("[WiFi] Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n[WiFi] Connected");
  Serial.print("[WiFi] IP: ");
  Serial.println(WiFi.localIP());
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n--- Initializing DecentIoT Motor Controller ---");

  pinMode(ULTRASONIC_TRIG_PIN, OUTPUT);
  pinMode(ULTRASONIC_ECHO_PIN, INPUT);
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);

  ESP32PWM::allocateTimer(0);
  gateServo.setPeriodHertz(50);
  gateServo.attach(SERVO_PUMP_PIN, 500, 2400);

  applyOutputs();

  connectWiFi();
  DecentIoT.begin(MQTT_BROKER, MQTT_PORT, MQTT_USERNAME, MQTT_PASSWORD, PROJECT_ID, USER_ID, DEVICE_ID);

  Serial.println("[SmartTank] Started");
}

void loop() {
  DecentIoT.run();
  updateServoMotion();
  delay(10);
}
