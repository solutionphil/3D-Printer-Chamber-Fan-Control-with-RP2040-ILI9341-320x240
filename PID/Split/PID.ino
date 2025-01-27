// PID Display State Structure
struct PIDDisplayState {
    float currentTemp;
    float targetTemp;
    bool isActive;
    float pTerm;
    float iTerm;
    float dTerm;
} pidState;

// Define button objects for PID control
TFT_eSPI_Button upButton, downButton, toggleButton;

void displayPID() {
    tft.fillScreen(TFT_BLACK);
 
    // Create title bar with gradient
    for(int i = 0; i < 40; i++) {
        uint16_t gradientColor = tft.color565(0, i * 2, i * 4);
        tft.drawFastHLine(0, i, 240, gradientColor);
    }

    // Draw title
    tft.setTextColor(TFT_WHITE);
    tft.setFreeFont(&FreeSansBold12pt7b);
    tft.setCursor(10, 30);
    tft.print("PID Control");
 
    // Draw current temperature
    tft.setFreeFont(&FreeSansBold12pt7b);
    tft.setCursor(20, 100);
    tft.print("Temp: ");
    tft.setTextColor(TFT_GREEN);
    tft.print(temp, 1);
    tft.print(" C");

    // Draw setpoint control
    tft.setTextColor(TFT_WHITE);
    tft.setCursor(20, 160);
    tft.print("Setpoint: ");
    tft.setTextColor(TFT_CYAN);
    tft.print(Setpoint, 1);
    tft.print(" C");

    // Draw PID status
    tft.setTextColor(TFT_WHITE);
    tft.setCursor(20, 220);
    tft.print("PID: ");
    tft.setTextColor(PIDactive ? TFT_GREEN : TFT_RED);
    tft.print(PIDactive ? "ON" : "OFF");

    // Initialize buttons
    upButton.initButton(&tft, 180, 140, 40, 40, TFT_WHITE, TFT_BLUE, TFT_WHITE, "+", 1);
    downButton.initButton(&tft, 180, 190, 40, 40, TFT_WHITE, TFT_BLUE, TFT_WHITE, "-", 1);
    toggleButton.initButton(&tft, 120, 260, 160, 40, TFT_WHITE, 
                          PIDactive ? TFT_RED : TFT_GREEN, 
                          TFT_WHITE, 
                          PIDactive ? "Turn OFF" : "Turn ON", 1);

    // Draw buttons
    upButton.drawButton();
    downButton.drawButton();
    toggleButton.drawButton();
 
    // Draw back button
    screenButton.initButton(&tft, 200, 20, 60, 30, TFT_WHITE, TFT_BLUE, TFT_WHITE, backButtonLabel, 1);
    screenButton.drawButton();
}
void updatePIDDisplay() {
    // Update temperature display
    tft.setFreeFont(&FreeSansBold12pt7b);
    tft.fillRect(90, 80, 120, 30, TFT_BLACK);
    tft.setCursor(90, 100);
    tft.setTextColor(TFT_GREEN);
    tft.print(temp, 1);
    tft.print(" C");
    // Update setpoint display
    tft.fillRect(120, 140, 80, 30, TFT_BLACK);
    tft.setCursor(120, 160);
    tft.setTextColor(TFT_CYAN);
    tft.print(Setpoint, 1);
    tft.print(" C");
}
