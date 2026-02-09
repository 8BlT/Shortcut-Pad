#include <Keyboard.h>
#include <Mouse.h>
#include <EEPROM.h>

#define DEBOUNCE_MS 40
#define LONG_PRESS_MS 420
#define DOUBLE_TAP_MS 380
#define MACRO_COOLDOWN_MS 80
#define EEPROM_MAGIC 0xAB
#define EEPROM_ADDR_MAGIC 0
#define EEPROM_ADDR_LAYER 1

#define LAYER_GIT 1
#define LAYER_EDITOR 2
#define LAYER_MEDIA 3

#define BTN_COUNT 9

const int encoderA = 7;
const int encoderB = 6;
const int rightPin = 11;
const int leftPin = 12;
const int layer1Pin = 9;
const int layer2Pin = 8;
const int layer3Pin = 2;
const int key1Pin = 5;
const int key2Pin = 4;
const int key3Pin = 3;
const int key4Pin = 10;
const int profile1LED = 15;
const int profile2LED = 16;
const int profile3LED = 17;
const int key1LED = 18;
const int key2LED = 19;
const int key3LED = 20;
const int key4LED = 21;

int layer = LAYER_GIT;
int key1State = LOW;
int key2State = LOW;
int key3State = LOW;
int key4State = LOW;

int encoderALast = LOW;
unsigned long lastEncoderStep = 0;
bool layer3ScrollMod = false;
bool layer3ZoomMod = false;

bool btnPrev[BTN_COUNT];
unsigned long btnPressTime[BTN_COUNT];
unsigned long pressPendingTime[BTN_COUNT];
bool btnLongFired[BTN_COUNT];
unsigned long lastTapTime[BTN_COUNT];
unsigned long lastMacroTime = 0;
unsigned long macroBlinkUntil = 0;

bool fwShift = false;
bool fwCtrl = false;
bool fwAlt = false;
bool fwGui = false;

bool fwMouseLeft = false;
bool fwMouseRight = false;
bool fwMouseMiddle = false;

bool allowDoubleTap[BTN_COUNT] = {
  false, // layer1
  false, // layer2
  false, // layer3
  false, // key1
  false, // key2
  false, // key3
  true,  // key4
  false, // left
  false  // right
};

#define MACRO_BLINK_MS 80
#define PULSE_MS 200
#define ENCODER_RATE_MS 5
#define EEPROM_WRITE_INTERVAL_MS 1000

void fwPressKey(uint8_t key) {
  Keyboard.press(key);
  if (key == KEY_LEFT_SHIFT) fwShift = true;
  if (key == KEY_LEFT_CTRL) fwCtrl = true;
  if (key == KEY_LEFT_ALT) fwAlt = true;
  if (key == KEY_LEFT_GUI) fwGui = true;
}

void fwReleaseKey(uint8_t key) {
  Keyboard.release(key);
  if (key == KEY_LEFT_SHIFT) fwShift = false;
  if (key == KEY_LEFT_CTRL) fwCtrl = false;
  if (key == KEY_LEFT_ALT) fwAlt = false;
  if (key == KEY_LEFT_GUI) fwGui = false;
}

void fwMousePress(uint8_t btn) {
  Mouse.press(btn);
  if (btn == MOUSE_LEFT) fwMouseLeft = true;
  if (btn == MOUSE_RIGHT) fwMouseRight = true;
  if (btn == MOUSE_MIDDLE) fwMouseMiddle = true;
}

void fwMouseRelease(uint8_t btn) {
  Mouse.release(btn);
  if (btn == MOUSE_LEFT) fwMouseLeft = false;
  if (btn == MOUSE_RIGHT) fwMouseRight = false;
  if (btn == MOUSE_MIDDLE) fwMouseMiddle = false;
}

