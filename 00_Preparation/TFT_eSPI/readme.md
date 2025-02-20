** These Files are necessary to change for RP2040 / ILI9341. (Located in your Arduino IDE 2.x working Folder. It´s by default on Win11 machines at: C:\Users\YOUR_NAME\Documents\Arduino\libraries\TFT_eSPI ).** <br/>
** See the PIN Configuration in RP2040_ILI9341_Touch.h or here:**<br/><br/>
// Typical setup for the RP2040 ZERO is :<br/>
// Display PINS:<br/>
// Display SDO/MISO  to RP2040 pin D0 (or leave disconnected if not reading TFT)<br/>
// Display LED       to RP2040 pin 3V3 or 5V<br/>
// Display SCK       to RP2040 pin D2<br/>
// Display SDI/MOSI  to RP2040 pin D3<br/>
// Display DC (RS/AO)to RP2040 pin D4 (can use another pin if desired)<br/>
// Display RESET     to RP2040 pin D6 (can use another pin if desired)<br/>
// Display CS        to RP2040 pin D1 (can use another pin if desired, or GND, see below)<br/>
// Display GND       to RP2040 pin GND (0V)<br/>
// Display VCC       to RP2040 5V or 3.3V (5v if display has a 5V to 3.3V regulator fitted)<br/>
// Touch Pins:<br/>
// T_IRQ             Not Connected<br/>
// T_D0  						to RP2040 pin D0 (must be same like Display SDO/MISO)<br/>
// T_DIN  						to RP2040 pin D3 (must be the same like Display SDI/MOSI)<br/>
// T_CS  						to RP2040 pin D5 (can use another pin if desired, or GND, see below)<br/>
// T_CLK  						to RP2040 pin D1 (must be the same like Display CS)<br/><br/>
** Alternatively you can just rename the file to User_Setup.h. The downside is that on an library update your settings might be gone... <br/>


