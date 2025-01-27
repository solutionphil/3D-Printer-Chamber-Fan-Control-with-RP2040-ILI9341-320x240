

// PID Display State Structure
struct PIDDisplayState {
    float currentTemp;
    float targetTemp;
    bool isActive;
    float pTerm;
    float iTerm;
    float dTerm;
} pidState;

void displayPID() {
   

    // Create title bar with gradient
    for(int i = 0; i < 40; i++) {
        uint16_t gradientColor = tft.color565(0, i * 2, i * 4);
        tft.drawFastHLine(0, i, 240, gradientColor);
    }

    // Title with shadow effect
    tft.setTextColor(TFT_DARKGREY);
    tft.setFreeFont(&FreeSansBold12pt7b);
    tft.setCursor(11, 31);
    tft.print("PID Control");
    tft.setTextColor(TFT_WHITE);
    tft.setCursor(10, 30);
    tft.print("PID Control");


  // Draw back button
  screenButton.initButton(&tft, 200, 20, 60, 30, TFT_WHITE, TFT_BLUE, TFT_WHITE, backButtonLabel, 1);
  screenButton.drawButton();


}


