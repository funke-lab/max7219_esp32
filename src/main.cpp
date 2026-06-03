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

#ifndef WS2812_MATRIX_PIN
#define WS2812_MATRIX_PIN 3
#endif

constexpr uint16_t kLedStepMs = 500;
constexpr uint16_t kWs2812StepMs = 40;
constexpr uint8_t kMatrixIntensity = 1;
constexpr uint16_t kScrollSpeed = 75;
constexpr uint16_t kScrollPause = 0;
constexpr char kScrollText[] = "FunkeLab x VOLT";
constexpr uint8_t kWs2812MatrixWidth = 8;
constexpr uint8_t kWs2812MatrixHeight = 8;
constexpr uint16_t kWs2812PixelCount = kWs2812MatrixWidth * kWs2812MatrixHeight;

Adafruit_NeoPixel onboardLed(1, ONBOARD_RGB_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel ws2812Matrix(kWs2812PixelCount, WS2812_MATRIX_PIN,
                               NEO_GRB + NEO_KHZ800);

// FC16 is the common 4-in-1 MAX7219 32x8 matrix module layout.
MD_Parola matrixDisplay(MD_MAX72XX::FC16_HW, MAX7219_DIN_PIN, MAX7219_CLK_PIN,
                        MAX7219_CS_PIN, MAX_DEVICES);

const uint32_t kLedColors[] = {
    onboardLed.Color(255, 0, 0),
    onboardLed.Color(0, 255, 0),
    onboardLed.Color(0, 0, 255),
};

uint32_t lastLedStep = 0;
uint32_t lastWs2812Step = 0;
uint8_t colorIndex = 0;
uint16_t ws2812Frame = 0;

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

void setupWs2812Matrix()
{
  ws2812Matrix.begin();
  ws2812Matrix.setBrightness(10);
  ws2812Matrix.clear();
  ws2812Matrix.show();
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
  if (now - lastLedStep < kLedStepMs)
  {
    return;
  }

  lastLedStep = now;
  stepOnboardLed();
}

void loopWs2812Matrix(uint32_t now)
{
  if (now - lastWs2812Step < kWs2812StepMs)
  {
    return;
  }

  lastWs2812Step = now;

  for (uint16_t pixel = 0; pixel < kWs2812PixelCount; pixel++)
  {
    const uint16_t hue = ws2812Frame + (pixel * 1024);
    const uint32_t color = ws2812Matrix.gamma32(ws2812Matrix.ColorHSV(hue));
    ws2812Matrix.setPixelColor(pixel, color);
  }

  ws2812Matrix.show();
  ws2812Frame += 256;
}

void loopMatrixDisplay()
{
  if (matrixDisplay.displayAnimate())
  {
    matrixDisplay.displayReset();
  }
}

void setup()
{
  setupOnboardLed();
  setupWs2812Matrix();
  setupMatrixDisplay();
}

void loop()
{
  const uint32_t now = millis();

  loopOnboardLed(now);
  loopWs2812Matrix(now);
  loopMatrixDisplay();
}