void setup() {
  pinMode(encoderA, INPUT_PULLUP);
  pinMode(encoderB, INPUT_PULLUP);
  pinMode(layer1Pin, INPUT_PULLUP);
  pinMode(layer2Pin, INPUT_PULLUP);
  pinMode(layer3Pin, INPUT_PULLUP);
  pinMode(key1Pin, INPUT_PULLUP);
  pinMode(key2Pin, INPUT_PULLUP);
  pinMode(key3Pin, INPUT_PULLUP);
  pinMode(key4Pin, INPUT_PULLUP);
  pinMode(leftPin, INPUT_PULLUP);
  pinMode(rightPin, INPUT_PULLUP);
  pinMode(profile1LED, OUTPUT);
  pinMode(profile2LED, OUTPUT);
  pinMode(profile3LED, OUTPUT);
  pinMode(key1LED, OUTPUT);
  pinMode(key2LED, OUTPUT);
  pinMode(key3LED, OUTPUT);
  pinMode(key4LED, OUTPUT);
  if (EEPROM.read(EEPROM_ADDR_MAGIC) == EEPROM_MAGIC) {
    int saved = EEPROM.read(EEPROM_ADDR_LAYER);
    if (saved >= LAYER_GIT && saved <= LAYER_MEDIA) layer = saved;
  }
  Keyboard.begin();
  Mouse.begin();
  ledDance();
  const int setupPins[] = { layer1Pin, layer2Pin, layer3Pin, key1Pin, key2Pin, key3Pin, key4Pin, leftPin, rightPin };
  for (int i = 0; i < BTN_COUNT; i++) {
    btnPrev[i] = (digitalRead(setupPins[i]) == LOW);
    btnPressTime[i] = 0;
    pressPendingTime[i] = 0;
    btnLongFired[i] = false;
    lastTapTime[i] = 0;
  }
}
void ledDance() {
  digitalWrite(key1LED, HIGH);
  delay(200);
  digitalWrite(key1LED, LOW);
  digitalWrite(profile1LED, HIGH);
  delay(200);
  digitalWrite(profile1LED, LOW);
  digitalWrite(key2LED, HIGH);
  delay(200);
  digitalWrite(key2LED, LOW);
  digitalWrite(profile2LED, HIGH);
  delay(200);
  digitalWrite(profile2LED, LOW);
  digitalWrite(key3LED, HIGH);
  delay(200);
  digitalWrite(key3LED, LOW);
  digitalWrite(profile3LED, HIGH);
  delay(200);
  digitalWrite(profile3LED, LOW);
  digitalWrite(key4LED, HIGH);
  delay(200);
  digitalWrite(key4LED, LOW);
  digitalWrite(profile3LED, HIGH);
  delay(200);
  digitalWrite(profile3LED, LOW);
  digitalWrite(key3LED, HIGH);
  delay(200);
  digitalWrite(key3LED, LOW);
  digitalWrite(profile2LED, HIGH);
  delay(200);
  digitalWrite(profile2LED, LOW);
  digitalWrite(key2LED, HIGH);
  delay(200);
  digitalWrite(key2LED, LOW);
  digitalWrite(profile1LED, HIGH);
  delay(200);
  digitalWrite(profile1LED, LOW);
  digitalWrite(key1LED, HIGH);
  delay(200);
  digitalWrite(key1LED, LOW);

  updateLED();
}

int getLayer() { return layer; }

static unsigned long lastEEPROMWrite = 0;

void setLayer(int L) {
  if (L == layer) return;
  layer = L;
  if (millis() - lastEEPROMWrite > EEPROM_WRITE_INTERVAL_MS) {
    EEPROM.update(EEPROM_ADDR_MAGIC, EEPROM_MAGIC);
    EEPROM.update(EEPROM_ADDR_LAYER, layer);
    lastEEPROMWrite = millis();
  }
  updateLED();
}

void releaseModKeys() {
  if (fwShift) fwReleaseKey(KEY_LEFT_SHIFT);
  if (fwCtrl) fwReleaseKey(KEY_LEFT_CTRL);
  if (fwAlt) fwReleaseKey(KEY_LEFT_ALT);
  if (fwGui) fwReleaseKey(KEY_LEFT_GUI);

  if (fwMouseLeft) fwMouseRelease(MOUSE_LEFT);
  if (fwMouseRight) fwMouseRelease(MOUSE_RIGHT);
  if (fwMouseMiddle) fwMouseRelease(MOUSE_MIDDLE);
}

