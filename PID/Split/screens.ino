void displayLoadingScreen() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.setFreeFont(LABEL2_FONT);
  tft.setTextSize(1);
  
  const char* loadingText = "Loading Files";
  tft.drawString(loadingText, 60, 140);

  // Faster animation with shorter delays
  for(int i = 0; i < 3; i++) {
    tft.drawString(".", 180 + (i * 10), 140);
    delay(150);
    yield(); // Prevent watchdog issues
  }
  delay(200); // Short pause before next screen
}

void displayScreen(int screen) {  // Update screen display logic
  // Clean up sprites before switching screens, but preserve main menu sprite
  if (screen != 0) {
    cleanupSprites();
  }
  
  // Always ensure menuSprite exists
  if (!menuSprite.created()) {
    menuSprite.createSprite(240, 320);
    Serial.println("Created menuSprite");
  }
  
  tft.fillScreen(TFT_BLACK);  // Clear the screen
  
  if (screen != 0) {  // Only show back button when not on main menu
    tft.setFreeFont(LABEL2_FONT);
    screenButton.initButton(&tft, 200, 20, 60, 30, TFT_WHITE, TFT_BLUE, TFT_WHITE, backButtonLabel, 1);
    screenButton.drawButton();
  }

  if (neopixelState) {  // Only update NeoPixel if it's enabled
    setNeoPixelColor(screen);  // Update NeoPixel color
  }

  switch (screen) {
    case 0:
      drawMainMenu(); // Display Main Menu
      break;
    case 1:
      displayPID();  // PID
      break;
    case 2:
      displayBGBrightness(); // Display Backlight Control screen
      break;
    case 3:
      displayTempAndAirQuality();
      break;
    case 4:
      displayFileExplorer();  // Display file explorer
      break;
    case 5:
      displaySettings();  // Display settings
      break;
    case 6:
      displayLEDControl();  // Display LED Control screen
      break;
    case 7:
      displayFanControl(0);  // Display Fan Control screen
      break;
    case 8:
      displayInfoScreen();  // Display system information screen
      break;
  }
}
void drawMainMenu() {
  // Create menuSprite if it doesn't exist
  if (!menuSprite.created()) {
    menuSprite.createSprite(240, 320);
  }
  
  // Draw to sprite instead of directly to screen
  menuSprite.fillSprite(TFT_DARKCYAN);
  menuSprite.setTextColor(TFT_WHITE);
  menuSprite.setFreeFont(&Yellowtail_32);
  menuSprite.setTextSize(1);
  menuSprite.setCursor(50, 50);
  menuSprite.print("Main Menu");
  menuSprite.setFreeFont(LABEL2_FONT);

  // Initialize buttons with menuSprite instead of tft
  mainMenuButtons[0].initButton(&menuSprite, 120, 100, 220, 40, TFT_WHITE, TFT_BLUE, TFT_WHITE, (char*)"Fans", 1); // Replaced Screen 1 with Fans
  mainMenuButtons[1].initButton(&menuSprite, 120, 160, 220, 40, TFT_WHITE, TFT_BLUE, TFT_WHITE, (char*)"Temp", 1);
  mainMenuButtons[2].initButton(&menuSprite, 120, 220, 220, 40, TFT_WHITE, TFT_BLUE, TFT_WHITE, (char*)"PID", 1);
  mainMenuButtons[3].initButton(&menuSprite, 120, 280, 220, 40, TFT_WHITE, TFT_DARKGREY, TFT_WHITE, (char*)"Settings", 1);

  for (uint8_t i = 0; i < 4; i++) {  // Adjusted loop to 3 buttons
    mainMenuButtons[i].drawButton();
  }
  
  // Push sprite to display
  menuSprite.pushSprite(0, 0);
}

//////

