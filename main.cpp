/*
  Mega 2560 + 4x ULN2003 + 28BYJ-48 steppers
  Controlled over Serial by TouchDesigner (audio/MIDI -> Serial bridge)

  PULL / HOLD / RETURN BEHAVIOR (for tensile fabric control):
    Each motor uses ABSOLUTE positioning, not relative. A trigger pulls the
    motor to a clamped maximum position (never further, no matter how many
    triggers arrive), holds briefly, then automatically returns to position
    0 at a slower "decay" speed. This makes runaway drift in one direction
    physically impossible, protecting the fabric regardless of how often or
    how hard beats trigger.

  REQUIRES: AccelStepper library
    Handled automatically by platformio.ini (lib_deps) -- PlatformIO downloads
    it on first build, no manual install needed.

  WIRING (ULN2003 board pins IN1-IN4 -> Mega digital pins):
    Motor 0: IN1=22 IN2=23 IN3=24 IN4=25
    Motor 1: IN1=26 IN2=27 IN3=28 IN4=29
    Motor 2: IN1=30 IN2=31 IN3=32 IN4=33
    Motor 3: IN1=34 IN2=35 IN3=36 IN4=37

  IMPORTANT POWER NOTE:
    Do NOT power 4 ULN2003 boards from the Mega's 5V pin or USB.
    Use a separate 5V supply (rated for at least 4 x 200-300mA = ~1-1.2A,
    more if the motors are under load) feeding the ULN2003 boards' power
    input, and tie that supply's GND to the Mega's GND. Only the IN1-IN4
    signal pins connect to the Mega.

  SERIAL PROTOCOL (115200 baud, newline-terminated ASCII) -- UNCHANGED,
  no TouchDesigner-side script changes needed:
    "M<index> S<steps>\n"
      index = 0-3 (which motor)
      steps = pull intensity/distance (magnitude only -- sign is ignored,
              and the value is clamped to MAX_PULL_STEPS as a hard safety
              limit regardless of what's sent)
    Examples:
      "M0 S512"  -> motor 0 pulls toward position 512 (or MAX_PULL_STEPS,
                    whichever is smaller), holds, then returns to 0
*/

#include <AccelStepper.h>

#define MOTOR_COUNT 4

// ULN2003 half-step wiring order is IN1, IN3, IN2, IN4 for AccelStepper's HALF4WIRE mode
AccelStepper steppers[MOTOR_COUNT] = {
  AccelStepper(AccelStepper::HALF4WIRE, 22, 24, 23, 25),
  AccelStepper(AccelStepper::HALF4WIRE, 26, 28, 27, 29),
  AccelStepper(AccelStepper::HALF4WIRE, 30, 32, 31, 33),
  AccelStepper(AccelStepper::HALF4WIRE, 34, 36, 35, 37)
};

// ---- Tune these to your motors/mechanical load and fabric tension ----

// Hard safety clamp: motors can NEVER be commanded past this many steps
// from zero, no matter what value TouchDesigner sends. Set this to
// whatever your fabric/wire setup can safely handle -- start conservative
// and increase gradually once you've confirmed it's safe.
const long MAX_PULL_STEPS = 6400;

// Speed/accel while pulling (the "hit" motion -- can be snappy).
const float PULL_SPEED = 900.0;
const float PULL_ACCEL = 1100.0;

// Speed/accel while returning to zero (slower = more of a gentle "decay"
// feel; raise this if you want a snappier reset instead).
const float RETURN_SPEED = 200.0;
const float RETURN_ACCEL = 300.0;

// How long (ms) to hold at full pull before starting the return.
const unsigned long HOLD_MS = 80;

// ---- State machine ----

enum MotorPhase { PHASE_IDLE, PHASE_PULLING, PHASE_HOLDING, PHASE_RETURNING };

MotorPhase phase[MOTOR_COUNT]      = {PHASE_IDLE, PHASE_IDLE, PHASE_IDLE, PHASE_IDLE};
unsigned long holdStart[MOTOR_COUNT] = {0, 0, 0, 0};

