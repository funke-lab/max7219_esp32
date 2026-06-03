# ESP32-S3 MAX7219 Matrix Demo

PlatformIO project for an ESP32-S3 connected to a MAX7219 LED matrix and an
onboard addressable RGB LED on GPIO 48.

## Wiring

| MAX7219 signal | ESP32-S3 pin |
| --- | --- |
| CLK | GPIO 12 |
| CS / LOAD | GPIO 10 |
| DIN | GPIO 11 |

The onboard WS2812-style RGB LED is configured on GPIO 48.

## Behavior

- The onboard RGB LED steps through fixed red, green, and blue states.
- The MAX7219 matrix scrolls the text `FunkeLab x VOLT` from right to left.

## Configuration

`MAX_DEVICES` in `platformio.ini` controls the number of chained 8x8 MAX7219
modules. It currently defaults to `4`.

Build and upload with:

```sh
pio run -t upload
```
