#include <Keyboard.h>
#include <Mouse.h>
#include <EEPROM.h>

#define DEBOUNCE_MS 40
#define LONG_PRESS_MS 420
#define DOUBLE_TAP_MS 380
#define MACRO_COOLDOWN_MS 80
#define MACRO_BLINK_MS 80
#define PULSE_MS 200
#define ENCODER_RATE_MS 5
#define EEPROM_MAGIC 0xAB
#define EEPROM_ADDR_MAGIC 0
#define EEPROM_ADDR_LAYER 1
#define EEPROM_WRITE_INTERVAL_MS 1000

#define LAYER_GIT 0
#define LAYER_EDITOR 1
#define LAYER_MEDIA 2
#define LAYER_COUNT 3

#define BTN_COUNT 9
#define BTN_LAYER1 0
#define BTN_LAYER2 1
#define BTN_LAYER3 2
#define BTN_KEY1 3
#define BTN_KEY2 4
#define BTN_KEY3 5
#define BTN_KEY4 6
#define BTN_LEFT 7
#define BTN_RIGHT 8

#define LED_COUNT 7

const int btnPins[BTN_COUNT] = { 9, 8, 2, 5, 4, 3, 10, 12, 11 };
const int ledPins[LED_COUNT] = { 15, 16, 17, 18, 19, 20, 21 };
const int encoderA = 7;
const int encoderB = 6;

bool allowDoubleTap[BTN_COUNT] = {
  false, false, false,
  false, false, false, true,
  false, false
};

struct ModState {
  bool shift;
  bool ctrl;
  bool alt;
  bool gui;
  bool mouseLeft;
  bool mouseRight;
  bool mouseMiddle;
};

struct ButtonState {
  bool prev[BTN_COUNT];
  unsigned long pressTime[BTN_COUNT];
  unsigned long pendingTime[BTN_COUNT];
  bool longFired[BTN_COUNT];
  unsigned long lastTap[BTN_COUNT];
};

ModState mod = {};
ButtonState btn = {};

int layer = LAYER_GIT;
int encoderALast = LOW;
unsigned long lastEncoderStep = 0;
unsigned long lastMacroTime = 0;
unsigned long macroBlinkUntil = 0;
unsigned long lastEEPROMWrite = 0;
bool layer3ZoomMod = false;
bool layer3ScrollMod = false;

void fwPressKey(uint8_t key) {
  Keyboard.press(key);
  if (key == KEY_LEFT_SHIFT) mod.shift = true;
  else if (key == KEY_LEFT_CTRL) mod.ctrl = true;
  else if (key == KEY_LEFT_ALT) mod.alt = true;
  else if (key == KEY_LEFT_GUI) mod.gui = true;
}

void fwReleaseKey(uint8_t key) {
  Keyboard.release(key);
  if (key == KEY_LEFT_SHIFT) mod.shift = false;
  else if (key == KEY_LEFT_CTRL) mod.ctrl = false;
  else if (key == KEY_LEFT_ALT) mod.alt = false;
  else if (key == KEY_LEFT_GUI) mod.gui = false;
}

void fwMousePress(uint8_t b) {
  Mouse.press(b);
  if (b == MOUSE_LEFT) mod.mouseLeft = true;
  else if (b == MOUSE_RIGHT) mod.mouseRight = true;
  else if (b == MOUSE_MIDDLE) mod.mouseMiddle = true;
}

void fwMouseRelease(uint8_t b) {
  Mouse.release(b);
  if (b == MOUSE_LEFT) mod.mouseLeft = false;
  else if (b == MOUSE_RIGHT) mod.mouseRight = false;
  else if (b == MOUSE_MIDDLE) mod.mouseMiddle = false;
}

void releaseModKeys() {
  if (mod.shift) fwReleaseKey(KEY_LEFT_SHIFT);
  if (mod.ctrl) fwReleaseKey(KEY_LEFT_CTRL);
  if (mod.alt) fwReleaseKey(KEY_LEFT_ALT);
  if (mod.gui) fwReleaseKey(KEY_LEFT_GUI);
  if (mod.mouseLeft) fwMouseRelease(MOUSE_LEFT);
  if (mod.mouseRight) fwMouseRelease(MOUSE_RIGHT);
  if (mod.mouseMiddle) fwMouseRelease(MOUSE_MIDDLE);
}

bool cooldown() {
  if (millis() - lastMacroTime < MACRO_COOLDOWN_MS) return true;
  lastMacroTime = millis();
  return false;
}

