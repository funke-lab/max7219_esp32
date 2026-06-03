#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <MD_MAX72xx.h>
#include <MD_Parola.h>

#ifndef MAX_DEVICES
#define MAX_DEVICES 4
#endif

#ifndef MAX7219_CLK_PIN
#define MAX7219_CLK_PIN 12
#endif

#ifndef MAX7219_CS_PIN
#define MAX7219_CS_PIN 10
#endif

#ifndef MAX7219_DIN_PIN
#define MAX7219_DIN_PIN 11
#endif

#ifndef ONBOARD_RGB_PIN
#define ONBOARD_RGB_PIN 48
#endif

constexpr uint16_t kLedStepMs = 500;
constexpr uint8_t kMatrixIntensity = 1;
constexpr uint16_t kScrollSpeed = 75;
constexpr uint16_t kScrollPause = 0;
constexpr char kScrollText[] = "FunkeLab x VOLT";

Adafruit_NeoPixel onboardLed(1, ONBOARD_RGB_PIN, NEO_GRB + NEO_KHZ800);

// FC16 is the common 4-in-1 MAX7219 32x8 matrix module layout.
MD_Parola matrixDisplay(MD_MAX72XX::FC16_HW, MAX7219_DIN_PIN, MAX7219_CLK_PIN,
                        MAX7219_CS_PIN, MAX_DEVICES);

const uint32_t kLedColors[] = {
    onboardLed.Color(255, 0, 0),
    onboardLed.Color(0, 255, 0),
    onboardLed.Color(0, 0, 255),
};

uint32_t lastLedStep = 0;
uint8_t colorIndex = 0;

void stepOnboardLed()
{
  onboardLed.setPixelColor(0, kLedColors[colorIndex]);
  onboardLed.show();

  colorIndex = (colorIndex + 1) % (sizeof(kLedColors) / sizeof(kLedColors[0]));
}

void setupOnboardLed()
{
  onboardLed.begin();
  onboardLed.setBrightness(40);
  stepOnboardLed();
}

void setupMatrixDisplay()
{
  matrixDisplay.begin();
  matrixDisplay.setIntensity(kMatrixIntensity);
  matrixDisplay.displayClear();
  matrixDisplay.setCharSpacing(1);

  // Parola owns the font, frame buffer, scrolling timing, and restart logic.
  matrixDisplay.displayText(kScrollText, PA_LEFT, kScrollSpeed, kScrollPause,
                            PA_SCROLL_LEFT, PA_SCROLL_LEFT);
}

void loopOnboardLed(uint32_t now)
{
  if (now - lastLedStep < kLedStepMs) {
    return;
  }

  lastLedStep = now;
  stepOnboardLed();
}

void loopMatrixDisplay()
{
  if (matrixDisplay.displayAnimate()) {
    matrixDisplay.displayReset();
  }
}

void setup()
{
  setupOnboardLed();
  setupMatrixDisplay();
}

void loop()
{
  const uint32_t now = millis();

  loopOnboardLed(now);
  loopMatrixDisplay();
}
