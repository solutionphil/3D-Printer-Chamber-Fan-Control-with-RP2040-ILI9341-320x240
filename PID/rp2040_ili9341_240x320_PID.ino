/*
 * Program Name: RP2040 TFT Touch UI with PWM Brightness Control
 * Version: 1.0
 * Author: Solutionphil
 * Date: 01/21/2025
 *
 * Description:
 * This program is designed for the RP2040 microcontroller to interface with an ILI9341 TFT display.
 * It provides a touch-based UI with multiple screens, PWM-based brightness control, PWM-based fan control,
 * and a file explorer using LittleFS. The program includes features like touch calibration, slider-based
 * brightness and fan speed adjustment, and file management.
 *
 * Hardware:
 * - RP2040 microcontroller
 * - ILI9341 TFT display with touch screen
 * - PWM pins for brightness control and fan control
 *
 * Libraries:
 * - TFT_eSPI: For TFT display control
 * - RP2040_PWM: For PWM-based brightness and fan speed control
 * - Adafruit_NeoPixel: For NeoPixel control
 * - Adafruit_BME280: For environmental sensing
 * - Adafruit_SGP40: For air quality sensing
 * - Wire: For I2C communication
 * - QuickPID: For PID control
 */
#include <Adafruit_NeoPixel.h>
#include <QuickPID.h>     // Add QuickPID library
#include <Adafruit_BME280.h>
#include <LittleFS.h>
#include <Adafruit_SGP40.h>
#include <Wire.h>
#include <SPI.h>
#include <TFT_eSPI.h>      // Hardware-specific library
#include <TFT_eWidget.h>  // Widget library for sliders
#include "RP2040_PWM.h"   // PWM library

// Sensor status flags
bool bme280_initialized = false;
bool sgp40_initialized = false;

#define PID_SETTINGS_FILE "/pid_settings.txt"
// Initialize TFT
TFT_eSPI tft = TFT_eSPI(); // Invoke custom library
// Create single shared sprite for all UI elements
TFT_eSprite uiSprite = TFT_eSprite(&tft);
TFT_eSprite knob = TFT_eSprite(&tft);
TFT_eSprite pidSprite = TFT_eSprite(&tft);
TFT_eSprite tempSprite = TFT_eSprite(&tft);
TFT_eSprite controlSprite = TFT_eSprite(&tft);
TFT_eSprite gauge1 = TFT_eSprite(&tft);
TFT_eSprite gauge2 = TFT_eSprite(&tft);
TFT_eSprite gauge3 = TFT_eSprite(&tft);
TFT_eSprite gaugebg = TFT_eSprite(&tft);
TFT_eSprite menuSprite = TFT_eSprite(&tft);
bool gaugesInitialized = false;
bool backgroundDrawn = false;

// PWM frequencies
float frequency = 1831;      // For brightness control
float fanFrequency = 20000;  // Separate frequency for fans

// Fan save delay mechanism
unsigned long lastFanChange = 0;
bool fanChangesPending = false;
const unsigned long FAN_SAVE_DELAY = 2000;  // 2 second delay

// Debounce control
unsigned long lastButtonPress = 0;
const unsigned long DEBOUNCE_DELAY = 250; // 250ms debounce time

#define LED_STATE_FILE "/led_state.txt"
// Global variables and definitions
bool neopixelState = false;  // Track NeoPixel state
const int totalScreens = 8;  // Increase total screens for info screen and fan control
char settingsLabels[5][20] = {"Light", "Files", "LED", "System"};  // Removed "Fans" from Settings menu

// LED control button labels
char ledOnLabel[] = "Turn ON";
char ledOffLabel[] = "Turn OFF";

// Navigation and dialog button labels (updated)
char backButtonLabel[] = "Back";
char yesLabel[] = "Yes";
char noLabel[] = "No";
char backLabel[] = "Back";
char plusLabel[] = "+";
char minusLabel[] = "-";
char pidEnableLabel[] = "Enable PID";
char pidDisableLabel[] = "Disable PID";

SliderWidget slider1 = SliderWidget(&tft, &knob);
SliderWidget slider2 = SliderWidget(&tft, &knob);
SliderWidget slider3 = SliderWidget(&tft, &knob);