void displayBGBrightness() {
  // Clear screen and set up title
  tft.fillScreen(TFT_BLACK);
  
  // Ensure knob sprite is ready
  if (!knob.created()) knob.createSprite(30, 40);
  
// Create title bar with gradient
  for(int i = 0; i < 40; i++) {
    uint16_t gradientColor = tft.color565(0, i * 2, i * 4);
    tft.drawFastHLine(0, i, 240, gradientColor);
  }
 
  // Title with shadow effect
  tft.setTextColor(TFT_DARKGREY);
  tft.setFreeFont(&FreeSansBold12pt7b);
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(10, 30);
  tft.print("Brightness");


  // Update percentage display
  tft.setFreeFont(LABEL2_FONT);
  tft.setTextSize(1);
  tft.fillRect(90, 110, 80, 30, TFT_BLACK);
  tft.setTextColor(TFT_GREEN);
  tft.drawString(String(int(dutyCycle)) + "%", 100, 120);

  // Create slider parameters
  slider_t param;
  
  // Slider slot parameters
  param.slotWidth = 10;
  param.slotLength = 200;
  param.slotColor = TFT_BLUE;
  param.slotBgColor = TFT_YELLOW;
  param.orientation = H_SLIDER;
  
  // Slider knob parameters
  param.knobWidth = 20;
  param.knobHeight = 30;
  param.knobRadius = 5;
  param.knobColor = TFT_WHITE;
  param.knobLineColor = TFT_RED;
   
  // Slider range and movement
  param.sliderLT = 10;
  param.sliderRB = 100;
  param.startPosition = int16_t(50);
  param.sliderDelay = 0;
  // Draw the slider
  slider1.drawSlider(20, 160, param);

  int16_t x, y;    // x and y can be negative
  uint16_t w, h;   // Width and height
  slider1.getBoundingRect(&x, &y, &w, &h);     // Update x,y,w,h with bounding box
  tft.drawRect(x, y, w, h, TFT_DARKGREY); // Draw rectangle outline
  slider1.setSliderPosition(dutyCycle);
    // Draw back button
  tft.setFreeFont(LABEL2_FONT);
  tft.setTextSize(1);
  screenButton.initButton(&tft, 200, 20, 60, 30, TFT_WHITE, TFT_BLUE, TFT_WHITE, backButtonLabel, 1);
  screenButton.drawButton();

}
void displayTempAndAirQuality() {
  tft.fillScreen(TFT_BLACK);
  
  if (!backgroundDrawn) {
    tft.setTextColor(TFT_WHITE);
    tft.setFreeFont(LABEL2_FONT);
    tft.setTextSize(1);
    backgroundDrawn = true;
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
  tft.print("AQI");
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(10, 30);
  tft.print("AQI");
  tft.setFreeFont(LABEL2_FONT);
  tft.setTextSize(1);


  // Initialize sprites only once
  if (!gaugesInitialized) {
    if (!gauge1.created()) {
      gauge1.createSprite(140, 140);
      gauge1.setColorDepth(8);
    }
    if (!gauge2.created()) {
      gauge2.createSprite(140, 140);
      gauge2.setColorDepth(8);
    }
    if (!gauge3.created()) {
      gauge3.createSprite(98, 98); // Create smaller sprite for VOC gauge (70% of 140)
      gauge3.setColorDepth(8);
    }
    gaugesInitialized = true;
  }

  // Create sprites with proper dimensions if not already created
  if (!gauge1.created()) {
    gauge1.createSprite(140, 140);
  }
  if (!gauge2.created()) {
    gauge2.createSprite(140, 140);
  }
  if (!gauge3.created()) {
    gauge3.createSprite(98, 98); // Create smaller sprite for VOC gauge (70% of 140)
  }
  if (!gaugebg.created()) {
    gaugebg.createSprite(240, 320);
    gaugebg.fillSprite(TFT_BLACK);
    gaugebg.pushSprite(0, 0);
  }

 /* float temp = bme.readTemperature();
  Serial.print(temp);
  float hum = bme.readHumidity();
  int32_t voc_index = sgp.measureVocIndex(temp, hum);*/

  uint16_t voc_color = TFT_GREEN;
  if (voc_index > 100) voc_color = TFT_YELLOW;
  if (voc_index > 200) voc_color = TFT_ORANGE;
  if (voc_index > 300) voc_color = TFT_RED;
  
  // Draw to sprites instead of screen
  drawGaugeToSprite(&gauge1, 70, 65, 0, 60, temp, "Temp C", TFT_RED, 0x8800);
  drawGaugeToSprite(&gauge2, 70, 75, 0, 100, hum, "Feuchte %", TFT_BLUE, 0x0011);
  drawGaugeToSprite(&gauge3, 49, 52, 0, 500, voc_index, "VOC", voc_color, 0x0011); // Adjusted center coordinates for smaller gauge
  
  // Push sprites to screen with VOC gauge on the right (pre-scaled)
  gauge1.pushSprite(7, 45);
  gauge2.pushSprite(7, 175);
  gauge3.pushSprite(142, 125);
}


void updateTempAndAirQualityDisplay() {
  unsigned long currentMillis = millis();
  lastSensorUpdate = currentMillis;
  
  // Only update if sprites are initialized
  if (!gaugesInitialized) {
    displayTempAndAirQuality();
    return;
  }
   
 // float temp = bme.readTemperature();
  //float hum = bme.readHumidity();
  
  // Get VOC reading with temperature/humidity compensation
 // int32_t voc_index = sgp.measureVocIndex(temp, hum);
  uint16_t voc_color = TFT_GREEN;
  if (voc_index > 100) voc_color = TFT_YELLOW;
  if (voc_index > 200) voc_color = TFT_ORANGE;
  if (voc_index > 300) voc_color = TFT_RED;
  
  drawGaugeToSprite(&gauge1, 70, 65, 0, 60, temp, "Temp C", TFT_RED, 0x8800);
  drawGaugeToSprite(&gauge2, 70, 75, 0, 100, hum, "Feuchte %", TFT_BLUE, 0x0011);
  drawGaugeToSprite(&gauge3, 49, 52, 0, 500, voc_index, "VOC", voc_color, 0x0011); // Adjusted center coordinates for smaller gauge
  
  // Push updated sprites to screen with VOC gauge on the right (pre-scaled)
  gauge1.pushSprite(7, 45);
  gauge2.pushSprite(7, 175);
  gauge3.pushSprite(142, 125);
}
void displayFileExplorer() {
  // Don't clean up sprites here, let displayScreen handle it
  tft.fillScreen(TFT_BLACK);

  // Create necessary sprites for file explorer
  if (!menuSprite.created()) {
    menuSprite.createSprite(240, 320);
  }
  
  // Create title bar with gradient
  for(int i = 0; i < 40; i++) {
    uint16_t gradientColor = tft.color565(0, i * 2, i * 4);
    tft.drawFastHLine(0, i, 240, gradientColor);
  }
  
  // Title with shadow effect
  tft.setTextColor(TFT_DARKGREY);
  tft.setFreeFont(&FreeSansBold12pt7b);
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(10, 30);
  tft.print("File Explorer");

  tft.setFreeFont(LABEL2_FONT);
  screenButton.initButton(&tft, 200, 20, 60, 30, TFT_WHITE, TFT_BLUE, TFT_WHITE, backButtonLabel, 1);
  screenButton.drawButton();

  // Count files in LittleFS
  File root = LittleFS.open("/", "r");
  uint8_t i = 0;
  while (File file = root.openNextFile()) {
    String fileName = file.name();
    fileNames[i] = fileName; // Store file name in array
    buttonLabels[i] = fileName; // Store label in array
    fileButtons[i].initButton(&tft, 120, 80 + i * 40, 200, 30, TFT_WHITE, TFT_BLUE, TFT_WHITE, (char*)fileName.c_str(), 1);
    fileButtons[i].drawButton();
    i++;
  }
}

void displaySettings() {
  menuSprite.fillSprite(TFT_BLACK);
  menuSprite.setTextColor(TFT_WHITE);
   
    // Create title bar with gradient
  for(int i = 0; i < 40; i++) {
    uint16_t gradientColor = menuSprite.color565(0, i * 2, i * 4);
    menuSprite.drawFastHLine(0, i, 240, gradientColor);
  }
  
  // Title with shadow effect
  menuSprite.setTextColor(TFT_DARKGREY);
  menuSprite.setFreeFont(&FreeSansBold12pt7b);
  menuSprite.setCursor(11, 31);
  menuSprite.print("Settings");
  menuSprite.setTextColor(TFT_WHITE);
  menuSprite.setCursor(10, 30);
  menuSprite.print("Settings");
  menuSprite.setFreeFont(LABEL2_FONT);
  menuSprite.setTextSize(1);


  // Initialize settings menu buttons
  for (int i = 0; i < 4; i++) {  // Changed to 4 for removed "Fans" button
    mainMenuButtons[i].initButton(&menuSprite, 120, 100 + (i * 60), 220, 40, TFT_WHITE, TFT_BLUE, TFT_WHITE, settingsLabels[i], 1);
    mainMenuButtons[i].drawButton();
  }
  
  menuSprite.pushSprite(0, 0);
}

void displayLEDControl() {
  bool currentState = neopixelState; // Store current state to detect changes
  tft.fillScreen(TFT_DARKGREY);
  // Create title bar with gradient
  for(int i = 0; i < 40; i++) {
    uint16_t gradientColor = tft.color565(0, i * 2, i * 4);
    tft.drawFastHLine(0, i, 240, gradientColor);
  }
  
  // Title with shadow effect
  tft.setTextColor(TFT_DARKGREY);
  tft.setFreeFont(&FreeSansBold12pt7b);
  tft.setCursor(11, 31);
  tft.print("RP2040 LED");
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(10, 30);
  tft.print("RP2040 LED");
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(1);
  tft.setFreeFont(LABEL2_FONT);

  // Draw LED status
  tft.drawString("Status:", 30, 100);
  tft.setTextColor(currentState ? TFT_GREEN : TFT_RED);
  tft.drawString(currentState ? "ON" : "OFF", 120, 100);
  
  // Initialize toggle button
  mainMenuButtons[0].initButton(&tft, 120, 160, 160, 40, TFT_WHITE, 
    currentState ? TFT_RED : TFT_GREEN, 
    TFT_WHITE, 
    currentState ? ledOffLabel : ledOnLabel, 1);
  mainMenuButtons[0].drawButton();
  
  // Redraw the back button
  screenButton.initButton(&tft, 200, 20, 60, 30, TFT_WHITE, TFT_BLUE, TFT_WHITE, backButtonLabel, 1);
  screenButton.drawButton();
}

void displayFanControl(uint8_t fanIndex) {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.setFreeFont(LABEL2_FONT);
  tft.setTextSize(1);
  
  // Ensure knob sprite is ready
  if (!knob.created()) knob.createSprite(30, 40);

  // Create slider parameters
  slider_t param;
  param.slotWidth = 10;
  param.slotLength = 200;
  param.slotColor = TFT_BLUE;
  param.slotBgColor = TFT_YELLOW;
  param.orientation = H_SLIDER;
  param.knobWidth = 20;
  param.knobHeight = 30;
  param.knobRadius = 5;
  param.knobColor = TFT_WHITE;
  param.knobLineColor = TFT_RED;
  param.sliderLT = 0;
  param.sliderRB = 100;
  param.sliderDelay = 0;
  
    int16_t x, y;
    uint16_t w, h;
    slider1.getBoundingRect(&x, &y, &w, &h);     // Update x,y,w,h with bounding box

  SliderWidget* sliders[] = {&slider1, &slider2, &slider3};
  
  for (int i = 0; i < 3; i++) {
    int yOffset = i * 90;  // Space between fan controls
      
  
        // Initialize slider position before drawing
    param.startPosition = float(currentFanSpeeds[i]*100);

    sliders[i]->drawSlider(20, 80 + yOffset, param);
    sliders[i]->setSliderPosition(currentFanSpeeds[i]);
  

    // Draw current speed percentage
    tft.setTextColor(TFT_GREEN);
    tft.drawString(String(int(currentFanSpeeds[i])) + "%", 150, 60 + yOffset);
    tft.setTextColor(TFT_WHITE);
    
    // Draw fan labels
    tft.drawString("Fan " + String(i + 1), 30, 60 + yOffset);
    
    int16_t x, y;
    uint16_t w, h;
    sliders[i]->getBoundingRect(&x, &y, &w, &h);
    tft.drawRect(x, y, w, h, TFT_DARKGREY);
  }

  // Add sync checkbox
  tft.fillRect(20, 20, 15, 15, TFT_WHITE);
  tft.drawString("Sync All", 45, 20);
  if (fanSyncEnabled) {
    tft.drawLine(20, 20, 35, 35, TFT_BLACK);
    tft.drawLine(35, 20, 20, 35, TFT_BLACK);
  }

  // Redraw the back button
  screenButton.initButton(&tft, 200, 20, 60, 30, TFT_WHITE, TFT_BLUE, TFT_WHITE, backButtonLabel, 1);
  screenButton.drawButton();

  while (true) {
    uint16_t t_x = 0, t_y = 0;
    bool pressed = tft.getTouch(&t_x, &t_y);
    
    // Check for sync checkbox touch
    if (pressed && t_x >= 20 && t_x <= 35 && t_y >= 20 && t_y <= 35) {
      fanSyncEnabled = !fanSyncEnabled;
      // Redraw checkbox
      tft.fillRect(20, 20, 15, 15, TFT_WHITE);
      if (fanSyncEnabled) {
        tft.drawLine(20, 20, 35, 35, TFT_BLACK);
        tft.drawLine(35, 20, 20, 35, TFT_BLACK);
      }
      delay(250); // Debounce delay
      continue;
    }

    if (pressed) {
      for (int i = 0; i < 3; i++) {
        if (sliders[i]->checkTouch(t_x, t_y)) {
          delay(10); // Small delay for stable touch reading
          float fanSpeed = min(100.0, max(0.0, round(sliders[i]->getSliderPosition() / 10.0) * 10.0));
          currentFanSpeeds[i] = fanSpeed;
          tft.fillRect(150, 40 + (i * 90), 60, 20, TFT_BLACK);  // Clear previous percentage
          tft.fillRect(150, 60 + (i * 90), 60, 20, TFT_BLACK);  // Clear previous percentage text area
          sliders[i]->setSliderPosition(fanSpeed); // Update slider position to snapped value
          Serial.printf("Updating Fan %d to %.1f%%\n", i+1, fanSpeed);
          
          // If sync is enabled, update all fans
          if (fanSyncEnabled) {
            // Update all fan speeds in memory
            for (int j = 0; j < 3; j++) {
              currentFanSpeeds[j] = fanSpeed;
            }
            // Save all fan speeds after updating
            saveFanSpeeds(currentFanSpeeds);
            
            for (int j = 0; j < 3; j++) {
              currentFanSpeeds[j] = fanSpeed;
              sliders[j]->setSliderPosition(fanSpeed);
              Fan_PWM[j]->setPWM(FAN1_PIN + j, fanFrequency, fanSpeed);
              
              // Update percentage displays for all fans
              int yOffset = j * 90;
              tft.fillRect(150, 40 + yOffset, 60, 20, TFT_BLACK);
              tft.fillRect(150, 60 + yOffset, 60, 20, TFT_BLACK);  // Clear previous percentage text area
              tft.setTextColor(TFT_GREEN);
              tft.drawString(String(int(currentFanSpeeds[j])) + "%", 150, 60 + yOffset);
              tft.setTextColor(TFT_WHITE);
            }
          } else {
            // Original single fan update code
            Fan_PWM[i]->setPWM(FAN1_PIN + i, fanFrequency, fanSpeed);
        
              int yOffset = i * 90;
              tft.fillRect(150, 40 + yOffset, 60, 20, TFT_BLACK);
              tft.fillRect(150, 60 + yOffset, 60, 20, TFT_BLACK);  // Clear previous percentage text area
              tft.setTextColor(TFT_GREEN);
              tft.drawString(String(int(currentFanSpeeds[i])) + "%", 150, 60 + yOffset);
              tft.setTextColor(TFT_WHITE);
            
            // Mark changes as pending and update timestamp
            fanChangesPending = true;
            lastFanChange = millis();
            
            // If we're exiting the screen, force save
            if (screenButton.justPressed())
              saveFanSpeeds(currentFanSpeeds);
          }
        }
      }
    }

    screenButton.press(pressed && screenButton.contains(t_x, t_y));
    if (screenButton.justReleased()) screenButton.drawButton();

    // Switch to the settings screen when the button is pressed
    if (screenButton.justPressed()) {
      unsigned long currentTime = millis();
      if (currentTime - lastButtonPress >= DEBOUNCE_DELAY) {
        lastButtonPress = currentTime;
        currentScreen = 0;
        displayScreen(currentScreen);
        return;
      }
    }
  }
}

bool displayDeletionPrompt(String fileName) {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.setFreeFont(LABEL2_FONT);
  tft.setTextSize(1);
  tft.setCursor(10, 20);
  tft.print("Delete file: ");
  tft.setCursor(10, 55);
  tft.print(fileName);
  tft.setCursor(10, 90);
  tft.print("Are you sure?");

  yesButton.initButton(&tft, 60, 160, 80, 40, TFT_WHITE, TFT_GREEN, TFT_WHITE, yesLabel, 1);
  noButton.initButton(&tft, 180, 160, 80, 40, TFT_WHITE, TFT_RED, TFT_WHITE, noLabel, 1);

  yesButton.drawButton();
  noButton.drawButton();

  while (true) {
    uint16_t t_x = 0, t_y = 0;
    bool pressed = tft.getTouch(&t_x, &t_y);

    yesButton.press(pressed && yesButton.contains(t_x, t_y));
    noButton.press(pressed && noButton.contains(t_x, t_y));

    if (yesButton.justReleased()) {
      return true;
    }
    if (noButton.justReleased()) {
      return false;
    }
  }
}

void handleFileButtonPress(uint8_t index) {
  String fileName = buttonLabels[index];
  if (displayDeletionPrompt(fileName)) {
    if (LittleFS.exists(fileName)) {
      LittleFS.remove(fileName);
      displayFileExplorer();  // Immediately refresh screen after deletion
      return;
    }
  } else {
    displayFileContents(fileName);  // Show file contents if deletion is canceled
  }
}

void displayFileContents(String fileName) {
      uint16_t t_x = 0, t_y = 0;
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.setFreeFont(LABEL2_FONT);
  tft.setTextSize(1);
  tft.setCursor(10, 20);
  tft.print("File: ");
  tft.print(fileName);

  File file = LittleFS.open(fileName, "r");
  if (!file) {
    tft.setCursor(10, 50);
    tft.print("Failed to open file");
    return;
  }

  tft.setCursor(10, 50);
  while (file.available()) {
    tft.print((char)file.read());
  }
  file.close();

  // Add a button to go back to the file explorer screen
  backButton.initButton(&tft, 120, 220, 80, 40, TFT_WHITE, TFT_BLUE, TFT_WHITE, backLabel, 1);
  backButton.drawButton();

  while (true) {

    bool pressed = tft.getTouch(&t_x, &t_y);

    backButton.press(pressed && backButton.contains(t_x, t_y));

    if (backButton.justReleased()) {
      displayFileExplorer();
      return;
    }
  }
}

void touch_calibrate() {
  uint16_t calData[5];
  uint8_t calDataOK = 0;

  if (!LittleFS.begin()) {
    Serial.println("formatting file system");
    LittleFS.format();
    LittleFS.begin();
  }

  if (LittleFS.exists(CALIBRATION_FILE)) {
    if (REPEAT_CAL) {
      LittleFS.remove(CALIBRATION_FILE);
    } else {
      File f = LittleFS.open(CALIBRATION_FILE, "r");
      if (f) {
        if (f.readBytes((char *)calData, 14) == 14) calDataOK = 1;
        f.close();
      }
    }
  }

  if (calDataOK && !REPEAT_CAL) {
    tft.setTouch(calData);
  } else {
    tft.fillScreen(TFT_BLACK);
    tft.setCursor(20, 0);
    tft.setTextFont(2);
    tft.setTextSize(1);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.println("Touch corners as indicated");
    tft.setTextFont(1);
    tft.println();
    if (REPEAT_CAL) {
      tft.setTextColor(TFT_RED, TFT_BLACK);
      tft.println("Set REPEAT_CAL to false to stop this running again!");
    }
    tft.calibrateTouch(calData, TFT_MAGENTA, TFT_BLACK, 15);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.println("Calibration complete!");
    File f = LittleFS.open(CALIBRATION_FILE, "w");
    if (f) {
      f.write((const unsigned char *)calData, 14);
      f.close();
    }
  }
}

