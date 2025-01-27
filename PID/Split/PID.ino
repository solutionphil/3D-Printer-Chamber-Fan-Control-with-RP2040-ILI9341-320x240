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
    float tempRatio = (temp - 20) / 40.0; // Assuming range 20-60Â°C
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
  tft.printf("%.1fÂ°C", temp);
  tft.setFreeFont(&FreeSans9pt7b);
  tft.setCursor(centerX-25, centerY+20);
  tft.printf("/ %.1fÂ°C", Setpoint);

  // Initialize PID toggle button
  pidToggle.initButton(&tft, 60, 280, 100, 40, TFT_WHITE, 
                      PIDactive ? TFT_GREEN : TFT_DARKGREY,
                      TFT_WHITE, PIDactive ? (char*)"PID ON" : (char*)"PID OFF", 1);
  pidToggle.drawButton();
  
  // Initialize temperature control buttons
  tempUp.initButton(&tft, 190, 200, 40, 40, TFT_WHITE, TFT_BLUE, TFT_WHITE, (char*)"+", 1);
  tempDown.initButton(&tft, 190, 260, 40, 40, TFT_WHITE, TFT_BLUE, TFT_WHITE, (char*)"-", 1);
  tempUp.drawButton();
  tempDown.drawButton();
  
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
    float tempRatio = (temp - 20) / 40.0; // Assuming range 20-60Â°C
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
  tft.printf("%.1fÂ°C", temp);
  tft.setFreeFont(&FreeSans9pt7b);
  tft.setCursor(centerX-25, centerY+20);
  tft.printf("/ %.1fÂ°C", Setpoint);


 
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