void sendChord(uint8_t mod1, uint8_t mod2, uint8_t key) {
  if (cooldown()) return;
  fwPressKey(mod1);
  if (mod2) fwPressKey(mod2);
  Keyboard.press(key);
  Keyboard.release(key);
  if (mod2) fwReleaseKey(mod2);
  fwReleaseKey(mod1);
}

void sendString(const char* str, bool enter) {
  if (cooldown()) return;
  Keyboard.print(str);
  if (enter) Keyboard.write(KEY_RETURN);
}

int getLayer() { return layer; }

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

void doGitPull()         { sendString("git pull", true); }
void doGitPush()         { sendString("git push", true); }
void doGitStatus()       { sendString("git status", true); }
void doGitCommit()       { sendString("git commit -m \"", false); }
void doGitCheckoutMain() { sendString("git checkout main", true); }
void doGitCheckoutB()    { sendString("git checkout -b ", false); }
void doGitLog5()         { sendString("git log --oneline -5", true); }

void doGitCommitClose() {
  Keyboard.print("\"");
  Keyboard.write(KEY_RETURN);
}

void doCmdP()        { sendChord(KEY_LEFT_GUI, 0, 'p'); }
void doCmdShiftF()   { sendChord(KEY_LEFT_GUI, KEY_LEFT_SHIFT, 'f'); }
void doCmdSlash()    { sendChord(KEY_LEFT_GUI, 0, '/'); }
void doCmdD()        { sendChord(KEY_LEFT_GUI, 0, 'd'); }
void doCmdL()        { sendChord(KEY_LEFT_GUI, 0, 'l'); }
void doCmdShiftK()   { sendChord(KEY_LEFT_GUI, KEY_LEFT_SHIFT, 'k'); }
void doCmdW()        { sendChord(KEY_LEFT_GUI, 0, 'w'); }
void doCmdShiftT()   { sendChord(KEY_LEFT_GUI, KEY_LEFT_SHIFT, 't'); }
void doCmdTab()      { sendChord(KEY_LEFT_GUI, 0, KEY_TAB); }
void doCmdShiftTab() { sendChord(KEY_LEFT_GUI, KEY_LEFT_SHIFT, KEY_TAB); }
void doMoveLineUp()  { sendChord(KEY_LEFT_GUI, KEY_LEFT_ALT, KEY_UP_ARROW); }
void doMoveLineDown(){ sendChord(KEY_LEFT_GUI, KEY_LEFT_ALT, KEY_DOWN_ARROW); }
void doZoomIn()      { sendChord(KEY_LEFT_GUI, 0, '='); }
void doZoomOut()     { sendChord(KEY_LEFT_GUI, 0, '-'); }

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

typedef void (*ActionFn)();

struct KeyAction {
  ActionFn tap;
  ActionFn hold;
  ActionFn doubleTap;
};

#define ACTION(t, h, d) { t, h, d }
#define NO_ACTION { NULL, NULL, NULL }

const KeyAction actionMap[LAYER_COUNT][BTN_COUNT] = {
  [LAYER_GIT] = {
    NO_ACTION,
    NO_ACTION,
    NO_ACTION,
    ACTION(doGitPull,         doGitPush,        NULL),
    ACTION(doGitStatus,       NULL,             NULL),
    ACTION(doGitCommit,       doGitCommitClose, NULL),
    ACTION(doGitCheckoutMain, doGitCheckoutB,   doGitLog5),
    NO_ACTION,
    NO_ACTION,
  },
  [LAYER_EDITOR] = {
    NO_ACTION,
    NO_ACTION,
    NO_ACTION,
    ACTION(doCmdP,     NULL,          NULL),
    ACTION(doCmdShiftF,NULL,          NULL),
    ACTION(doCmdSlash, NULL,          NULL),
    ACTION(doCmdD,     doCmdTab,      NULL),
    ACTION(doCmdL,     doCmdW,        NULL),
    ACTION(doCmdShiftK,doCmdShiftT,   NULL),
  },
  [LAYER_MEDIA] = {
    NO_ACTION,
    NO_ACTION,
    NO_ACTION,
    ACTION(doMute,       NULL, NULL),
    ACTION(doMiddleClick,NULL, NULL),
    NO_ACTION,
    NO_ACTION,
    NO_ACTION,
    NO_ACTION,
  },
};

