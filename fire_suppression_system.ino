#include <Servo.h>
#include <SoftwareSerial.h>

// ================= PINS =================
#define FLAME_FIXED_PIN  2    // fixed flame sensor (wide-area alert)
#define FLAME_SERVO_PIN  3    // flame sensor mounted ON TOP of the servo (points where the nozzle points)
#define SMOKE_PIN        A0
#define BUZZER_PIN       8
#define RELAY_PIN        7
#define SERVO_PIN        9
#define GSM_RX_PIN       10
#define GSM_TX_PIN       11
#define LED_STANDBY_PIN  12
#define LED_ALERT_PIN    13

// ================= SETTINGS =================
// Relay type: true = relay turns ON when pin is LOW (most common blue boards)
const bool RELAY_ACTIVE_LOW = true;
#define RELAY_ON  (RELAY_ACTIVE_LOW ? LOW : HIGH)
#define RELAY_OFF (RELAY_ACTIVE_LOW ? HIGH : LOW)

// Flame sensor output level when it sees fire (KY-026 is normally LOW)
#define FLAME_ACTIVE_STATE LOW

const int SMOKE_THRESHOLD = 300;                 // smoke warning level (see Serial Monitor)
const unsigned long SMOKE_WARMUP_MS = 20000;     // MQ2 warm-up (does NOT block the system)

const unsigned long SENSE_INTERVAL_MS = 20;      // how often sensors are read
const uint8_t FLAME_HITS_NEEDED   = 3;           // consecutive reads to confirm flame (~60 ms)
const uint8_t FLAME_MISSES_NEEDED = 3;           // consecutive reads to confirm flame is gone

const int SCAN_MIN_ANGLE = 20;
const int SCAN_MAX_ANGLE = 160;
const unsigned long SCAN_STEP_MS  = 25;          // scan speed (lower = faster)
const unsigned long ALIGN_STEP_MS = 20;          // speed of the fine-aiming pass

const unsigned long FIRE_OUT_MS       = 1500;    // flame must be gone this long -> pump OFF
const unsigned long MAX_PUMP_RUN_MS   = 60000;   // safety: max pump time in one go
const unsigned long PUMP_COOLDOWN_MS  = 10000;   // pause after hitting the max time

const unsigned long BUZZER_ON_MS  = 200;
const unsigned long BUZZER_OFF_MS = 200;

const unsigned long STATUS_INTERVAL_MS = 1000;   // Serial Monitor refresh (events print instantly)

const unsigned long SMS_MIN_INTERVAL_MS = 60000; // never send SMS more than once a minute
const char* ALERT_PHONE_NUMBER = "+8801XXXXXXXXX";

// ================= GLOBALS =================
Servo nozzleServo;
SoftwareSerial gsm(GSM_RX_PIN, GSM_TX_PIN);

enum SystemState { SCANNING, ALIGNING, FIGHTING };
SystemState state = SCANNING;
SystemState prevState = SCANNING;

uint8_t servoHits = 0, servoMisses = 0;
uint8_t fixedHits = 0, fixedMisses = 0;
bool servoFlame = false, fixedFlame = false;
bool prevServoFlame = false, prevFixedFlame = false;

int  smokeValue = 0;
bool smokeWarning = false;

int scanAngle = SCAN_MIN_ANGLE;
int scanDir = 1;
unsigned long lastServoStep = 0;
unsigned long lastSense = 0;

int alignStartAngle = 0;
int alignDir = 1;

unsigned long fightStartTime = 0;
unsigned long clearStartTime = 0;
unsigned long cooldownUntil = 0;

bool pumpOn = false;
bool buzzerState = false;
unsigned long lastBuzzerToggle = 0;

unsigned long lastStatusPrint = 0;
unsigned long lastSmsTime = 0;
bool smsSentOnce = false;

// ================= HELPERS =================
void updateFlame(bool raw, uint8_t &hits, uint8_t &misses, bool &active) {
  if (raw) {
    if (hits < 255) hits++;
    misses = 0;
    if (hits >= FLAME_HITS_NEEDED) active = true;
  } else {
    if (misses < 255) misses++;
    hits = 0;
    if (misses >= FLAME_MISSES_NEEDED) active = false;
  }
}

void readSensors(unsigned long now) {
  if (now - lastSense < SENSE_INTERVAL_MS) return;
  lastSense = now;
  updateFlame(digitalRead(FLAME_SERVO_PIN) == FLAME_ACTIVE_STATE, servoHits, servoMisses, servoFlame);
  updateFlame(digitalRead(FLAME_FIXED_PIN) == FLAME_ACTIVE_STATE, fixedHits, fixedMisses, fixedFlame);
  smokeValue = analogRead(SMOKE_PIN);
  smokeWarning = (now > SMOKE_WARMUP_MS) && (smokeValue > SMOKE_THRESHOLD);
}

void setPump(bool on) {
  pumpOn = on;
  digitalWrite(RELAY_PIN, on ? RELAY_ON : RELAY_OFF);
}

void moveServoTo(int angle) {
  scanAngle = angle;
  nozzleServo.write(angle);
}

void stepScan(unsigned long now) {
  if (now - lastServoStep < SCAN_STEP_MS) return;
  lastServoStep = now;
  scanAngle += scanDir;
  if (scanAngle >= SCAN_MAX_ANGLE) { scanAngle = SCAN_MAX_ANGLE; scanDir = -1; }
  if (scanAngle <= SCAN_MIN_ANGLE) { scanAngle = SCAN_MIN_ANGLE; scanDir = 1; }
  nozzleServo.write(scanAngle);
}