#define _PWM_LOGLEVEL_        1
#define CALIBRATION_FILE "/TouchCalData1" // Calibration data file
#define REPEAT_CAL false
#define pinToUse      7
#define BRIGHTNESS_FILE "/brightness.txt"

// NeoPixel definitions
#define NEOPIXEL_PIN 16
#define NUM_PIXELS 1

// Initialize NeoPixel
Adafruit_NeoPixel pixels(NUM_PIXELS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);
uint32_t currentColor = 0;

// I2C pins
#define I2C0_SDA 8
#define I2C0_SCL 9

#define I2C1_SDA 10  // Second I2C SDA pin
#define I2C1_SCL 11  // Second I2C SCL pin

// Environmental sensors
Adafruit_BME280 bme;
Adafruit_SGP40 sgp;
unsigned long lastSensorUpdate = 0;
const unsigned long SENSOR_UPDATE_INTERVAL = 2000; // 2 seconds

// Define constants for screen dimensions and colors
#define DISP_X 1
#define DISP_Y 10
#define DISP_W 238
#define DISP_H 50
#define DISP_TCOLOR TFT_CYAN
#define NUM_LEN 12
#define STATUS_X 120

// Fonts for key labels
#define LABEL1_FONT &FreeSansOblique12pt7b // Key label font 1 
#define LABEL2_FONT &FreeSansBold9pt7b    // Key label font 2
#define LABEL3_FONT &TomThumb  //
#define LABEL4_FONT &FreeSerifBold9pt7b    
#define LABEL5_FONT &FreeMonoBold9pt7b

RP2040_PWM* PWM_Instance;

// Initialize PWM instance for brightness control
float dutyCycle; //= 90;  // Default brightness value

// PID Control variables
float pidSetpoint = 25.0f;    // Target temperature
float pidInput;              // Current temperature
float pidOutput;             // PID output (0-100 for fan speed)
bool pidEnabled = false;      // PID control enabled flag

// PID parameters
float Kp = 2.0f;
float Ki = 5.0f;
float Kd = 1.0f;

// Initialize PID
QuickPID myPID(&pidInput, &pidOutput, &pidSetpoint);

// PWM Fan Control
#define FAN1_PIN 27
#define FAN2_PIN 28
#define FAN3_PIN 29
#define FAN_SETTINGS_FILE "/fan_settings.txt"
RP2040_PWM* Fan_PWM[3];

// Global array to store current fan speeds
float currentFanSpeeds[3] = {0.0, 0.0, 0.0};

// Fan sync control
bool fanSyncEnabled = false;

TFT_eSPI_Button screenButton;  // Button to switch screens
TFT_eSPI_Button fileButtons[10]; // Buttons for file explorer
String fileNames[10]; // Array to store file names
String buttonLabels[10];
TFT_eSPI_Button backButton;
TFT_eSPI_Button yesButton;
TFT_eSPI_Button noButton;
TFT_eSPI_Button mainMenuButtons[5]; // Buttons for main menu

int currentScreen = 0;

// Function to cleanup sprites and free memory
void cleanupSprites() {
    gaugesInitialized = false;
    backgroundDrawn = false;
    // Only clean up sprites that aren't needed for the next screen
    if (currentScreen != 0 && uiSprite.created()) {
        uiSprite.deleteSprite();
        Serial.println("Cleaned uiSprite");
    }
    if (currentScreen != 2 && knob.created()) {
        knob.deleteSprite();
        Serial.println("Cleaned knob");
    }
    if (gauge1.created()) {
        gauge1.deleteSprite();
        Serial.println("Cleaned gauge1");
    }
    if (gauge2.created()) {
        gauge2.deleteSprite();
        Serial.println("Cleaned gauge2");
    }
    if (gauge3.created()) {
        gauge3.deleteSprite();
        Serial.println("Cleaned gauge3");
    }
    if (gaugebg.created()) {
        gaugebg.deleteSprite();
        Serial.println("Cleaned gaugebg");
    }
    if (menuSprite.created()) {
        menuSprite.deleteSprite();
        Serial.println("Cleaned menuSprite");
    }
    
    // Log available memory after cleanup
    Serial.printf("Free RAM after cleanup: %d bytes\n", rp2040.getFreeHeap());
}

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