void fireAction(int b, bool isLong, bool isDouble) {
  releaseModKeys();
  macroBlinkUntil = millis() + MACRO_BLINK_MS;

  if (b == BTN_LAYER1) { setLayer(LAYER_GIT); return; }
  if (b == BTN_LAYER2) { setLayer(LAYER_EDITOR); return; }
  if (b == BTN_LAYER3) { setLayer(LAYER_MEDIA); return; }

  if (layer == LAYER_MEDIA) {
    if (b == BTN_KEY3) { layer3ZoomMod = !layer3ZoomMod; return; }
    if (b == BTN_KEY4) { layer3ScrollMod = !layer3ScrollMod; return; }
  }

  const KeyAction* act = &actionMap[layer][b];
  ActionFn fn = NULL;
  if (isDouble && act->doubleTap) fn = act->doubleTap;
  else if (isLong && act->hold) fn = act->hold;
  else if (act->tap) fn = act->tap;
  if (fn) fn();
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

void updateLED() {
  unsigned long now = millis();
  bool blinking = (now < macroBlinkUntil);
  bool pulseOn = (now / PULSE_MS) % 2;
  bool mediaActive = (layer == LAYER_MEDIA && (layer3ZoomMod || layer3ScrollMod));

  int layerLEDs[3];
  for (int i = 0; i < 3; i++) layerLEDs[i] = (layer == i) ? HIGH : LOW;
  digitalWrite(ledPins[0], layerLEDs[0]);
  digitalWrite(ledPins[1], layerLEDs[1]);
  digitalWrite(ledPins[2], layerLEDs[2]);

  if (blinking) {
    for (int i = 3; i < LED_COUNT; i++) digitalWrite(ledPins[i], HIGH);
  } else {
    int keyStates[4] = { LOW, LOW, LOW, LOW };
    if (layer == LAYER_MEDIA) {
      keyStates[0] = layer3ZoomMod ? HIGH : LOW;
      keyStates[2] = layer3ScrollMod ? HIGH : LOW;
    }
    for (int i = 0; i < 4; i++) {
      int val = keyStates[i];
      if (mediaActive) {
        if (i == 0 && layer3ZoomMod) val = pulseOn;
        if (i == 2 && layer3ScrollMod) val = pulseOn;
      }
      digitalWrite(ledPins[3 + i], val);
    }
  }
}

void ledDance() {
  const int order[] = { 3, 0, 4, 1, 5, 2, 6, 2, 5, 1, 4, 0, 3 };
  for (int i = 0; i < 13; i++) {
    digitalWrite(ledPins[order[i]], HIGH);
    delay(200);
    digitalWrite(ledPins[order[i]], LOW);
  }
  updateLED();
}

void setup() {
  pinMode(encoderA, INPUT_PULLUP);
  pinMode(encoderB, INPUT_PULLUP);
  for (int i = 0; i < BTN_COUNT; i++) pinMode(btnPins[i], INPUT_PULLUP);
  for (int i = 0; i < LED_COUNT; i++) pinMode(ledPins[i], OUTPUT);

  if (EEPROM.read(EEPROM_ADDR_MAGIC) == EEPROM_MAGIC) {
    int saved = EEPROM.read(EEPROM_ADDR_LAYER);
    if (saved >= LAYER_GIT && saved < LAYER_COUNT) layer = saved;
  }
  Keyboard.begin();
  Mouse.begin();
  ledDance();
  for (int i = 0; i < BTN_COUNT; i++) {
    btn.prev[i] = (digitalRead(btnPins[i]) == LOW);
    btn.pressTime[i] = 0;
    btn.pendingTime[i] = 0;
    btn.longFired[i] = false;
    btn.lastTap[i] = 0;
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

  for (int i = 0; i < BTN_COUNT; i++) {
    bool rawPressed = (digitalRead(btnPins[i]) == LOW);

    if (rawPressed) {
      if (!btn.prev[i]) {
        if (btn.pendingTime[i] == 0) btn.pendingTime[i] = now;
        else if ((now - btn.pendingTime[i]) >= DEBOUNCE_MS) {
          btn.pressTime[i] = now;
          btn.longFired[i] = false;
          btn.prev[i] = true;
          btn.pendingTime[i] = 0;
        }
      }
      if (btn.prev[i] && !btn.longFired[i] && (now - btn.pressTime[i]) >= LONG_PRESS_MS) {
        btn.longFired[i] = true;
        if (i >= BTN_KEY1) fireAction(i, true, false);
      }
    } else {
      btn.pendingTime[i] = 0;
      if (btn.prev[i]) {
        if (!btn.longFired[i] && (now - btn.pressTime[i]) >= DEBOUNCE_MS) {
          bool doubleTap = allowDoubleTap[i] && (now - btn.lastTap[i]) <= DOUBLE_TAP_MS;
          if (doubleTap) {
            fireAction(i, false, true);
            btn.lastTap[i] = 0;
          } else {
            fireAction(i, false, false);
            if (allowDoubleTap[i]) btn.lastTap[i] = now;
          }
        }
        btn.prev[i] = false;
      }
    }
  }
  updateLED();
}
