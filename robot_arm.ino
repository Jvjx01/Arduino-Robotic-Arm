/*
  6-servo robotic arm controller
  ------------------------------
  Joint order and pins (base = pin 7, gripper = pin 2):

    index  joint         pin
      0    Base           7
      1    Shoulder       6
      2    Elbow          5
      3    Wrist pitch    4
      4    Wrist roll     3
      5    Gripper        2

  POWER: do NOT power six servos from the Arduino's 5V pin. Use a separate
  5-6 V supply (4 A or more is a safe starting point), and join its GND to an
  Arduino GND pin. Without that, the board will brown out and reset.

  Serial: 115200 baud, newline-terminated text commands.

    S <i> <angle>       move joint i to angle (0-180)      e.g.  S 0 45
    <i> <angle>         same as S                          e.g.  0 45
    A a0 a1 a2 a3 a4 a5 move all six joints at once        e.g.  A 90 90 90 90 90 30
    H                   go to the home pose (all 90)
    X                   stop where the arm is right now
    V <1-10>            speed, in degrees per 15 ms tick
    L <i> <min> <max>   set a safe angle range for joint i e.g.  L 5 20 100
    D                   release (detach) all servos; the next move re-attaches
    P                   report current positions
    I                   identify (replies "ARM6 ...")
    ?                   list commands

  Replies: "OK", "ERR <reason>", "POS a0 a1 a2 a3 a4 a5" (sent once the arm has
  finished a move), and "READY ARM6" after boot.
*/

#include <Servo.h>

// ---------------------------------------------------------------- hardware --
const uint8_t NUM_SERVOS = 6;
const uint8_t SERVO_PINS[NUM_SERVOS] = {7, 6, 5, 4, 3, 2};

// Pose used at boot and by the H command.
const int HOME_ANGLE[NUM_SERVOS] = {90, 90, 90, 90, 90, 90};

// Safe ranges. Narrow the gripper (index 5) to the span where your gripper
// opens and closes without hitting its end stops, e.g. 20 and 100.
int minAngle[NUM_SERVOS] = {0, 0, 0, 0, 0, 0};
int maxAngle[NUM_SERVOS] = {180, 180, 180, 180, 180, 180};

// Pulse range in microseconds. 544-2400 is the Servo library default; some
// servos behave better with 500-2500 or 600-2300.
const int PULSE_MIN_US = 544;
const int PULSE_MAX_US = 2400;

// ---------------------------------------------------------------- settings --
const unsigned long BAUD = 115200;
const unsigned long TICK_MS = 15;   // motion update interval
const int SPEED_MIN = 1;
const int SPEED_MAX = 10;
const unsigned long ATTACH_GAP_MS = 60;  // stagger attaches to ease inrush current

// ------------------------------------------------------------------- state --
Servo servos[NUM_SERVOS];
int currentAngle[NUM_SERVOS];
int targetAngle[NUM_SERVOS];
int speedStep = 2;                  // degrees per tick (2 = about 130 deg/s)
bool attachedAll = false;
bool reportPending = false;         // send POS when the current move finishes
unsigned long lastTick = 0;

char lineBuf[64];
uint8_t lineLen = 0;
bool lineOverflow = false;

// ----------------------------------------------------------------- helpers --
void replyOk() { Serial.println(F("OK")); }

void replyErr(const __FlashStringHelper *msg) {
  Serial.print(F("ERR "));
  Serial.println(msg);
}

void printPositions() {
  Serial.print(F("POS"));
  for (uint8_t i = 0; i < NUM_SERVOS; i++) {
    Serial.print(' ');
    Serial.print(currentAngle[i]);
  }
  Serial.println();
}

void attachAll() {
  for (uint8_t i = 0; i < NUM_SERVOS; i++) {
    servos[i].attach(SERVO_PINS[i], PULSE_MIN_US, PULSE_MAX_US);
    servos[i].write(currentAngle[i]);
    delay(ATTACH_GAP_MS);
  }
  attachedAll = true;
}

void detachAll() {
  for (uint8_t i = 0; i < NUM_SERVOS; i++) servos[i].detach();
  attachedAll = false;
}

// Called whenever a command asks the arm to move.
void beginMove() {
  if (!attachedAll) attachAll();
  reportPending = true;
}

bool parseLong(const char *s, long &out) {
  char *end;
  out = strtol(s, &end, 10);
  return end != s && *end == '\0';
}

bool parseJoint(const char *s, uint8_t &idx) {
  long v;
  if (!parseLong(s, v) || v < 0 || v >= NUM_SERVOS) return false;
  idx = (uint8_t)v;
  return true;
}

// -------------------------------------------------------------- commands ----
void cmdSet(const char *idxStr, const char *angleStr) {
  uint8_t i;
  long a;
  if (!parseJoint(idxStr, i)) { replyErr(F("joint must be 0-5")); return; }
  if (!parseLong(angleStr, a)) { replyErr(F("angle must be a number")); return; }
  targetAngle[i] = (int)constrain(a, (long)minAngle[i], (long)maxAngle[i]);
  beginMove();
  replyOk();
}

void cmdAll(char **tok) {
  long v[NUM_SERVOS];
  for (uint8_t i = 0; i < NUM_SERVOS; i++) {
    if (!parseLong(tok[1 + i], v[i])) { replyErr(F("angles must be numbers")); return; }
  }
  for (uint8_t i = 0; i < NUM_SERVOS; i++) {
    targetAngle[i] = (int)constrain(v[i], (long)minAngle[i], (long)maxAngle[i]);
  }
  beginMove();
  replyOk();
}