bool cooldown() {
  if (millis() - lastMacroTime < MACRO_COOLDOWN_MS) return true;
  lastMacroTime = millis();
  return false;
}

void doGitPull() {
  if (cooldown()) return;
  Keyboard.print("git pull");
  Keyboard.write(KEY_RETURN);
}
void doGitPush() {
  if (cooldown()) return;
  Keyboard.print("git push");
  Keyboard.write(KEY_RETURN);
}
void doGitStatus() {
  if (cooldown()) return;
  Keyboard.print("git status");
  Keyboard.write(KEY_RETURN);
}
void doGitCommit() {
  if (cooldown()) return;
  Keyboard.print("git commit -m \"");
}
void doGitCommitClose() {
  Keyboard.print("\"");
  Keyboard.write(KEY_RETURN);
}
void doGitCheckoutMain() {
  if (cooldown()) return;
  Keyboard.print("git checkout main");
  Keyboard.write(KEY_RETURN);
}
void doGitCheckoutB() {
  if (cooldown()) return;
  Keyboard.print("git checkout -b ");
}
void doGitLog5() {
  if (cooldown()) return;
  Keyboard.print("git log --oneline -5");
  Keyboard.write(KEY_RETURN);
}

void doCmdP() {
  if (cooldown()) return;
  fwPressKey(KEY_LEFT_GUI);
  Keyboard.press('p');
  Keyboard.release('p');
  fwReleaseKey(KEY_LEFT_GUI);
}
void doCmdShiftF() {
  if (cooldown()) return;
  fwPressKey(KEY_LEFT_GUI);
  fwPressKey(KEY_LEFT_SHIFT);
  Keyboard.press('f');
  Keyboard.release('f');
  fwReleaseKey(KEY_LEFT_SHIFT);
  fwReleaseKey(KEY_LEFT_GUI);
}
void doCmdSlash() {
  if (cooldown()) return;
  fwPressKey(KEY_LEFT_GUI);
  Keyboard.press('/');
  Keyboard.release('/');
  fwReleaseKey(KEY_LEFT_GUI);
}
void doCmdD() {
  if (cooldown()) return;
  fwPressKey(KEY_LEFT_GUI);
  Keyboard.press('d');
  Keyboard.release('d');
  fwReleaseKey(KEY_LEFT_GUI);
}
void doCmdL() {
  if (cooldown()) return;
  fwPressKey(KEY_LEFT_GUI);
  Keyboard.press('l');
  Keyboard.release('l');
  fwReleaseKey(KEY_LEFT_GUI);
}
void doCmdShiftK() {
  if (cooldown()) return;
  fwPressKey(KEY_LEFT_GUI);
  fwPressKey(KEY_LEFT_SHIFT);
  Keyboard.press('k');
  Keyboard.release('k');
  fwReleaseKey(KEY_LEFT_SHIFT);
  fwReleaseKey(KEY_LEFT_GUI);
}
void doCmdW() {
  if (cooldown()) return;
  fwPressKey(KEY_LEFT_GUI);
  Keyboard.press('w');
  Keyboard.release('w');
  fwReleaseKey(KEY_LEFT_GUI);
}
void doCmdShiftT() {
  if (cooldown()) return;
  fwPressKey(KEY_LEFT_GUI);
  fwPressKey(KEY_LEFT_SHIFT);
  Keyboard.press('t');
  Keyboard.release('t');
  fwReleaseKey(KEY_LEFT_SHIFT);
  fwReleaseKey(KEY_LEFT_GUI);
}
void doCmdTab() {
  if (cooldown()) return;
  fwPressKey(KEY_LEFT_GUI);
  Keyboard.press(KEY_TAB);
  Keyboard.release(KEY_TAB);
  fwReleaseKey(KEY_LEFT_GUI);
}
void doCmdShiftTab() {
  if (cooldown()) return;
  fwPressKey(KEY_LEFT_GUI);
  fwPressKey(KEY_LEFT_SHIFT);
  Keyboard.press(KEY_TAB);
  Keyboard.release(KEY_TAB);
  fwReleaseKey(KEY_LEFT_SHIFT);
  fwReleaseKey(KEY_LEFT_GUI);
}
void doMoveLineUp() {
  if (cooldown()) return;
  fwPressKey(KEY_LEFT_GUI);
  fwPressKey(KEY_LEFT_ALT);
  Keyboard.press(KEY_UP_ARROW);
  Keyboard.release(KEY_UP_ARROW);
  fwReleaseKey(KEY_LEFT_ALT);
  fwReleaseKey(KEY_LEFT_GUI);
}
void doMoveLineDown() {
  if (cooldown()) return;
  fwPressKey(KEY_LEFT_GUI);
  fwPressKey(KEY_LEFT_ALT);
  Keyboard.press(KEY_DOWN_ARROW);
  Keyboard.release(KEY_DOWN_ARROW);
  fwReleaseKey(KEY_LEFT_ALT);
  fwReleaseKey(KEY_LEFT_GUI);
}

