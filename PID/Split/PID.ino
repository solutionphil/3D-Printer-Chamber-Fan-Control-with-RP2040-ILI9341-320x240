void displayPID() {

 // Berechne PID-Output nur, wenn PID aktiv ist
  if (PIDactive==true) {
  float gap = abs(Setpoint - temp); //distance away from setpoint
  if (gap < 2) { //we're close to setpoint, use conservative tuning parameters
    myPID.SetTunings(consKp, consKi, consKd);
  } else {
    //we're far from setpoint, use aggressive tuning parameters
    myPID.SetTunings(aggKp, aggKi, aggKd);
    myPID.Compute();
   }
   Fan_PWM[0]->setPWM(FAN1_PIN, fanFrequency, fanSpeed2);
   Fan_PWM[1]->setPWM(FAN2_PIN, fanFrequency, fanSpeed2);
   Fan_PWM[2]->setPWM(FAN3_PIN, fanFrequency, fanSpeed2);
  }
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

   // Draw circular temperature gauge
  int centerX = 120;
  int centerY = 120;
  int radius = 60;
  
  // Draw outer ring with gradient
  for(int r = radius; r > radius-5; r--) {
    float tempRatio = (temp - 20) / 40.0; // Assuming range 20-60°C
    uint16_t gaugeColor = tft.color565(255 * tempRatio, 255 * (1-tempRatio), 0);
    tft.drawCircle(centerX, centerY, r, gaugeColor);
  }
  
  // Draw inner ring for target temperature
  for(int r = radius-8; r > radius-12; r--) {
    float targetRatio = (Setpoint - 20) / 40.0;
    uint16_t targetColor = tft.color565(0, 255 * (1-targetRatio), 255 * targetRatio);
    tft.drawCircle(centerX, centerY, r, targetColor);
  }

  // Temperature display in center
  tft.setTextColor(TFT_WHITE);
  tft.setFreeFont(&FreeSansBold9pt7b);
  tft.setCursor(centerX-30, centerY-10);
  tft.printf("%.1f°C", temp);
  tft.setFreeFont(&FreeSans9pt7b);
  tft.setCursor(centerX-25, centerY+20);
  tft.printf("/ %.1f°C", Setpoint);

  // Modern toggle switch for PID
  int toggleX = 30;
  int toggleY = 280;
  tft.fillRoundRect(toggleX, toggleY, 60, 30, 15, PIDactive ? TFT_GREEN : TFT_DARKGREY);
  tft.fillCircle(toggleX + (PIDactive ? 45 : 15), toggleY + 15, 12, TFT_WHITE);
  
  // Stylish up/down buttons
  drawRoundButton(190, 200, 40, 40, "+", TFT_BLUE);
  drawRoundButton(190, 260, 40, 40, "-", TFT_BLUE);
  
  // PID terms visualization
  int barX = 20;
  int barY = 195;
  drawPIDBar(barX, barY, "P", myPID.GetPterm(), TFT_RED);
  drawPIDBar(barX, barY + 25, "I", myPID.GetIterm(), TFT_GREEN);
  drawPIDBar(barX, barY + 50, "D", myPID.GetDterm(), TFT_BLUE);

  // Draw back button
  screenButton.initButton(&tft, 200, 20, 60, 30, TFT_WHITE, TFT_BLUE, TFT_WHITE, backButtonLabel, 1);
  screenButton.drawButton();

  // Anzeige aktualisieren
  updatePIDDisplay();
}

// Helper function to draw rounded buttons
void drawRoundButton(int x, int y, int w, int h, const char* label, uint16_t color) {
  tft.fillRoundRect(x, y, w, h, 10, color);
  tft.drawRoundRect(x, y, w, h, 10, TFT_WHITE);
  tft.setTextColor(TFT_WHITE);
  tft.setFreeFont(&FreeSansBold9pt7b);
  tft.setCursor(x + w/2 - 5, y + h/2 + 5);
  tft.print(label);
}