void setNeoPixelColor(int screenNumber) {
  if (!neopixelState) return; // Don't change colors if LED is off

  switch(screenNumber) {
    case 0: // Main Menu
      pixels.setPixelColor(0, pixels.Color(0, 0, 255)); // Blue
      break;
    case 1: // PID
      pixels.setPixelColor(0, pixels.Color(255, 0, 0)); // Red
      break;
    case 2: // Brightness Screen
      pixels.setPixelColor(0, pixels.Color(255, 255, 0)); // Yellow
      break;
    case 3: // Temperature Screen
      pixels.setPixelColor(0, pixels.Color(0, 255, 0)); // Green
      break;
    case 4: // File Explorer
      pixels.setPixelColor(0, pixels.Color(128, 0, 128)); // Purple
      break;
    case 5: // Settings
      pixels.setPixelColor(0, pixels.Color(255, 128, 0)); // Orange
      break;
    case 6: // LED Control
      pixels.setPixelColor(0, pixels.Color(255, 255, 255)); // White
      break;
    case 7: // Fan Control
      pixels.setPixelColor(0, pixels.Color(0, 255, 255)); // Cyan
      break;
    case 8: // System Info
      pixels.setPixelColor(0, pixels.Color(0, 255, 255)); // Cyan
      break;
  }
  pixels.show();
}

void setup() {

  // Initialize serial communication for debugging
  Serial.begin(9600);  
  // Initialize I2C0
  Wire.setSDA(I2C0_SDA);
  Wire.setSCL(I2C0_SCL);
  Wire.begin();
  
  // Initialize I2C1 with proper constructor
  //TwoWire Wire1(i2c1, I2C1_SDA, I2C1_SCL);
  //Wire1.begin();
  Wire1.setSDA(I2C1_SDA);
  Wire1.setSCL(I2C1_SCL);
  Wire1.begin();

  // Initialize menu sprite
  menuSprite.createSprite(240, 320);

  // Initialize knob sprite once during setup
  knob.setColorDepth(8);
  knob.createSprite(30, 40);
 
  // Initialize LittleFS and load saved brightness
  if (!LittleFS.begin()) {
    LittleFS.format();
    LittleFS.begin();
  }
  
  // Initialize NeoPixel and load state before any operations
  pixels.begin();
  pixels.setBrightness(50);
  delay(100); // Small delay for stable initialization

  // Load saved brightness
  dutyCycle = loadBrightness();
  
  // Load saved LED state
  neopixelState = loadLEDState();
  
  // Apply LED state and show immediately
  if (neopixelState) {
    setNeoPixelColor(0); // Set initial color for main menu
  } else {
    pixels.setPixelColor(0, 0); // LED off
  }
  pixels.show();

  // Set up PWM for brightness control
  PWM_Instance = new RP2040_PWM(pinToUse, frequency, dutyCycle);

  delay(1000);
  PWM_Instance->setPWM(pinToUse, frequency, dutyCycle);

  // Initialize PWM for fans
  for (int i = 0; i < 3; i++) {
    Fan_PWM[i] = new RP2040_PWM(FAN1_PIN + i, fanFrequency, 0);
  }
  
  // Initialize PID controller
  myPID.SetTunings(Kp, Ki, Kd);
  myPID.SetMode(myPID.Control::manual);
  myPID.SetOutputLimits(0, 100);
  myPID.SetSampleTimeUs(100000); // 100ms update rate
  
  // Load PID settings from LittleFS
  loadPIDSettings();
  
  // Load saved fan speeds
  float fanSpeeds[3] = {0, 0, 0};
  loadFanSpeeds(fanSpeeds);
  for (int i = 0; i < 3; i++) {
    currentFanSpeeds[i] = fanSpeeds[i];
    Fan_PWM[i]->setPWM(FAN1_PIN + i, fanFrequency, fanSpeeds[i]);
  }

  // Initialize the TFT display and set its rotation (Main Menu updated)
  tft.init();  
  tft.setRotation(0);  

  // Initialize BME280
  if (bme.begin(0x76, &Wire)) {
    Serial.println("BME280 sensor found!");
    bme280_initialized = true;
  } else {
    Serial.println("Could not find BME280 sensor!");
    bme280_initialized = false;
  }

  // Initialize SGP40
  if (sgp.begin()) {
    Serial.println("SGP40 sensor found!");
    sgp40_initialized = true;
  } else {
    Serial.println("Could not find SGP40 sensor!");
    sgp40_initialized = false;
  }

  // Small delay to ensure stable sensor initialization
  delay(100);

  //Calibrate the touch screen and display the initial screen
  touch_calibrate();  
  displayScreen(currentScreen);  
}