void cmdLimits(char **tok) {
  uint8_t i;
  long lo, hi;
  if (!parseJoint(tok[1], i)) { replyErr(F("joint must be 0-5")); return; }
  if (!parseLong(tok[2], lo) || !parseLong(tok[3], hi)) { replyErr(F("limits must be numbers")); return; }
  if (lo < 0 || hi > 180 || lo >= hi) { replyErr(F("need 0 <= min < max <= 180")); return; }
  minAngle[i] = (int)lo;
  maxAngle[i] = (int)hi;
  int clamped = constrain(targetAngle[i], minAngle[i], maxAngle[i]);
  if (clamped != targetAngle[i]) {
    targetAngle[i] = clamped;
    beginMove();
  }
  replyOk();
}

void cmdHome() {
  for (uint8_t i = 0; i < NUM_SERVOS; i++) {
    targetAngle[i] = constrain(HOME_ANGLE[i], minAngle[i], maxAngle[i]);
  }
  beginMove();
  replyOk();
}

void cmdStop() {
  for (uint8_t i = 0; i < NUM_SERVOS; i++) targetAngle[i] = currentAngle[i];
  reportPending = true;
  replyOk();
}

void cmdRelease() {
  for (uint8_t i = 0; i < NUM_SERVOS; i++) targetAngle[i] = currentAngle[i];
  reportPending = false;
  detachAll();
  replyOk();
}

void cmdHelp() {
  Serial.println(F("S i a | i a | A a0..a5 | H | X | V 1-10 | L i min max | D | P | I | ?"));
}

void handleLine(char *line) {
  char *tok[8];
  uint8_t n = 0;
  char *p = line;
  while (*p && n < 8) {
    while (*p == ' ' || *p == '\t') p++;
    if (!*p) break;
    tok[n++] = p;
    while (*p && *p != ' ' && *p != '\t') p++;
    if (*p) { *p = '\0'; p++; }
  }
  if (n == 0) return;

  char c = tok[0][0];
  if (c >= 'a' && c <= 'z') c -= 32;

  if (c >= '0' && c <= '9') {            // "<i> <angle>" shorthand
    if (n != 2) { replyErr(F("usage: <joint> <angle>")); return; }
    cmdSet(tok[0], tok[1]);
    return;
  }
  if (tok[0][1] != '\0') { replyErr(F("unknown command")); return; }

  switch (c) {
    case 'S':
      if (n != 3) { replyErr(F("usage: S <joint> <angle>")); return; }
      cmdSet(tok[1], tok[2]);
      break;
    case 'A':
      if (n != 1 + NUM_SERVOS) { replyErr(F("usage: A a0 a1 a2 a3 a4 a5")); return; }
      cmdAll(tok);
      break;
    case 'H':
      cmdHome();
      break;
    case 'X':
      cmdStop();
      break;
    case 'V': {
      long v;
      if (n != 2 || !parseLong(tok[1], v)) { replyErr(F("usage: V <1-10>")); return; }
      speedStep = (int)constrain(v, (long)SPEED_MIN, (long)SPEED_MAX);
      replyOk();
      break;
    }
    case 'L':
      if (n != 4) { replyErr(F("usage: L <joint> <min> <max>")); return; }
      cmdLimits(tok);
      break;
    case 'D':
      cmdRelease();
      break;
    case 'P':
      printPositions();
      break;
    case 'I':
      Serial.println(F("ARM6 v1 pins 7 6 5 4 3 2"));
      break;
    case '?':
      cmdHelp();
      break;
    default:
      replyErr(F("unknown command"));
      break;
  }
}

// ----------------------------------------------------------------- serial ---
void readSerial() {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      if (lineOverflow) {
        replyErr(F("line too long"));
      } else if (lineLen > 0) {
        lineBuf[lineLen] = '\0';
        handleLine(lineBuf);
      }
      lineLen = 0;
      lineOverflow = false;
    } else if (lineLen < sizeof(lineBuf) - 1) {
      lineBuf[lineLen++] = c;
    } else {
      lineOverflow = true;
    }
  }
}

// ----------------------------------------------------------------- motion ---
// Every TICK_MS each joint moves up to speedStep degrees toward its target,
// so the arm eases to new positions instead of snapping.
void updateMotion() {
  unsigned long now = millis();
  if (now - lastTick < TICK_MS) return;
  lastTick = now;

  bool moved = false;
  if (attachedAll) {
    for (uint8_t i = 0; i < NUM_SERVOS; i++) {
      int diff = targetAngle[i] - currentAngle[i];
      if (diff == 0) continue;
      int step = abs(diff) < speedStep ? abs(diff) : speedStep;
      currentAngle[i] += (diff > 0) ? step : -step;
      servos[i].write(currentAngle[i]);
      moved = true;
    }
  }
  if (!moved && reportPending) {
    printPositions();
    reportPending = false;
  }
}

// ------------------------------------------------------------------ setup ---
void setup() {
  Serial.begin(BAUD);
  for (uint8_t i = 0; i < NUM_SERVOS; i++) {
    currentAngle[i] = constrain(HOME_ANGLE[i], minAngle[i], maxAngle[i]);
    targetAngle[i] = currentAngle[i];
  }
  attachAll();
  lastTick = millis();
  Serial.println(F("READY ARM6"));
}

void loop() {
  readSerial();
  updateMotion();
}
