**RP2040 / ILI9341 Configuration Guide**

These files need to be modified for RP2040 / ILI9341 compatibility. You can find them in your Arduino IDE 2.x working folder, typically located at:
```
C:\Users\YOUR_NAME\Documents\Arduino\libraries\TFT_eSPI\ 
```

**Pin Configuration:**

**Display Pins:**
- SDO/MISO: RP2040 pin D0 (or leave disconnected if not reading TFT)
- LED: RP2040 pin 3V3 or 5V
- SCK: RP2040 pin D2
- SDI/MOSI: RP2040 pin D3
- DC (RS/AO): RP2040 pin D4 (can use another pin if desired)
- RESET: RP2040 pin D6 (can use another pin if desired)
- CS: RP2040 pin D1 (can use another pin if desired, or GND, see below)
- GND: RP2040 pin GND (0V)
- VCC: RP2040 5V or 3.3V (5V if display has a 5V to 3.3V regulator fitted)

**Touch Pins:**
- T_IRQ: Not Connected
- T_D0: RP2040 pin D0 (must be same as Display SDO/MISO)
- T_DIN: RP2040 pin D3 (must be the same as Display SDI/MOSI)
- T_CS: RP2040 pin D5 (can use another pin if desired, or GND, see below)
- T_CLK: RP2040 pin D1 (must be the same as Display CS)

**Note:**
Alternatively, you can rename the file to `User_Setup.h`. However, be aware that your settings may be lost during a library update.