void loop(void) {
  // Main loop to handle touch input and update screens
  uint16_t t_x = 0, t_y = 0;  // Variables to store touch coordinates
  bool pressed = tft.getTouch(&t_x, &t_y);  // Boolean indicating if the screen is being touched

  // Check if fan changes need to be saved
  if (fanChangesPending && (millis() - lastFanChange >= FAN_SAVE_DELAY)) {
    saveFanSpeeds(currentFanSpeeds);
    fanChangesPending = false;
    Serial.println("Saved fan settings after delay");
  }

  // Handle temperature and air quality screen updates
  if (currentScreen == 3) {
    unsigned long currentMillis = millis();
    if (currentMillis - lastSensorUpdate >= SENSOR_UPDATE_INTERVAL)
      updateTempAndAirQualityDisplay();
  }

  if (currentScreen == 0) {  // Main menu screen (System Info removed)
    for (uint8_t b = 0; b < 4; b++) {  // Adjusted loop to 3 buttons
      mainMenuButtons[b].press(pressed && mainMenuButtons[b].contains(t_x, t_y));  // Update button state
    }

    for (uint8_t b = 0; b < 4; b++) {  // Adjusted loop to 4 buttons
      if (mainMenuButtons[b].justReleased()) mainMenuButtons[b].drawButton();  // Draw normal state
      if (mainMenuButtons[b].justPressed()) {
        unsigned long currentTime = millis();
        if (currentTime - lastButtonPress >= DEBOUNCE_DELAY) {
          lastButtonPress = currentTime + 250; // Add extra delay for button presses
          switch(b) {
            case 0: currentScreen = 7; break;  // Fans (moved from Settings)
            case 1: currentScreen = 3; break;  // Temperature and Air Quality
            case 2: currentScreen = 1; break;  // PID
            case 3: currentScreen = 5; break;  // Settings
          }
          displayScreen(currentScreen);  // Display the selected screen
        }
      }
    }
  } else if (currentScreen == 5) {  // Settings menu
    for (uint8_t b = 0; b < 4; b++) {  // Adjusted to 4 buttons for Settings
      mainMenuButtons[b].press(pressed && mainMenuButtons[b].contains(t_x, t_y));
      if (mainMenuButtons[b].justPressed()) {
        unsigned long currentTime = millis();
        if (currentTime - lastButtonPress >= DEBOUNCE_DELAY) {
          lastButtonPress = currentTime;
          if (b == 0) currentScreen = 2;      // Brightness
          else if (b == 1) {  // File Explorer
            displayLoadingScreen();
            currentScreen = 4;
          }
          else if (b == 2) currentScreen = 6;  // LED Control
          else if (b == 3) currentScreen = 8;  // System Info
          displayScreen(currentScreen);
        }
      }
    }
  } else if (currentScreen == 6) {
    mainMenuButtons[0].press(pressed && mainMenuButtons[0].contains(t_x, t_y));
    if (mainMenuButtons[0].justPressed()) {
      neopixelState = !neopixelState;
      if (!neopixelState) {
        pixels.setPixelColor(0, 0);
      } else {
        pixels.setPixelColor(0, pixels.Color(255, 255, 255));
        setNeoPixelColor(currentScreen);
      }
      pixels.show();
      saveLEDState(neopixelState);
      displayLEDControl();
      delay(500);
    }
  } else if (currentScreen == 7) {
    for (uint8_t b = 0; b < 3; b++) {
      mainMenuButtons[b].press(pressed && mainMenuButtons[b].contains(t_x, t_y));
      if (mainMenuButtons[b].justPressed()) {
        unsigned long currentTime = millis();
        if (currentTime - lastButtonPress >= DEBOUNCE_DELAY) {
          lastButtonPress = currentTime;
          displayFanControl(b);
        }
      }
    }
  } else if (currentScreen == 2) {
    if (pressed) {
      if (slider1.checkTouch(t_x, t_y)) {
        // Handle slider touch logic
        dutyCycle = round(slider1.getSliderPosition() / 10.0) * 10.0; // Snap to nearest 10% increment
        slider1.setSliderPosition(dutyCycle); // Update slider position to snapped value
        PWM_Instance->setPWM(pinToUse, frequency, dutyCycle);
        saveBrightness(dutyCycle);  // Save the new brightness value
        // Update percentage display
        tft.fillRect(90, 110, 80, 30, TFT_BLACK);
        tft.setTextColor(TFT_GREEN);
        tft.drawString(String(int(dutyCycle)) + "%", 100, 120);
      }
    }
  }

  // Check for touch on the screen switch button
  if(currentScreen!=0){
    screenButton.press(pressed && screenButton.contains(t_x, t_y));
    if (screenButton.justReleased()) screenButton.drawButton();

    // Switch to the next screen when the button is pressed
      if (screenButton.justPressed()) {
        unsigned long currentTime = millis();
        if (currentTime - lastButtonPress >= DEBOUNCE_DELAY) {
          lastButtonPress = currentTime;
          // Return to settings menu for specific screens
          if (currentScreen == 2 || currentScreen == 4 || currentScreen == 6 || currentScreen == 8) {
            currentScreen = 5;  // Return to Settings
          } else {
            currentScreen = 0;  // Return to Main Menu for other screens
          }
          displayScreen(currentScreen);
      }
    }
  }

  // Check for touch on file explorer buttons only on screen 4
  if (currentScreen == 4) {
    // Clear any residual touch data to prevent accidental button presses
    t_x=0;
    t_y=0;
    while (tft.getTouch(&t_x, &t_y)) {}

    // Additional delay specifically for file explorer to prevent immediate touch processing
    delay(100);

    for (uint8_t i = 0; i < 10; i++) {
      fileButtons[i].press(pressed && fileButtons[i].contains(t_x, t_y));

      // Handle file button interactions
      if (fileButtons[i].justReleased()) fileButtons[i].drawButton();
      if (fileButtons[i].justPressed()) handleFileButtonPress(i);
    }
  }
}