void updateBuzzer(unsigned long now, bool wanted) {
  if (!wanted) {
    buzzerState = false;
    digitalWrite(BUZZER_PIN, LOW);
    return;
  }
  unsigned long interval = buzzerState ? BUZZER_ON_MS : BUZZER_OFF_MS;
  if (now - lastBuzzerToggle >= interval) {
    lastBuzzerToggle = now;
    buzzerState = !buzzerState;
    digitalWrite(BUZZER_PIN, buzzerState ? HIGH : LOW);
  }
}

void sendSMSAlert() {
  gsm.println(F("AT+CMGF=1"));
  delay(300);
  gsm.print(F("AT+CMGS=\""));
  gsm.print(ALERT_PHONE_NUMBER);
  gsm.println(F("\""));
  delay(300);
  gsm.print(F("Fire detected! Water pump activated."));
  gsm.write(26);   // Ctrl+Z = send
  delay(200);
}

void printStatus() {
  Serial.print(F("State:"));
  if (state == SCANNING)      Serial.print(F("SCANNING"));
  else if (state == ALIGNING) Serial.print(F("ALIGNING"));
  else                        Serial.print(F("FIGHTING"));
  Serial.print(F(" | ServoFlame:")); Serial.print(servoFlame);
  Serial.print(F(" FixedFlame:"));   Serial.print(fixedFlame);
  Serial.print(F(" | Smoke:"));      Serial.print(smokeValue);
  if (smokeWarning) Serial.print(F(" (HIGH)"));
  Serial.print(F(" | Angle:"));      Serial.print(scanAngle);
  Serial.print(F(" | Pump:"));       Serial.println(pumpOn ? F("ON") : F("OFF"));
}

// ================= SETUP =================
void setup() {
  Serial.begin(9600);
  gsm.begin(9600);

  pinMode(FLAME_FIXED_PIN, INPUT);
  pinMode(FLAME_SERVO_PIN, INPUT);

  digitalWrite(RELAY_PIN, RELAY_OFF);   // set level first so the pump never blips on at power-up
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_STANDBY_PIN, OUTPUT);
  pinMode(LED_ALERT_PIN, OUTPUT);

  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(LED_STANDBY_PIN, HIGH);
  digitalWrite(LED_ALERT_PIN, LOW);

  nozzleServo.attach(SERVO_PIN);
  moveServoTo(scanAngle);

  Serial.println(F("System ready. Scanning for fire..."));
}

// ================= MAIN LOOP =================
void loop() {
  unsigned long now = millis();
  readSensors(now);

  switch (state) {

    case SCANNING:
      stepScan(now);
      setPump(false);
      if (servoFlame && now >= cooldownUntil) {
        alignStartAngle = scanAngle;
        alignDir = scanDir;
        state = ALIGNING;
        Serial.print(F(">>> FLAME seen by servo sensor at angle "));
        Serial.println(scanAngle);
      }
      break;

    case ALIGNING: {
      // Keep turning slowly while the servo sensor still sees flame,
      // then settle in the middle of the flame zone = exactly on the fire.
      setPump(false);
      bool atLimit = (alignDir > 0 && scanAngle >= SCAN_MAX_ANGLE) ||
                     (alignDir < 0 && scanAngle <= SCAN_MIN_ANGLE);
      if (!servoFlame || atLimit) {
        int mid = (alignStartAngle + scanAngle) / 2;
        moveServoTo(mid);
        setPump(true);
        state = FIGHTING;
        fightStartTime = now;
        clearStartTime = 0;
        Serial.print(F(">>> LOCKED on fire at angle "));
        Serial.print(mid);
        Serial.println(F(" - PUMP ON"));
        if (!smsSentOnce || (now - lastSmsTime >= SMS_MIN_INTERVAL_MS)) {
          sendSMSAlert();
          smsSentOnce = true;
          lastSmsTime = millis();
        }
      } else if (now - lastServoStep >= ALIGN_STEP_MS) {
        lastServoStep = now;
        scanAngle += alignDir;
        nozzleServo.write(scanAngle);
      }
      break;
    }

    case FIGHTING:
      setPump(true);
      if (servoFlame) {
        clearStartTime = 0;                       // still burning -> keep spraying
      } else {
        if (clearStartTime == 0) clearStartTime = now;
        if (now - clearStartTime >= FIRE_OUT_MS) {
          setPump(false);
          state = SCANNING;
          Serial.println(F(">>> FIRE OUT - PUMP OFF - scanning again"));
        }
      }
      if (state == FIGHTING && (now - fightStartTime >= MAX_PUMP_RUN_MS)) {
        setPump(false);
        state = SCANNING;
        cooldownUntil = now + PUMP_COOLDOWN_MS;
        Serial.println(F(">>> MAX PUMP TIME reached - pausing"));
      }
      break;
  }

  // Buzzer + LEDs: alarm only when flame is involved (smoke alone = warning LED only)
  bool alarm = (state != SCANNING) || servoFlame || fixedFlame;
  updateBuzzer(now, alarm);
  digitalWrite(LED_STANDBY_PIN, alarm ? LOW : HIGH);
  digitalWrite(LED_ALERT_PIN, (alarm || smokeWarning) ? HIGH : LOW);

  // Serial Monitor: slow refresh, but any change prints instantly
  bool changed = (state != prevState) || (servoFlame != prevServoFlame) || (fixedFlame != prevFixedFlame);
  if (changed || (now - lastStatusPrint >= STATUS_INTERVAL_MS)) {
    printStatus();
    lastStatusPrint = now;
    prevState = state;
    prevServoFlame = servoFlame;
    prevFixedFlame = fixedFlame;
  }
}
