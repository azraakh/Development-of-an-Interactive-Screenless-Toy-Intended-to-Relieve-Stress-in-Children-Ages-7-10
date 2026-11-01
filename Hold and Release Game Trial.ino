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

const float BUILD_MS = 4000.0f;
const float RELEASE_MS = 6000.0f;
const float BREATH_PERIOD_MS = 4000.0f;
const float BREATH_DEPTH = 0.12f;
const float MIN_BRI = 0.12f;
const float MAX_BRI = 0.85f;

struct RGB { float r, g, b; };
const RGB COOL = {60, 130, 255};
const RGB WARM = {255, 90, 30};

float tension1 = 0.0f;
float tension2 = 0.0f;
uint32_t lastUpdate = 0;
const uint32_t PHASE_OFFSET_MS = 500;

void writeLED(uint8_t base, RGB c, float scale) {
  uint16_t r = (uint16_t)((c.r / 255.0f) * scale * 4095);
  uint16_t g = (uint16_t)((c.g / 255.0f) * scale * 4095);
  uint16_t b = (uint16_t)((c.b / 255.0f) * scale * 4095);
  pwm.setPWM(base + 0, 0, r);
  pwm.setPWM(base + 1, 0, g);
  pwm.setPWM(base + 2, 0, b);
}

RGB lerpColor(RGB a, RGB b, float t) {
  RGB c;
  c.r = a.r + (b.r - a.r) * t;
  c.g = a.g + (b.g - a.g) * t;
  c.b = a.b + (b.b - a.b) * t;
  return c;
}

void updateSphere(uint8_t base, float &tension, bool held, float dt, uint32_t breathPhaseOffset) {
  if (held) {
    tension += dt / BUILD_MS;
  } else {
    tension -= dt / RELEASE_MS;
  }
  tension = constrain(tension, 0.0f, 1.0f);

  float baseBrightness = MIN_BRI + (MAX_BRI - MIN_BRI) * tension;
  RGB color = lerpColor(COOL, WARM, tension);

  uint32_t now = millis();
  float phase = ((now + breathPhaseOffset) % (uint32_t)BREATH_PERIOD_MS) / BREATH_PERIOD_MS;
  float mod = 1.0f + BREATH_DEPTH * sinf(phase * TWO_PI);

  float finalBrightness = constrain(baseBrightness * mod, 0.0f, 1.0f);
  writeLED(base, color, finalBrightness);
}

void setup() {
  pinMode(BUTTON1_PIN, INPUT_PULLUP);
  pinMode(BUTTON2_PIN, INPUT_PULLUP);
  Wire.begin(SDA_PIN, SCL_PIN);
  pwm.begin();
  pwm.setPWMFreq(1000);
  lastUpdate = millis();
}

void loop() {
  uint32_t now = millis();
  float dt = now - lastUpdate;
  lastUpdate = now;

  bool held1 = (digitalRead(BUTTON1_PIN) == LOW);
  bool held2 = (digitalRead(BUTTON2_PIN) == LOW);

  updateSphere(LED1_BASE, tension1, held1, dt, 0);
  updateSphere(LED2_BASE, tension2, held2, dt, PHASE_OFFSET_MS);

  delay(15);
}