// Function to draw gauge on sprite
void drawGaugeToSprite(TFT_eSprite* sprite, int x, int y, float min_val, float max_val, float value, const char* label, uint16_t color, uint16_t bgColor) {
  sprite->fillSprite(TFT_BLACK);
  
  // Calculate scaling factor for VOC gauge (98px vs 140px)
  float scale = (sprite->width() == 98) ? 0.7 : 1.0;
  int outerRadius = round(62 * scale);
  int innerRadius = round(58 * scale);
  bool isVOC = (sprite->width() == 98);
  int fillRadius = round(54 * scale);
  int centerRadius = round(35 * scale);
  
  // Adjust fill radius for VOC gauge to ensure visibility
  if (sprite->width() == 98) {
    fillRadius = round(58 * scale);  // Increased fill radius for VOC
    innerRadius = round(52 * scale); // Adjusted inner radius
  }

  // Draw outer circle with anti-aliasing
  sprite->drawCircle(x, y, outerRadius, TFT_WHITE);
  for(int i = round(59 * scale); i >= round(58 * scale); i--) {
    sprite->drawCircle(x, y, i, TFT_DARKGREY);
  }

  // Draw tick marks only for non-VOC gauges
  if (!isVOC) {
    int radius = round(58 * scale);
    for (int i = -225; i <= 45; i += 27) {
      float rad = i * PI / 180.0;
      int len = (i == -225 || i == 45 || i == -90) ? round(12 * scale) : round(8 * scale);
      // Draw anti-aliased tick marks
      for(int w = 0; w < 1; w++) {
        sprite->drawLine(
          x + cos(rad) * (radius-w), 
          y + sin(rad) * (radius-w),
          x + cos(rad) * (radius-len-w), 
          y + sin(rad) * (radius-len-w),
          (i == -225 || i == 45 || i == -90) ? TFT_WHITE : TFT_DARKGREY
        );
      }
    }
  }

  // Calculate angles with improved precision
  float startAngle = -225 * PI / 180.0;
  float mappedValue = constrain(value, min_val, max_val);
  float endAngle = -225 + (mappedValue - min_val) * (270) / (max_val - min_val);
  endAngle = endAngle * PI / 180.0;
  
  // Adjust fill radius for VOC gauge
  int fillStart = isVOC ? round(58 * scale) : round(54 * scale);
  int fillEnd = round(42 * scale);
  
  // Draw filled arc with smoother gradient and anti-aliasing
  for (int r = fillStart; r >= fillEnd; r--) {
    float stepSize = 0.01; // Smaller step size for smoother arc
    for (float angle = startAngle; angle <= endAngle; angle += stepSize) {
      float nextAngle = min(angle + stepSize, endAngle);
      // Draw multiple lines for anti-aliasing
      for(int w = 0; w < 2; w++) {
        sprite->drawLine(
          x + cos(angle) * (r-w), 
          y + sin(angle) * (r-w),
          x + cos(nextAngle) * (r-w), 
          y + sin(nextAngle) * (r-w),
          color
        );
      }
    }
  }

  // Draw inner circle with scaled dimensions
  for(int r = round(41 * scale); r >= round(36 * scale); r--) {
    uint8_t shadow = map(r, round(38 * scale), round(36 * scale), 46, 0);
    sprite->drawCircle(x, y, r, sprite->color565(shadow, shadow, shadow));
  }
  sprite->fillCircle(x, y, centerRadius, bgColor);

  // Draw labels and value
  char buf[10];
  if (strstr(label, "VOC")) {
    sprintf(buf, "%d", (int)value); // Integer format for VOC
  } else {
    sprintf(buf, "%.1f", value); // Keep decimal for temp and humidity
  }
  
  sprite->setTextColor(TFT_WHITE, bgColor);
  // Adjust text size and position for VOC gauge
  int textSize = (sprite->width() == 98) ? 2 : 4;
  sprite->setTextDatum(MC_DATUM);  // Set text datum to middle-center
  sprite->drawString(buf, x, y-(round(16 * scale)));
  
  sprite->setTextSize(1);
  sprite->setTextColor(TFT_WHITE, bgColor);
  sprite->drawString(label, x, y+5);
}

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
      displayPID();  // Display screen 1
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
  mainMenuButtons[1].initButton(&menuSprite, 120, 160, 220, 40, TFT_WHITE, TFT_BLUE, TFT_WHITE, (char*)"AQI", 1);
  mainMenuButtons[2].initButton(&menuSprite, 120, 220, 220, 40, TFT_WHITE, TFT_BLUE, TFT_WHITE, (char*)"PID", 1);
  mainMenuButtons[3].initButton(&menuSprite, 120, 280, 220, 40, TFT_WHITE, TFT_DARKGREY, TFT_WHITE, (char*)"Settings", 1);

  for (uint8_t i = 0; i < 4; i++) {  // Adjusted loop to 3 buttons
    mainMenuButtons[i].drawButton();
  }
  
  // Push sprite to display
  menuSprite.pushSprite(0, 0);
}