String inputBuffer = "";

// Forward declarations -- required here because this is a plain .cpp file.
// (The Arduino IDE auto-generates these for .ino files, but PlatformIO
// compiles .cpp with standard C++ rules: a function must be declared
// before it's used.)
void readSerial();
void handleCommand(String cmd);
void startPull(int motorIndex, long pullAmount);

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  for (int i = 0; i < MOTOR_COUNT; i++) {
    steppers[i].setMaxSpeed(PULL_SPEED);
    steppers[i].setAcceleration(PULL_ACCEL);
    steppers[i].setCurrentPosition(0);
  }
}

unsigned long lastBlink = 0;
bool ledState = false;

void loop() {
  readSerial();

  for (int i = 0; i < MOTOR_COUNT; i++) {
    steppers[i].run();

    switch (phase[i]) {
      case PHASE_PULLING:
        if (steppers[i].distanceToGo() == 0) {
          phase[i] = PHASE_HOLDING;
          holdStart[i] = millis();
        }
        break;

      case PHASE_HOLDING:
        if (millis() - holdStart[i] >= HOLD_MS) {
          steppers[i].setMaxSpeed(RETURN_SPEED);
          steppers[i].setAcceleration(RETURN_ACCEL);
          steppers[i].moveTo(0);
          phase[i] = PHASE_RETURNING;
        }
        break;

      case PHASE_RETURNING:
        if (steppers[i].distanceToGo() == 0) {
          phase[i] = PHASE_IDLE;
          steppers[i].disableOutputs();
          // Reset speed profile back to pull settings for the next hit.
          steppers[i].setMaxSpeed(PULL_SPEED);
          steppers[i].setAcceleration(PULL_ACCEL);
        }
        break;

      case PHASE_IDLE:
      default:
        break;
    }
  }

  // Heartbeat: toggles roughly twice a second if the loop is still running.
  // If this LED ever stops blinking, the board is genuinely hung (not just
  // unresponsive over serial) -- confirms a firmware/lockup issue rather
  // than a USB/serial-only problem.
  if (millis() - lastBlink > 250) {
    lastBlink = millis();
    ledState = !ledState;
    digitalWrite(LED_BUILTIN, ledState);
  }
}

void readSerial() {
  while (Serial.available() > 0) {
    char c = Serial.read();
    if (c == '\n') {
      handleCommand(inputBuffer);
      inputBuffer = "";
    } else if (c != '\r') {
      // Bound the buffer so a malformed/partial line (missing '\n') can
      // never grow it indefinitely and exhaust memory.
      if (inputBuffer.length() < 32) {
        inputBuffer += c;
      } else {
        // Buffer overrun -- discard and reset rather than let it grow.
        inputBuffer = "";
      }
    }
  }
}

void handleCommand(String cmd) {
  cmd.trim();
  if (cmd.length() == 0 || cmd[0] != 'M') return;

  int spaceIdx = cmd.indexOf(' ');
  if (spaceIdx == -1) return;

  int motorIndex = cmd.substring(1, spaceIdx).toInt();
  if (motorIndex < 0 || motorIndex >= MOTOR_COUNT) return;

  String stepPart = cmd.substring(spaceIdx + 1);
  if (stepPart.length() == 0 || stepPart[0] != 'S') return;

  long steps = stepPart.substring(1).toInt();
  startPull(motorIndex, steps);
}

void startPull(int motorIndex, long pullAmount) {
  // Ignore sign, clamp to the hard safety limit.
  long clamped = abs(pullAmount);
  if (clamped > MAX_PULL_STEPS) clamped = MAX_PULL_STEPS;

  steppers[motorIndex].enableOutputs();
  steppers[motorIndex].setMaxSpeed(PULL_SPEED);
  steppers[motorIndex].setAcceleration(PULL_ACCEL);
  steppers[motorIndex].moveTo(clamped); // absolute -- always safe, never accumulates
  phase[motorIndex] = PHASE_PULLING;
}
