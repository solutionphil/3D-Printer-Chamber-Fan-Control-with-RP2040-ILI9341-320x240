void displayInfoScreen() {
  tft.fillScreen(TFT_BLACK);
    // Create title bar with gradient
  for(int i = 0; i < 40; i++) {
    uint16_t gradientColor = tft.color565(0, i * 2, i * 4);
    tft.drawFastHLine(0, i, 240, gradientColor);
  }
  
  // Title with shadow effect
  tft.setTextColor(TFT_DARKGREY);
  tft.setFreeFont(&FreeSansBold12pt7b);
  tft.setCursor(11, 31);
  tft.print("System Info");
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(10, 30);
  tft.print("System Info");
  tft.setTextColor(TFT_WHITE);
  tft.setFreeFont(LABEL2_FONT);
  tft.setTextSize(1);
  
  // Draw back button that returns to Settings
  screenButton.initButton(&tft, 200, 20, 60, 30, TFT_WHITE, TFT_BLUE, TFT_WHITE, backButtonLabel, 1);
  screenButton.drawButton();

   // Display CPU frequency
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(10, 70);
  tft.print("CPU: ");
  tft.setTextColor(TFT_GREEN);
  tft.print(F_CPU / 1000000);
  tft.print(" MHz"); 

  // Display RP2040 temperature
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(10, 90);
  tft.print("Temp: ");
  tft.setTextColor(TFT_GREEN);
  tft.print(analogReadTemp());
  tft.print(" C");

  // Display free heap memory
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(10,110);
  tft.print("Free RAM: ");
  tft.setTextColor(TFT_GREEN);
  tft.print(rp2040.getFreeHeap()/1024);
  tft.print(" KB/ 260KB");
  
    // Display LittleFS information
  tft.setTextColor(TFT_YELLOW);
  tft.setCursor(10, 140);
  tft.print("LittleFS: ");
    tft.setTextColor(TFT_WHITE);
  tft.setCursor(10, 160);
  tft.print("Storage: ");

    if(!LittleFS.begin()) {
      tft.println("LittleFS Mount failed!");
      while (1);
    }
    fs::FSInfo fs_info;
    LittleFS.info(fs_info);
    uint32_t totalBytes= fs_info.totalBytes;
    uint32_t usedBytes= (totalBytes - fs_info.usedBytes);
  tft.setTextColor(TFT_GREEN);
    tft.print(usedBytes/1024);
      tft.print("KB / ");
    tft.print(totalBytes/1024);
          tft.print("KB");
  // Display LittleFS information
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(10, 180);
  tft.print("Number of Files: ");
    tft.setTextColor(TFT_GREEN);
    
  // Count files in root directory
  File root = LittleFS.open("/", "r");
  int fileCount = 0;
  while (File file = root.openNextFile()) {
    fileCount++;
    file.close();
  }
  tft.print(fileCount);

  // I2C Device Detection
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(10, 210);
  tft.print("I2C0 Bus:");
  tft.setCursor(10, 270);
  tft.print("I2C1 Bus:");
  
  // Draw bus lines with improved visibility
  tft.drawFastHLine(20, 235, 200, TFT_WHITE);    // I2C0 bus line      
  tft.drawFastHLine(20, 300, 200, TFT_WHITE);    // I2C1 bus line


  // Scan both I2C buses
  bool foundDevice = false;
  bool foundDevice1 = false;
  int yPos = 230;
  int xPos = 20;
  delay(10);  // Small delay before I2C scan
  
  // Scan I2C0
  for (byte address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    byte error = Wire.endTransmission();
    
    if (error == 0) {      
      if (!foundDevice) foundDevice = true;
      int xPos = map(address, 0, 127, 20, 200);  // Map address to x position
      
      // Draw larger device box
      tft.fillRect(xPos-15, 220, 28, 28, TFT_DARKCYAN);
      tft.drawRoundRect(xPos-15, 220, 30, 30,5, TFT_WHITE);
      
      // Draw address with larger font
      tft.setTextColor(TFT_WHITE);
      tft.setFreeFont(LABEL2_FONT);  // Use larger font
      tft.setCursor(xPos-12, 240);
      if (address < 16) tft.print("0");
      tft.print(address, HEX);
    }
  }
  
  // Scan I2C1
  yPos = 300;
  for (byte address = 1; address < 127; address++) {
    Wire1.beginTransmission(address);
    byte error = Wire1.endTransmission();
    
    if (error == 0) {
      if (!foundDevice1) foundDevice1 = true;
      int xPos = map(address, 0, 127, 20, 200);  // Map address to x position
      
      // Draw larger device box
      tft.fillRect(xPos-15, 285, 28, 28, TFT_DARKCYAN);
      tft.drawRoundRect(xPos-15, 285, 30, 30,5, TFT_WHITE);
      
      // Draw address with larger font
      tft.setTextColor(TFT_WHITE);
      tft.setFreeFont(LABEL2_FONT);  // Use larger font
      tft.setCursor(xPos-12, 305);
      if (address < 16) tft.print("0");
      tft.print(address, HEX);
   }
  }

  if (!foundDevice) {
    tft.drawFastHLine(20, 235, 200, TFT_BLACK);    
    tft.setTextColor(TFT_RED);
    tft.setFreeFont(LABEL2_FONT);  // Use larger font
    tft.setCursor(10, 240);
    tft.print("No I2C devices found");
  }
  
  if (!foundDevice1) {
    tft.drawFastHLine(20, 300, 200, TFT_BLACK); 
    tft.setTextColor(TFT_RED);
    tft.setFreeFont(LABEL2_FONT);  // Use larger font
    tft.setCursor(10, 300);
    tft.print("No I2C devices found");
  }
}