void displayPID() {
  // Create sprites with proper dimensions for 240x320 display
  if (!pidSprite.created()) {
    pidSprite.createSprite(240, 320);
    pidSprite.setTextColor(TFT_WHITE);
    pidSprite.setFreeFont(LABEL2_FONT);
  }
  if (!tempSprite.created()) {
    tempSprite.createSprite(220, 80);  // Larger sprite for temperature display
    tempSprite.setTextColor(TFT_WHITE);
    tempSprite.setFreeFont(LABEL2_FONT);
  }
  if (!controlSprite.created()) {
    controlSprite.createSprite(220, 60);  // Adjusted for fan control display
    controlSprite.setTextColor(TFT_WHITE);
    controlSprite.setFreeFont(LABEL2_FONT);
  }

  // Draw title centered at top
  controlSprite.fillSprite(TFT_BLACK);
  controlSprite.setTextDatum(MC_DATUM);  // Set text datum to middle-center
  controlSprite.drawString("Fan Speed", 110, 5);
  controlSprite.drawString(String(int(pidOutput)) + "%", 110, 25);
  controlSprite.pushSprite(10, 250);

  // Display current temperature
  float currentTemp = bme.readTemperature();
  pidInput = currentTemp;
  
  // Draw temperature displays in larger format
  tempSprite.fillSprite(TFT_BLACK);
  tempSprite.drawString("Current:", 20, 10);
  tempSprite.setTextFont(4);  // Larger font for temperature
  tempSprite.drawString(String(currentTemp, 1) + "Â°C", 120, 10);
  tempSprite.setTextFont(2);
  tempSprite.drawString("Target:", 20, 45);
  tempSprite.setTextFont(4);
  tempSprite.drawString(String(pidSetpoint, 1) + "Â°C", 120, 45);
  tempSprite.pushSprite(10, 60);

  // Position setpoint control buttons
  mainMenuButtons[0].initButton(&pidSprite, 190, 85, 40, 35, TFT_WHITE, TFT_BLUE, TFT_WHITE, plusLabel, 1);
  mainMenuButtons[1].initButton(&pidSprite, 190, 125, 40, 35, TFT_WHITE, TFT_BLUE, TFT_WHITE, minusLabel, 1);
  mainMenuButtons[0].drawButton();
  mainMenuButtons[1].drawButton();

  // Draw PID enable/disable button
  mainMenuButtons[2].initButton(&pidSprite, 120, 200, 200, 40, TFT_WHITE, 
    pidEnabled ? TFT_RED : TFT_GREEN, TFT_WHITE, 
    pidEnabled ? pidDisableLabel : pidEnableLabel, 1);
  mainMenuButtons[2].drawButton();

  // Draw fan speed indicator
  controlSprite.fillSprite(TFT_BLACK);
  controlSprite.setTextDatum(MC_DATUM);  // Set text datum to middle-center
  controlSprite.drawString("Fan Speed", 110, 5);
  controlSprite.drawString(String(int(pidOutput)) + "%", 110, 25);
  controlSprite.pushSprite(10, 250);
}

