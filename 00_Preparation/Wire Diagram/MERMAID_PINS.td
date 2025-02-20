---
config:
  layout: elk
---
flowchart TD
    B1["Pin 6: to RST"] --> B["ILI9341 TFT Display"]
    B2["Pin 5: to TFT_CS and CSn"] --> B
    B3["Pin 4: to DC"] --> B
    B4["Pin 3: to T_DIN and MOSI"] --> B
    B5["Pin 2: to T_CLK and SCLK"] --> B
    B6["Pin 1: to CS"] --> B
    B7["Pin 0: to T_D0 and MISO"] --> B
    C1["PIN 7: to TFT LED"] --> B
    D1["PIN 27: FAN1_PIN"] --> K["PWM FAN 1"]
    D2["PIN 28: FAN2_PIN"] --> L["PWM FAN 2"]
    D3["PIN 29: FAN3_PIN"] --> M["PWM FAN 3"]
    E1["PIN 16: NEOPIXEL_PIN ONBOARD"] --> E["NeoPixel"]
    H1["PIN 8: SDA"] --> H["I2C0"]
    H2["PIN 9: SCL"] --> H
    I1["PIN 10: SDA"] --> I["I2C1"]
    I2["PIN 11: SCL"] --> I
    J1["PIN 12: UART TX"] --> J["UART"]
    J2["PIN 13: UART RX"] --> J
    A["RP2040"] --> B1 & B2 & B3 & B4 & B5 & B6 & B7 & C1 & D1 & D2 & D3 & E1 & H1 & H2 & I1 & I2 & J1 & J2
    H --> F["Adafruit BME280"]
    F --> G["Adafruit SGP40"]
