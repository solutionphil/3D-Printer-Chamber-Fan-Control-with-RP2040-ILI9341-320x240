 * Program Name: RP2040 TFT Touch UI with PWM Brightness Control
 * Version: 1.0
 * Author: Solutionphil
 * Date: 01/20/2025
 *
 * Description:
 * This program is designed for the RP2040 microcontroller to interface with an ILI9341 TFT display.
 * It provides a touch-based UI with multiple screens, PWM-based brightness control, and a file explorer
 * using LittleFS. The program includes features like touch calibration, slider-based brightness adjustment,
 * and file management.
 *
 * Hardware:
 * - RP2040 ZERO microcontroller (Arduino IDE Boardmanager: Raspberry Pi Pico/RP2040/RP2350 by Earle F. Philhower, III)
 * - ILI9341 TFT display with touch screen (Red Board)
 * - PWM pin for brightness control
 *
 * Libraries:
 * - TFT_eSPI: For TFT display control (by Bodmer)
 * - TFT_eWidget: For Slider Control (by Bodmer)
 * - RP2040_PWM: For PWM-based brightness control (by Khoi Hoang)
   - 
Instructions 
  1. Install the Libraries in the Arduino IDE 2.X
  2. Install in Boardmanager the above mentioned Board
  3. Copy the files from the eSPI folder to your Arduino IDE working folder into /libraries/TFT_eSPI
  4. Download and Open the *.INO file in your Arduino IDE
  5. Compile and Upload (for first time uploading it`s necessary to press reset+boot and release the reset button. Your RP2040 Zero should be detected as an new drive in your operating system)
  6. Have fun :) 
     
 
