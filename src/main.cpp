#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

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
constexpr uint16_t kScrollStepMs = 75;
constexpr uint8_t kMatrixIntensity = 1;
constexpr uint8_t kMatrixRows = 8;
constexpr uint8_t kMatrixColumns = MAX_DEVICES * 8;
constexpr uint8_t kGlyphWidth = 5;
constexpr uint8_t kGlyphSpacing = 1;
constexpr uint8_t kGlyphStride = kGlyphWidth + kGlyphSpacing;
constexpr int8_t kTextYOffset = 1;
constexpr uint8_t kMax7219Noop = 0x00;
constexpr uint8_t kMax7219DecodeMode = 0x09;
constexpr uint8_t kMax7219Intensity = 0x0a;
constexpr uint8_t kMax7219ScanLimit = 0x0b;
constexpr uint8_t kMax7219Shutdown = 0x0c;
constexpr uint8_t kMax7219DisplayTest = 0x0f;
constexpr char kScrollText[] = "FunkeLab x VOLT";
constexpr uint8_t kScrollTextLength = sizeof(kScrollText) - 1;

Adafruit_NeoPixel onboardLed(1, ONBOARD_RGB_PIN, NEO_GRB + NEO_KHZ800);

const uint32_t kLedColors[] = {
    onboardLed.Color(255, 0, 0),
    onboardLed.Color(0, 255, 0),
    onboardLed.Color(0, 0, 255),
};

uint32_t lastLedStep = 0;
uint32_t lastScrollStep = 0;
uint8_t colorIndex = 0;
int16_t scrollX = kMatrixColumns;

uint8_t matrixBuffer[MAX_DEVICES][kMatrixRows] = {};

struct Glyph {
  char c;
  uint8_t columns[kGlyphWidth];
};

// 5x7 font columns, least significant bit at the top of the character.
const Glyph kFont[] = {
    {' ', {0x00, 0x00, 0x00, 0x00, 0x00}},
    {'F', {0x7f, 0x09, 0x09, 0x09, 0x01}},
    {'L', {0x7f, 0x40, 0x40, 0x40, 0x40}},
    {'O', {0x3e, 0x41, 0x41, 0x41, 0x3e}},
    {'T', {0x01, 0x01, 0x7f, 0x01, 0x01}},
    {'V', {0x1f, 0x20, 0x40, 0x20, 0x1f}},
    {'a', {0x20, 0x54, 0x54, 0x54, 0x78}},
    {'b', {0x7f, 0x48, 0x44, 0x44, 0x38}},
    {'e', {0x38, 0x54, 0x54, 0x54, 0x18}},
    {'k', {0x7f, 0x10, 0x28, 0x44, 0x00}},
    {'n', {0x7c, 0x08, 0x04, 0x04, 0x78}},
    {'u', {0x3c, 0x40, 0x40, 0x20, 0x7c}},
    {'x', {0x44, 0x28, 0x10, 0x28, 0x44}},
};

void max7219ShiftByte(uint8_t value)
{
  // MAX7219 reads DIN on CLK rising edges, most significant bit first.
  for (uint8_t bit = 0; bit < 8; bit++) {
    digitalWrite(MAX7219_CLK_PIN, LOW);
    digitalWrite(MAX7219_DIN_PIN, (value & 0x80) != 0 ? HIGH : LOW);
    delayMicroseconds(2);
    digitalWrite(MAX7219_CLK_PIN, HIGH);
    delayMicroseconds(2);
    value <<= 1;
  }

  digitalWrite(MAX7219_CLK_PIN, LOW);
}

void max7219SendAll(uint8_t reg, uint8_t value)
{
  // One register/value pair is shifted for each cascaded MAX7219 chip.
  digitalWrite(MAX7219_CS_PIN, LOW);
  delayMicroseconds(2);

  for (uint8_t device = 0; device < MAX_DEVICES; device++) {
    max7219ShiftByte(reg);
    max7219ShiftByte(value);
  }

  delayMicroseconds(2);
  digitalWrite(MAX7219_CS_PIN, HIGH);
  delayMicroseconds(2);
}

void max7219SendToDevice(uint8_t targetDevice, uint8_t reg, uint8_t value)
{
  // In a daisy chain, every chip receives 16 bits per transfer. NOOP keeps the
  // untargeted chips unchanged while the selected chip receives real data.
  digitalWrite(MAX7219_CS_PIN, LOW);
  delayMicroseconds(2);

  for (int8_t device = MAX_DEVICES - 1; device >= 0; device--) {
    if (device == targetDevice) {
      max7219ShiftByte(reg);
      max7219ShiftByte(value);
    } else {
      max7219ShiftByte(kMax7219Noop);
      max7219ShiftByte(0x00);
    }
  }

  delayMicroseconds(2);
  digitalWrite(MAX7219_CS_PIN, HIGH);
  delayMicroseconds(2);
}

