# RP2040 TFT Touch UI with PWM Brightness Control

## Version: 1.0
**Author**: Solutionphil  
**Date**: February 19th, 2025

## Description
This project leverages the RP2040 microcontroller to interface with an ILI9341 TFT display, creating a touch-based UI with multiple screens, PWM-based brightness control, and a file explorer using LittleFS. Key features include touch calibration, slider-based brightness adjustment, and file management.

## Features
- **Touch-based UI** with multiple menu screens
- **PWM-based brightness control**
- **File explorer** using LittleFS
- **Touch calibration**
- **Slider-based brightness adjustment**
- **File management**
- **MANUAL slider based FAN control**
- **PID FAN control**
- **Temperature and VOC measurement**
- **Configurable display dim**
- **Autosave and load for changed values**

## Hardware Requirements
- **RP2040 ZERO microcontroller** (Arduino IDE Boardmanager: Raspberry Pi Pico/RP2040/RP2350 by Earle F. Philhower, III)
- **ILI9341 TFT display** 2.8" 240x320 with touch screen (Red Board)
- **PWM pin** for brightness control

## Libraries
- **TFT_eSPI**: For TFT display control (by Bodmer)
- **TFT_eWidget**: For Slider Control (by Bodmer)
- **RP2040_PWM**: For PWM-based brightness control (by Khoi Hoang)

## Instructions

### Hardware Setup
1. **Wire your RP2040 Zero** with the ILI9341 240x320 display as follows:

    - **ILI9341 TFT Display Pins:**
        - PIN 7 (LED): Backlight Brightness Control 
        - Pin 6 (RST): Reset
        - Pin 5 (TFT_CS and CSn): Chip Select
        - Pin 4 (DC): Data/Command
        - Pin 3 (T_DIN and MOSI): Master Out Slave In
        - Pin 2 (T_CLK and SCLK): Serial Clock
        - Pin 1 (CS): Chip Select
        - Pin 0 (T_D0 and MISO): Master In Slave Out

    - **PWM for Fan Control:**
        - FAN1_PIN 27
        - FAN2_PIN 28
        - FAN3_PIN 29

    - **NeoPixel:**
        - ONBOARD RP2040 NEOPIXEL LED on PIN16

    - **I2C Pins:**
        - I2C0 SDA 8
        - I2C0 SCL 9
        - I2C1 SDA 10 (not used yet)
        - I2C1 SCL 11 (not used yet)

    - **UART Pins:**
        - UART TX 12 (not used yet)
        - UART RX 13 (not used yet)

    - **Environmental Sensors (Connected to I2C0):**
        - Adafruit BME280
        - Adafruit SGP40

### Software Setup
1. **Install the required libraries** in the Arduino IDE 2.x.
2. **Install the RP2040 board** in the Arduino IDE 2.x Boardmanager.
3. **Copy the files** from the eSPI folder to your Arduino IDE working folder into `/libraries/TFT_eSPI`.
4. **Download and open** the *.INO file in your Arduino IDE.
5. **Compile and upload** the code. For the first upload, press reset+boot and release the reset button. Your RP2040 Zero should be detected as a new drive in your operating system.
6. **Enjoy!** :)

---

Feel free to customize this further to better fit your needs.