void doMute() {
  if (cooldown()) return;
  Keyboard.press(KEY_F13);
  Keyboard.release(KEY_F13);
}
void doVolumeUp() {
  if (cooldown()) return;
  Keyboard.press(KEY_F15);
  Keyboard.release(KEY_F15);
}
void doVolumeDown() {
  if (cooldown()) return;
  Keyboard.press(KEY_F14);
  Keyboard.release(KEY_F14);
}
void doMiddleClick() {
  if (cooldown()) return;
  fwMousePress(MOUSE_MIDDLE);
  fwMouseRelease(MOUSE_MIDDLE);
}
void doZoomIn() {
  if (cooldown()) return;
  fwPressKey(KEY_LEFT_GUI);
  Keyboard.press('=');
  Keyboard.release('=');
  fwReleaseKey(KEY_LEFT_GUI);
}
void doZoomOut() {
  if (cooldown()) return;
  fwPressKey(KEY_LEFT_GUI);
  Keyboard.press('-');
  Keyboard.release('-');
  fwReleaseKey(KEY_LEFT_GUI);
}

void runEncoderUp() {
  if (millis() - lastEncoderStep < ENCODER_RATE_MS) return;
  lastEncoderStep = millis();
  int L = getLayer();
  if (L == LAYER_GIT) return;
  if (L == LAYER_EDITOR) { doMoveLineUp(); return; }
  if (layer3ZoomMod) { doZoomIn(); return; }
  if (layer3ScrollMod) { Mouse.move(0, 0, -1); return; }
  doVolumeUp();
}
void runEncoderDown() {
  if (millis() - lastEncoderStep < ENCODER_RATE_MS) return;
  lastEncoderStep = millis();
  int L = getLayer();
  if (L == LAYER_GIT) return;
  if (L == LAYER_EDITOR) { doMoveLineDown(); return; }
  if (layer3ZoomMod) { doZoomOut(); return; }
  if (layer3ScrollMod) { Mouse.move(0, 0, 1); return; }
  doVolumeDown();
}

void fireAction(int btn, bool isLong, bool isDouble) {
  releaseModKeys();
  macroBlinkUntil = millis() + MACRO_BLINK_MS;
  int L = getLayer();
  if (btn == 0) { setLayer(LAYER_GIT); return; }
  if (btn == 1) { setLayer(LAYER_EDITOR); return; }
  if (btn == 2) { setLayer(LAYER_MEDIA); return; }
  if (L == LAYER_GIT) {
    if (btn == 3) { if (isLong) doGitPush(); else doGitPull(); return; }
    if (btn == 4) { doGitStatus(); return; }
    if (btn == 5) { if (isLong) doGitCommitClose(); else doGitCommit(); return; }
    if (btn == 6) {
      if (isDouble) doGitLog5();
      else if (isLong) doGitCheckoutB();
      else doGitCheckoutMain();
      return;
    }
    if (btn == 7 || btn == 8) return;
  }
  if (L == LAYER_EDITOR) {
    if (btn == 3) { doCmdP(); return; }
    if (btn == 4) { doCmdShiftF(); return; }
    if (btn == 5) { doCmdSlash(); return; }
    if (btn == 6) { if (isLong) doCmdTab(); else doCmdD(); return; }
    if (btn == 7) { if (isLong) doCmdW(); else doCmdL(); return; }
    if (btn == 8) { if (isLong) doCmdShiftT(); else doCmdShiftK(); return; }
  }
  if (L == LAYER_MEDIA) {
    if (btn == 3) { doMute(); return; }
    if (btn == 4) { doMiddleClick(); return; }
    if (btn == 5) { layer3ZoomMod = !layer3ZoomMod; return; }
    if (btn == 6) { layer3ScrollMod = !layer3ScrollMod; return; }
    if (btn == 7 || btn == 8) return;
  }
}