void max7219Flush()
{
  // Copy the RAM buffer into the MAX7219 row registers. The chip's row
  // registers are addressed 1..8, while our local row index is 0..7.
  for (uint8_t row = 0; row < kMatrixRows; row++) {
    for (uint8_t device = 0; device < MAX_DEVICES; device++) {
      max7219SendToDevice(device, row + 1, matrixBuffer[device][row]);
    }
  }
}

void max7219Clear()
{
  memset(matrixBuffer, 0, sizeof(matrixBuffer));
  max7219Flush();
}

void max7219SetPixel(uint8_t x, uint8_t y, bool on)
{
  if (x >= kMatrixColumns || y >= kMatrixRows) {
    return;
  }

  // The physical 32x8 panel row is mounted/wired as a full horizontal mirror
  // of our logical screen. Mirror x across the complete display before
  // splitting it into an 8x8 module index and a bit inside that module.
  const uint8_t physicalX = (kMatrixColumns - 1) - x;
  const uint8_t device = physicalX / 8;
  const uint8_t bit = physicalX % 8;

  if (on) {
    matrixBuffer[device][y] |= (1 << bit);
  } else {
    matrixBuffer[device][y] &= ~(1 << bit);
  }
}

void max7219Begin()
{
  pinMode(MAX7219_DIN_PIN, OUTPUT);
  pinMode(MAX7219_CLK_PIN, OUTPUT);
  pinMode(MAX7219_CS_PIN, OUTPUT);

  digitalWrite(MAX7219_CS_PIN, HIGH);
  digitalWrite(MAX7219_CLK_PIN, LOW);
  digitalWrite(MAX7219_DIN_PIN, LOW);

  delay(100);
  max7219SendAll(kMax7219Shutdown, 0x01);              // Leave shutdown mode.
  max7219SendAll(kMax7219DisplayTest, 0x00);           // Normal display mode.
  max7219SendAll(kMax7219DecodeMode, 0x00);            // Raw LED matrix rows.
  max7219SendAll(kMax7219ScanLimit, kMatrixRows - 1);  // Scan all 8 rows.
  max7219SendAll(kMax7219Intensity, kMatrixIntensity); // Brightness 0..15.
  max7219Clear();
}

void stepOnboardLed()
{
  onboardLed.setPixelColor(0, kLedColors[colorIndex]);
  onboardLed.show();

  colorIndex = (colorIndex + 1) % (sizeof(kLedColors) / sizeof(kLedColors[0]));
}

const uint8_t *glyphFor(char c)
{
  // Unknown characters become spaces so the renderer stays simple.
  for (const Glyph &glyph : kFont) {
    if (glyph.c == c) {
      return glyph.columns;
    }
  }

  return kFont[0].columns;
}

uint16_t textWidth()
{
  return kScrollTextLength * kGlyphStride;
}

void drawScrollText()
{
  // Rebuild the whole 32x8 frame from the current scroll offset.
  memset(matrixBuffer, 0, sizeof(matrixBuffer));

  for (uint8_t charIndex = 0; charIndex < kScrollTextLength; charIndex++) {
    const int16_t charX = scrollX + (charIndex * kGlyphStride);
    const uint8_t *glyph = glyphFor(kScrollText[charIndex]);

    for (uint8_t glyphX = 0; glyphX < kGlyphWidth; glyphX++) {
      const int16_t x = charX + glyphX;
      if (x < 0 || x >= kMatrixColumns) {
        continue;
      }

      // Each font byte is one vertical column of a 5x7 glyph.
      const uint8_t column = glyph[glyphX];
      for (uint8_t glyphY = 0; glyphY < 7; glyphY++) {
        if ((column & (1 << glyphY)) == 0) {
          continue;
        }

        const int8_t y = glyphY + kTextYOffset;
        if (y >= 0 && y < kMatrixRows) {
          max7219SetPixel(x, y, true);
        }
      }
    }
  }

  max7219Flush();
}

void stepScrollText()
{
  drawScrollText();
  scrollX--;

  if (scrollX < -static_cast<int16_t>(textWidth())) {
    scrollX = kMatrixColumns;
  }
}

void setup()
{
  onboardLed.begin();
  onboardLed.setBrightness(40);
  stepOnboardLed();

  max7219Begin();
  drawScrollText();
}

void loop()
{
  const uint32_t now = millis();

  if (now - lastLedStep >= kLedStepMs) {
    lastLedStep = now;
    stepOnboardLed();
  }

  if (now - lastScrollStep >= kScrollStepMs) {
    lastScrollStep = now;
    stepScrollText();
  }
}
