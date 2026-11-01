#include <Wire.h>
#include <math.h>
#include <Adafruit_PWMServoDriver.h>

const uint8_t SDA_PIN = 8;
const uint8_t SCL_PIN = 9;
const uint8_t BUTTON1_PIN = 4;
const uint8_t BUTTON2_PIN = 5;

Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(0x40);

const uint8_t LED1_BASE = 0;
const uint8_t LED2_BASE = 4;

const float BRIGHTNESS = 0.6f;

const uint32_t INTRO_MS = 5000;
const uint32_t SHOW_MS = 800;
const uint32_t GAP_MS = 300;
const uint32_t RESPONSE_MS = 4000;
const uint32_t FAIL_MS = 5000;
const uint32_t LONG_PRESS_MS = 800;
const uint32_t TRIPLE_WINDOW_MS = 1200;
const uint32_t DEBOUNCE_MS = 40;

const uint8_t START_LEVEL = 4;
const uint8_t MAX_LEVEL = 20;
const uint8_t LEVEL_STEP = 2;

struct RGB { float r, g, b; };

const RGB PALETTE[6] = {
  {255, 60, 60},
  {60, 255, 90},
  {70, 120, 255},
  {221, 87, 28},
  {200, 60, 220},
  {60, 220, 220},
};
const RGB RED_COLOR = {255, 0, 0};

enum AppState { INTRO, SHOW, RECALL, FAILSTATE, MANUAL, FROZEN };
AppState appState = INTRO;
uint32_t stateStart = 0;

uint8_t level = START_LEVEL;
uint8_t sequence[MAX_LEVEL];
uint8_t seqIndex = 0;
RGB settledColor = PALETTE[0];
RGB manualColor = PALETTE[0];

bool btn1Down = false, btn2Down = false;
uint32_t btn1DownSince = 0, btn2DownSince = 0;
uint32_t btn1LastEdge = 0, btn2LastEdge = 0;
bool btn1RawLast = HIGH, btn2RawLast = HIGH;

bool btn1JustPressed, btn1JustReleased, btn2JustPressed, btn2JustReleased;

uint8_t pressCountSinceFreeze = 0;
uint32_t firstPressTimeSinceFreeze = 0;

bool recallWaiting = false;
uint32_t recallWindowStart = 0;
bool recallB1 = false, recallB2 = false;

uint8_t longPressButton = 0;

void updateButtons() {
  btn1JustPressed = false;
  btn1JustReleased = false;
  btn2JustPressed = false;
  btn2JustReleased = false;

  bool raw1 = digitalRead(BUTTON1_PIN);
  uint32_t now = millis();
  if (raw1 != btn1RawLast && (now - btn1LastEdge) > DEBOUNCE_MS) {
    btn1LastEdge = now;
    btn1RawLast = raw1;
    if (raw1 == LOW) {
      btn1Down = true;
      btn1DownSince = now;
      btn1JustPressed = true;
    } else {
      btn1Down = false;
      btn1JustReleased = true;
    }
  }

  bool raw2 = digitalRead(BUTTON2_PIN);
  if (raw2 != btn2RawLast && (now - btn2LastEdge) > DEBOUNCE_MS) {
    btn2LastEdge = now;
    btn2RawLast = raw2;
    if (raw2 == LOW) {
      btn2Down = true;
      btn2DownSince = now;
      btn2JustPressed = true;
    } else {
      btn2Down = false;
      btn2JustReleased = true;
    }
  }
}

bool checkLongPress() {
  uint32_t now = millis();
  if (btn1Down && (now - btn1DownSince) >= LONG_PRESS_MS) {
    longPressButton = 1;
    return true;
  }
  if (btn2Down && (now - btn2DownSince) >= LONG_PRESS_MS) {
    longPressButton = 2;
    return true;
  }
  return false;
}

void writeBoth(RGB c, float scale) {
  uint16_t r = (uint16_t)((c.r / 255.0f) * scale * 4095);
  uint16_t g = (uint16_t)((c.g / 255.0f) * scale * 4095);
  uint16_t b = (uint16_t)((c.b / 255.0f) * scale * 4095);
  pwm.setPWM(LED1_BASE + 0, 0, r);
  pwm.setPWM(LED1_BASE + 1, 0, g);
  pwm.setPWM(LED1_BASE + 2, 0, b);
  pwm.setPWM(LED2_BASE + 0, 0, r);
  pwm.setPWM(LED2_BASE + 1, 0, g);
  pwm.setPWM(LED2_BASE + 2, 0, b);
}

void writeSingle(uint8_t sphere, RGB c, float scale) {
  uint8_t base = (sphere == 0) ? LED1_BASE : LED2_BASE;
  uint8_t other = (sphere == 0) ? LED2_BASE : LED1_BASE;
  uint16_t r = (uint16_t)((c.r / 255.0f) * scale * 4095);
  uint16_t g = (uint16_t)((c.g / 255.0f) * scale * 4095);
  uint16_t b = (uint16_t)((c.b / 255.0f) * scale * 4095);
  pwm.setPWM(base + 0, 0, r);
  pwm.setPWM(base + 1, 0, g);
  pwm.setPWM(base + 2, 0, b);
  pwm.setPWM(other + 0, 0, 0);
  pwm.setPWM(other + 1, 0, 0);
  pwm.setPWM(other + 2, 0, 0);
}