void updateLED() {
  unsigned long now = millis();
  if (now < macroBlinkUntil) {
    digitalWrite(key1LED, HIGH);
    digitalWrite(key2LED, HIGH);
    digitalWrite(key3LED, HIGH);
    digitalWrite(key4LED, HIGH);
  } else {
    uint8_t pulse = (now / PULSE_MS) % 2;
    bool pulseActive = (layer == LAYER_MEDIA && (layer3ZoomMod || layer3ScrollMod));
    digitalWrite(profile1LED, layer == LAYER_GIT ? HIGH : LOW);
    digitalWrite(profile2LED, layer == LAYER_EDITOR ? HIGH : LOW);
    digitalWrite(profile3LED, layer == LAYER_MEDIA ? HIGH : LOW);
    digitalWrite(key1LED, pulseActive && layer3ZoomMod ? pulse : key1State);
    digitalWrite(key2LED, key2State);
    digitalWrite(key3LED, pulseActive && layer3ScrollMod ? pulse : key3State);
    digitalWrite(key4LED, key4State);
  }
}

void loop() {
  unsigned long now = millis();
  int encoderRead = digitalRead(encoderA);
  if (encoderALast == LOW && encoderRead == HIGH) {
    if (digitalRead(encoderB) == LOW) runEncoderDown();
    else runEncoderUp();
  }
  encoderALast = encoderRead;

  const int pins[] = { layer1Pin, layer2Pin, layer3Pin, key1Pin, key2Pin, key3Pin, key4Pin, leftPin, rightPin };
  for (int i = 0; i < BTN_COUNT; i++) {
    bool rawPressed = (digitalRead(pins[i]) == LOW);
    if (rawPressed) {
      if (!btnPrev[i]) {
        if (pressPendingTime[i] == 0) pressPendingTime[i] = now;
        else if ((now - pressPendingTime[i]) >= DEBOUNCE_MS) {
          btnPressTime[i] = now;
          btnLongFired[i] = false;
          btnPrev[i] = true;
          pressPendingTime[i] = 0;
        }
      }
      if (btnPrev[i] && !btnLongFired[i] && (now - btnPressTime[i]) >= LONG_PRESS_MS) {
        btnLongFired[i] = true;
        if (i >= 3) fireAction(i, true, false);
      }
    } else {
      pressPendingTime[i] = 0;
      if (btnPrev[i]) {
        if (!btnLongFired[i] && (now - btnPressTime[i]) >= DEBOUNCE_MS) {
          bool doubleTap = allowDoubleTap[i] && (now - lastTapTime[i]) <= DOUBLE_TAP_MS;
          if (doubleTap) {
            fireAction(i, false, true);
            lastTapTime[i] = 0;
          } else {
            fireAction(i, false, false);
            if (allowDoubleTap[i]) lastTapTime[i] = now;
          }
        }
        btnPrev[i] = false;
      }
    }
  }
  key1State = (layer == LAYER_MEDIA && layer3ZoomMod) ? HIGH : LOW;
  key2State = LOW;
  key3State = (layer == LAYER_MEDIA && layer3ScrollMod) ? HIGH : LOW;
  key4State = LOW;
  updateLED();
}
