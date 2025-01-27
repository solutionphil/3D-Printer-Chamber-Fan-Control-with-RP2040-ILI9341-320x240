void saveBrightness(float value) {
  File f = LittleFS.open(BRIGHTNESS_FILE, "w");
  if (f) {
    f.println(value);
    f.close();
  }
}

float loadBrightness() {
  if (LittleFS.exists(BRIGHTNESS_FILE)) {
    File f = LittleFS.open(BRIGHTNESS_FILE, "r");
    if (f) {
      String val = f.readStringUntil('\n');
      f.close();
      return val.toFloat();
    }
  }
  return 90.0; // Default brightness if file doesn't exist
}

void saveLEDState(bool state) {
  File f = LittleFS.open(LED_STATE_FILE, "w");
  if (f) {
    f.println(state ? "1" : "0");
    f.close();
  }
}

bool loadLEDState() {
  if (LittleFS.exists(LED_STATE_FILE)) {
    File f = LittleFS.open(LED_STATE_FILE, "r");
    if (!f) {
      return false;  // Return false if file can't be opened
    }
    String val = f.readStringUntil('\n');
    f.close();
    // Trim whitespace and compare exact string
    val.trim();
    return (val == "1");
  }
  return false; // Default LED state to OFF if file doesn't exist
}

void saveFanSpeeds(float speeds[3]) {
  File f = LittleFS.open(FAN_SETTINGS_FILE, "w");
  if (f) {
    Serial.println("Saving fan speeds to file");
    // Write each speed on a new line
    for (int i = 0; i < 3; i++) {
      Serial.printf("Saving Fan %d: %.1f%%\n", i+1, speeds[i]);
      f.println(speeds[i]);
    }
    f.close();
    Serial.println("Fan speeds saved successfully");
  } else {
    Serial.println("Failed to open fan settings file for writing");
  }
}

void loadFanSpeeds(float speeds[3]) {

  // Initialize with default values
  for (int i = 0; i < 3; i++) {
    speeds[i] = 0.0;
  }
  
  if (LittleFS.exists(FAN_SETTINGS_FILE)) {
    Serial.println("Loading fan speeds from file");
    File f = LittleFS.open(FAN_SETTINGS_FILE, "r");
    if (f) {
      for (int i = 0; i < 3; i++) {
        String val = f.readStringUntil('\n');
        Serial.printf("Read value for Fan %d: %s\n", i+1, val.c_str());
        if (val.length() > 0) {
          speeds[i] = constrain(val.toFloat(), 0, 100);
        }
      }
      f.close();
      Serial.println("Fan speeds loaded successfully");
    } else {
      Serial.println("Failed to open fan settings file for reading");
    }
  }
}

void savePIDSettings() {
  File f = LittleFS.open(PID_SETTINGS_FILE, "w");
  if (f) {
    f.println(Setpoint);
    f.println(PIDactive ? "1" : "0");
    f.close();
  }
}

void loadPIDSettings() {
  if (LittleFS.exists(PID_SETTINGS_FILE)) {
    File f = LittleFS.open(PID_SETTINGS_FILE, "r");
    if (f) {
      String setpoint = f.readStringUntil('\n');
      String active = f.readStringUntil('\n');
      Setpoint = setpoint.toFloat();
      active.trim();  // Trim the string first
      PIDactive = (active == "1");  // Then compare
      f.close();
    }
  }
}