graph TD
    A[RP2040]
    B[ILI9341 TFT Display]
    E[NeoPixel]
    F[Adafruit BME280]
    G[Adafruit SGP40]
    H[I2C0]
    I[I2C1]
    J[UART]
    K[PWM FAN 1]
    L[PWM FAN 2]
    M[PWM FAN 3]

    B1[Pin 6: to RST] --> B
    B2[Pin 5: to TFT_CS and CSn] --> B
    B3[Pin 4: to DC] --> B
    B4[Pin 3: to T_DIN and MOSI] --> B
    B5[Pin 2: to T_CLK and SCLK] --> B
    B6[Pin 1: to CS] --> B
    B7[Pin 0: to T_D0 and MISO] --> B

    C1[PWM Pin 7] --> B

    D1[FAN1_PIN 27] --> K
    D2[FAN2_PIN 28] --> L
    D3[FAN3_PIN 29] --> M

    E1[NEOPIXEL_PIN 16 ONBOARD] --> E


    H1[SDA 8] --> H
    H2[SCL 9] --> H

    I1[SDA 10] --> I
    I2[SCL 11] --> I

    J1[UART TX 12] --> J
    J2[UART RX 13] --> J

    A --> B1
    A --> B2
    A --> B3
    A --> B4
    A --> B5
    A --> B6
    A --> B7
    A --> C1
    A --> D1
    A --> D2
    A --> D3
    A --> E1

    A --> H1
    A --> H2
    A --> I1
    A --> I2
    A --> J1
    A --> J2
    H --> F
    F --> G