void allOff() {
  RGB z = {0, 0, 0};
  writeBoth(z, 0);
}

RGB colorWheel(float phase) {
  float p = phase * TWO_PI;
  RGB c;
  c.r = (sinf(p) + 1.0f) * 0.5f * 255.0f;
  c.g = (sinf(p + 2.0944f) + 1.0f) * 0.5f * 255.0f;
  c.b = (sinf(p + 4.1888f) + 1.0f) * 0.5f * 255.0f;
  return c;
}

float breathEnvelope(float phase) {
  float s = sinf(phase * TWO_PI - HALF_PI);
  float e = expf(s);
  return (e - expf(-1.0f)) / (expf(1.0f) - expf(-1.0f));
}

void generateSequence(uint8_t len) {
  for (uint8_t i = 0; i < len; i++) sequence[i] = random(0, 2);
}

void enterState(AppState s) {
  appState = s;
  stateStart = millis();
}

void setup() {
  pinMode(BUTTON1_PIN, INPUT_PULLUP);
  pinMode(BUTTON2_PIN, INPUT_PULLUP);
  Wire.begin(SDA_PIN, SCL_PIN);
  pwm.begin();
  pwm.setPWMFreq(1000);
  randomSeed(analogRead(0));
  enterState(INTRO);
}

void loop() {
  updateButtons();
  uint32_t now = millis();

  if (appState != MANUAL) {
    if (checkLongPress()) {
      enterState(MANUAL);
    }
  }

  switch (appState) {

    case INTRO: {
      float t = (now - stateStart) / (float)INTRO_MS;
      if (t >= 1.0f) {
        settledColor = PALETTE[random(0, 6)];
        level = START_LEVEL;
        generateSequence(level);
        seqIndex = 0;
        enterState(SHOW);
      } else {
        float breath = breathEnvelope(t);
        RGB c = colorWheel(t);
        writeBoth(c, breath * BRIGHTNESS);
      }
      break;
    }

    case SHOW: {
      uint32_t elapsed = now - stateStart;
      uint32_t cycle = SHOW_MS + GAP_MS;
      uint32_t totalShow = (uint32_t)level * cycle;

      if (elapsed >= totalShow) {
        seqIndex = 0;
        recallWaiting = false;
        allOff();
        enterState(RECALL);
      } else {
        uint8_t idx = elapsed / cycle;
        uint32_t within = elapsed % cycle;
        if (within < SHOW_MS) {
          writeSingle(sequence[idx], settledColor, BRIGHTNESS);
        } else {
          allOff();
        }
      }
      break;
    }

    case RECALL: {
      if (seqIndex >= level) {
        if (level + LEVEL_STEP <= MAX_LEVEL) level += LEVEL_STEP;
        generateSequence(level);
        seqIndex = 0;
        enterState(SHOW);
        break;
      }

      if (!recallWaiting) {
        recallWaiting = true;
        recallWindowStart = now;
        recallB1 = false;
        recallB2 = false;
      }

      if (btn1JustPressed) recallB1 = true;
      if (btn2JustPressed) recallB2 = true;

      bool gotOne = recallB1 || recallB2;
      bool windowDone = (now - recallWindowStart) >= RESPONSE_MS;

      if (gotOne || windowDone) {
        bool correct;
        if (recallB1 && !recallB2) correct = (sequence[seqIndex] == 0);
        else if (recallB2 && !recallB1) correct = (sequence[seqIndex] == 1);
        else correct = false;

        if (!correct) {
          enterState(FAILSTATE);
        } else {
          seqIndex++;
          recallWaiting = false;
        }
      }
      break;
    }

    case FAILSTATE: {
      float t = (now - stateStart) / (float)FAIL_MS;
      if (t >= 1.0f) {
        allOff();
        enterState(INTRO);
      } else {
        float breath = breathEnvelope(t);
        writeBoth(RED_COLOR, breath * BRIGHTNESS);
      }
      break;
    }

    case MANUAL: {
      bool stillDown = (longPressButton == 1) ? btn1Down : btn2Down;
      if (stillDown) {
        uint32_t elapsed = now - stateStart;
        float tb = (elapsed % 3000UL) / 3000.0f;
        float th = (elapsed % 4000UL) / 4000.0f;
        float breath = breathEnvelope(tb);
        RGB c = colorWheel(th);
        manualColor = c;
        writeBoth(c, breath * BRIGHTNESS);
      } else {
        pressCountSinceFreeze = 0;
        enterState(FROZEN);
      }
      break;
    }

    case FROZEN: {
      uint32_t elapsed = now - stateStart;
      float t = (elapsed % 3000UL) / 3000.0f;
      float breath = breathEnvelope(t);
      writeBoth(manualColor, breath * BRIGHTNESS);

      if (btn1JustPressed || btn2JustPressed) {
        uint32_t sinceFirst = now - firstPressTimeSinceFreeze;
        if (pressCountSinceFreeze == 0 || sinceFirst > TRIPLE_WINDOW_MS) {
          pressCountSinceFreeze = 1;
          firstPressTimeSinceFreeze = now;
        } else {
          pressCountSinceFreeze++;
        }

        if (pressCountSinceFreeze >= 3) {
          pressCountSinceFreeze = 0;
          enterState(INTRO);
        }
      }
      break;
    }
  }
}