// Helper function to draw PID term bars
void drawPIDBar(int x, int y, const char* term, float value, uint16_t color) {
  tft.setTextColor(TFT_WHITE);
  tft.setFreeFont(&FreeSans9pt7b);
  tft.setCursor(x, y+15);
  tft.print(term);
  int barWidth = constrain(abs(value) * 20, 0, 100);
  tft.fillRect(x+20, y, barWidth, 20, color);
  tft.drawRect(x+20, y, 100, 20, TFT_DARKGREY);
}

void updatePIDDisplay() {
  unsigned long currentMillis = millis();
  lastSensorUpdate = currentMillis;

  int centerX = 120;
  int centerY = 120;
  int radius = 60;
  
  // Draw outer ring with gradient
  for(int r = radius; r > radius-5; r--) {
    float tempRatio = (temp - 20) / 40.0; // Assuming range 20-60°C
    uint16_t gaugeColor = tft.color565(255 * tempRatio, 255 * (1-tempRatio), 0);
    tft.drawCircle(centerX, centerY, r, gaugeColor);
  }
  
  // Draw inner ring for target temperature
  for(int r = radius-8; r > radius-12; r--) {
    float targetRatio = (Setpoint - 20) / 40.0;
    uint16_t targetColor = tft.color565(0, 255 * (1-targetRatio), 255 * targetRatio);
    tft.drawCircle(centerX, centerY, r, targetColor);
  }

  // Temperature display in center
  tft.setTextColor(TFT_WHITE);
  tft.setFreeFont(&FreeSansBold9pt7b);
  tft.setCursor(centerX-30, centerY-10);
  tft.printf("%.1f°C", temp);
  tft.setFreeFont(&FreeSans9pt7b);
  tft.setCursor(centerX-25, centerY+20);
  tft.printf("/ %.1f°C", Setpoint);

  // Modern toggle switch for PID
  int toggleX = 30;
  int toggleY = 280;
  tft.fillRoundRect(toggleX, toggleY, 60, 30, 15, PIDactive ? TFT_GREEN : TFT_DARKGREY);
  tft.fillCircle(toggleX + (PIDactive ? 45 : 15), toggleY + 15, 12, TFT_WHITE);
  
 
  // PID terms visualization
  int barX = 20;
  int barY = 195;
  drawPIDBar(barX, barY, "P", myPID.GetPterm(), TFT_RED);
  drawPIDBar(barX, barY + 25, "I", myPID.GetIterm(), TFT_GREEN);
  drawPIDBar(barX, barY + 50, "D", myPID.GetDterm(), TFT_BLUE);


  if (PIDactive==true) {
  float gap = abs(Setpoint - temp); //distance away from setpoint
  if (gap < 2) { //we're close to setpoint, use conservative tuning parameters
    myPID.SetTunings(consKp, consKi, consKd);
  } else {
    //we're far from setpoint, use aggressive tuning parameters
    myPID.SetTunings(aggKp, aggKi, aggKd);
    myPID.Compute();
   }
  }

   Serial.println();
  Serial.print(F(" Setpoint: "));  Serial.println(Setpoint);
  Serial.print(F(" Input:    "));  Serial.println(temp);
  Serial.print(F(" Output:   "));  Serial.println(fanSpeed2);
  Serial.print(F(" Pterm:    "));  Serial.println(myPID.GetPterm());
  Serial.print(F(" Iterm:    "));  Serial.println(myPID.GetIterm());
  Serial.print(F(" Dterm:    "));  Serial.println(myPID.GetDterm());
  Serial.print(F(" Control:  "));  Serial.println(myPID.GetMode());
  Serial.print(F(" Action:   "));  Serial.println(myPID.GetDirection());
  Serial.print(F(" Pmode:    "));  Serial.println(myPID.GetPmode());
  Serial.print(F(" Dmode:    "));  Serial.println(myPID.GetDmode());
  Serial.print(F(" AwMode:   "));  Serial.println(myPID.GetAwMode());
}
