# RP2040 TFT Touch UI with PWM Brightness Control

## Program Information
- **Program Name**: RP2040 TFT Touch UI with PWM Brightness Control
- **Version**: 1.0
- **Author**: Solutionphil
- **Date**: 02/15/2025

## Description
This program is designed for the RP2040 microcontroller to interface with an ILI9341 TFT display. It provides a touch-based UI with multiple screens, PWM-based brightness control, PWM-based fan control, and a file explorer using LittleFS. The program includes features like touch calibration, slider-based brightness and fan speed adjustment, and file management.

## Hardware
- RP2040 microcontroller
- ILI9341 TFT display with touch screen
- PWM pins for brightness control and fan control

## Libraries
- TFT_eSPI: For TFT display control
- TFT_eWidget: For Sliders
- RP2040_PWM: For PWM-based brightness and fan speed control
- Adafruit_NeoPixel: For NeoPixel control
- Adafruit_BME280: For environmental sensing
- Adafruit_SGP40: For air quality sensing
- Wire: For I2C communication
- QuickPID for PWM Control

## Pin Configuration

### ILI9341 TFT Display Pins:
- **Pin 6 (RST)**: Reset
- **Pin 5 (TFT_CS and CSn)**: Chip Select
- **Pin 4 (DC)**: Data/Command
- **Pin 3 (T_DIN and MOSI)**: Master Out Slave In
- **Pin 2 (T_CLK and SCLK)**: Serial Clock
- **Pin 1 (CS)**: Chip Select
- **Pin 0 (T_D0 and MISO)**: Master In Slave Out

### PWM for Brightness Control:
- **PWM Pin 7**

### PWM for Fan Control:
- **FAN1_PIN 27**
- **FAN2_PIN 28**
- **FAN3_PIN 29**

### NeoPixel:
- **NEOPIXEL_PIN 16**

### I2C Pins:
- **I2C0 SDA 8**
- **I2C0 SCL 9**
- **I2C1 SDA 10**
- **I2C1 SCL 11**

### UART Pins:
- **UART TX 12**
- **UART RX 13**

### Environmental Sensors (Connected to I2C0):
- **Adafruit BME280**
- **Adafruit SGP40**

## Setup and Initialization

### Code Snippet
