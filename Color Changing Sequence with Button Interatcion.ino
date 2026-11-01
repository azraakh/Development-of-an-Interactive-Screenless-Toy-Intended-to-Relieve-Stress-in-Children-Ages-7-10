#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>

const uint8_t SDA_PIN = 8;
const uint8_t SCL_PIN = 9;
const uint8_t BUTTON1_PIN = 4;
const uint8_t BUTTON2_PIN = 5;

Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(0x40);

const uint8_t LED1_BASE = 0;
const uint8_t LED2_BASE = 4;

const float BRIGHTNESS = 0.6f;
const uint32_t DEBOUNCE_MS = 150;

struct RGB { float r, g, b; };
const RGB CYCLE[3] = {
  {  0,   0, 255},
  {  0, 255,   0},
  {255,   0,   0},
};

uint8_t state1 = 0;
uint8_t state2 = 0;

bool last1 = HIGH, last2 = HIGH;
uint32_t lastChange1 = 0, lastChange2 = 0;

void writeLED(uint8_t base, RGB c) {
  uint16_t r = (uint16_t)((c.r / 255.0f) * BRIGHTNESS * 4095);
  uint16_t g = (uint16_t)((c.g / 255.0f) * BRIGHTNESS * 4095);
  uint16_t b = (uint16_t)((c.b / 255.0f) * BRIGHTNESS * 4095);
  pwm.setPWM(base + 0, 0, r);
  pwm.setPWM(base + 1, 0, g);
  pwm.setPWM(base + 2, 0, b);
}

void setup() {
  pinMode(BUTTON1_PIN, INPUT_PULLUP);
  pinMode(BUTTON2_PIN, INPUT_PULLUP);
  Wire.begin(SDA_PIN, SCL_PIN);
  pwm.begin();
  pwm.setPWMFreq(1000);

  writeLED(LED1_BASE, CYCLE[state1]);
  writeLED(LED2_BASE, CYCLE[state2]);
}

void loop() {
  uint32_t now = millis();

  bool s1 = digitalRead(BUTTON1_PIN);
  if (s1 != last1 && (now - lastChange1) > DEBOUNCE_MS) {
    if (s1 == LOW) {
      state1 = (state1 + 1) % 3;
      writeLED(LED1_BASE, CYCLE[state1]);
    }
    last1 = s1;
    lastChange1 = now;
  }

  bool s2 = digitalRead(BUTTON2_PIN);
  if (s2 != last2 && (now - lastChange2) > DEBOUNCE_MS) {
    if (s2 == LOW) {
      state2 = (state2 + 1) % 3;
      writeLED(LED2_BASE, CYCLE[state2]);
    }
    last2 = s2;
    lastChange2 = now;
  }
}