void displayBGBrightness() {
  // Clear screen and set up title
  tft.fillScreen(TFT_BLACK);
  
  // Draw back button
  screenButton.initButton(&tft, 200, 20, 60, 30, TFT_WHITE, TFT_BLUE, TFT_WHITE, backButtonLabel, 1);
  screenButton.drawButton();
  
  // Ensure knob sprite is ready
  if (!knob.created()) knob.createSprite(30, 40);
  
  tft.setTextColor(TFT_CYAN);
  tft.setFreeFont(LABEL2_FONT);
  tft.setTextSize(0);
  tft.drawString("Brightness", 30, 50);

  // Update percentage display
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
}
void displayTempAndAirQuality() {
  tft.fillScreen(TFT_BLACK);
  
  if (!backgroundDrawn) {
    tft.setTextColor(TFT_WHITE);
    tft.setFreeFont(LABEL2_FONT);
    tft.setTextSize(1);
    backgroundDrawn = true;
  }

  // Draw back button
  screenButton.initButton(&tft, 200, 20, 60, 30, TFT_WHITE, TFT_BLUE, TFT_WHITE, backButtonLabel, 1);
  screenButton.drawButton();

  // Only initialize BME280 and SGP40 once
  if (!bme280_initialized) {
    tft.setCursor(10, 70);
    tft.print("BME280 not found!");
    delay(2000);  // Show error for 2 seconds
    return;
  }
  
  if (!sgp40_initialized) {
    tft.setCursor(10, 90);
    tft.print("SGP40 not found!");
    delay(2000);  // Show error for 2 seconds
    return;
  }

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

  float temp = bme.readTemperature();
  float hum = bme.readHumidity();
  int32_t voc_index = sgp.measureVocIndex(temp, hum);
  uint16_t voc_color = TFT_DARKGREEN ;
  if (voc_index > 100) voc_color = TFT_GREEN;
  if (voc_index > 200) voc_color = TFT_YELLOW;
  if (voc_index > 300) voc_color = TFT_ORANGE;
  if (voc_index > 400) voc_color = TFT_RED;
  
  // Draw to sprites instead of screen
  drawGaugeToSprite(&gauge1, 70, 65, 0, 60, temp, "Temp C", TFT_RED, 0x8800);
  drawGaugeToSprite(&gauge2, 70, 75, 0, 100, hum, "Feuchte %", TFT_BLUE, 0x0011);
  drawGaugeToSprite(&gauge3, 49, 52, 0, 500, voc_index, "VOC", voc_color, 0x0011); // Adjusted center coordinates for smaller gauge
  
  // Push sprites to screen with VOC gauge on the right (pre-scaled)
  gauge1.pushSprite(7, 30);
  gauge2.pushSprite(7, 160);
  gauge3.pushSprite(142, 110);
}


void updateTempAndAirQualityDisplay() {
  unsigned long currentMillis = millis();
  lastSensorUpdate = currentMillis;
  
  // Only update if sprites are initialized
  if (!gaugesInitialized) {
    displayTempAndAirQuality();
    return;
  }
   
  float temp = bme.readTemperature();
  float hum = bme.readHumidity();
  
  // Get VOC reading with temperature/humidity compensation
  int32_t voc_index = sgp.measureVocIndex(temp, hum);
  uint16_t voc_color = TFT_GREEN;
  if (voc_index > 100) voc_color = TFT_YELLOW;
  if (voc_index > 200) voc_color = TFT_ORANGE;
  if (voc_index > 300) voc_color = TFT_RED;
  
  drawGaugeToSprite(&gauge1, 70, 65, 0, 60, temp, "Temp C", TFT_RED, 0x8800);
  drawGaugeToSprite(&gauge2, 70, 75, 0, 100, hum, "Feuchte %", TFT_BLUE, 0x0011);
  drawGaugeToSprite(&gauge3, 49, 52, 0, 500, voc_index, "VOC", voc_color, 0x0011); // Adjusted center coordinates for smaller gauge
  
  // Push updated sprites to screen with VOC gauge on the right (pre-scaled)
  gauge1.pushSprite(7, 30);
  gauge2.pushSprite(7, 160);
  gauge3.pushSprite(142, 110);
}
void displayFileExplorer() {
  // Don't clean up sprites here, let displayScreen handle it
  tft.fillScreen(TFT_BLACK);

  // Create necessary sprites for file explorer
  if (!menuSprite.created()) {
    menuSprite.createSprite(240, 320);
  }
  
  tft.setTextColor(TFT_WHITE);
  tft.setFreeFont(LABEL2_FONT);
  tft.setTextSize(1);
  tft.setCursor(10, 20);
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
  menuSprite.setFreeFont(LABEL2_FONT);
  menuSprite.setTextSize(1);
  menuSprite.setCursor(10, 50);
  menuSprite.print("Settings");

  // Initialize settings menu buttons
  for (int i = 0; i < 4; i++) {  // Changed to 4 for removed "Fans" button
    mainMenuButtons[i].initButton(&menuSprite, 120, 100 + (i * 60), 220, 40, TFT_WHITE, TFT_BLUE, TFT_WHITE, settingsLabels[i], 1);
    mainMenuButtons[i].drawButton();
  }
  
  menuSprite.pushSprite(0, 0);
}

void displayLEDControl() {
  bool currentState = neopixelState; // Store current state to detect changes
  tft.fillScreen(TFT_BLACK);

  tft.setTextColor(TFT_WHITE);
  
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
            
            // Save individual fan speed changes
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

void displayInfoScreen() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(1);
  
  // Draw back button that returns to Settings
  screenButton.initButton(&tft, 200, 20, 60, 30, TFT_WHITE, TFT_BLUE, TFT_WHITE, backButtonLabel, 1);
  screenButton.drawButton();

  // Rest of header
  tft.setFreeFont(LABEL2_FONT);
  tft.setCursor(10, 20);
  tft.print("System Info");
  tft.setFreeFont(LABEL2_FONT);

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

void savePIDSettings() {
  File f = LittleFS.open(PID_SETTINGS_FILE, "w");
  if (f) {
    f.println(Kp);
    f.println(Ki);
    f.println(Kd);
    f.println(pidSetpoint);
    f.println(pidEnabled ? "1" : "0");
    f.close();
    Serial.println("PID settings saved successfully");
  } else {
    Serial.println("Failed to save PID settings");
  }
}

void loadPIDSettings() {
  if (LittleFS.exists(PID_SETTINGS_FILE)) {
    File f = LittleFS.open(PID_SETTINGS_FILE, "r");
    if (f) {
      String kp = f.readStringUntil('\n');
      String ki = f.readStringUntil('\n');
      String kd = f.readStringUntil('\n');
      String sp = f.readStringUntil('\n');
      String en = f.readStringUntil('\n');
      
      // Remove any whitespace/newlines
      kp.trim(); ki.trim(); kd.trim(); sp.trim(); en.trim();
      
      Kp = kp.toFloat();
      Ki = ki.toFloat();
      Kd = kd.toFloat();
      pidSetpoint = sp.toFloat();
      pidEnabled = (en == "1");
      
      f.close();
      Serial.println("PID settings loaded successfully");
    } else {
      Serial.println("Failed to load PID settings");
    }
  }
}
