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
 * - Wire: For I2C communication
 */

#include <LittleFS.h>
#include <SPI.h>
#include <TFT_eSPI.h>      // Hardware-specific library
#include <TFT_eWidget.h>  // Widget library for sliders
#include "RP2040_PWM.h"   // PWM library
#include <Adafruit_NeoPixel.h>
#include <Wire.h>

// PWM frequencies
float frequency = 1831;      // For brightness control
float fanFrequency = 20000;  // Separate frequency for fans

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

// Initialize TFT and slider objects (updated for Fans in Main Menu)
TFT_eSPI tft = TFT_eSPI(); // Invoke custom library
TFT_eSprite knob = TFT_eSprite(&tft); // Create TFT sprite for slider knob
SliderWidget slider1 = SliderWidget(&tft, &knob);
SliderWidget slider2 = SliderWidget(&tft, &knob);
SliderWidget slider3 = SliderWidget(&tft, &knob);

// Create sprite for main menu
TFT_eSprite menuSprite = TFT_eSprite(&tft);

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

RP2040_PWM* PWM_Instance;

// Initialize PWM instance for brightness control
float dutyCycle; //= 90;  // Default brightness value

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
    Serial.println("Saving fan speeds");
    // Write each speed on a new line
    for (int i = 0; i < 3; i++) {
      f.println(speeds[i]);
    }
    f.close();
  }
}

void loadFanSpeeds(float speeds[3]) {
  // Initialize with default values
  for (int i = 0; i < 3; i++) {
    Serial.println("Loading fan speeds");
    speeds[i] = 0.0;
  }
  
  if (LittleFS.exists(FAN_SETTINGS_FILE)) {
    File f = LittleFS.open(FAN_SETTINGS_FILE, "r");
    if (f) {
      for (int i = 0; i < 3; i++) {
        String val = f.readStringUntil('\n');
        if (val.length() > 0) {
          speeds[i] = constrain(val.toFloat(), 0, 100);
        }
      }
      f.close();
    }
  }
}

void setNeoPixelColor(int screenNumber) {
  if (!neopixelState) return; // Don't change colors if LED is off

  switch(screenNumber) {
    case 0: // Main Menu
      pixels.setPixelColor(0, pixels.Color(0, 0, 255)); // Blue
      break;
    case 1: // Screen 1
      pixels.setPixelColor(0, pixels.Color(255, 0, 0)); // Red
      break;
    case 2: // Brightness Screen
      pixels.setPixelColor(0, pixels.Color(255, 255, 0)); // Yellow
      break;
    case 3: // Screen 3
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
  TwoWire Wire1(i2c1, I2C1_SDA, I2C1_SCL);
  Wire1.begin();

  // Initialize menu sprite
  menuSprite.createSprite(240, 320);

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

  // Initialize the knob sprite early
  knob.setColorDepth(8);
  knob.createSprite(30, 40);  // Size for the slider knob
  knob.fillSprite(TFT_BLACK);

  delay(1000);
  PWM_Instance->setPWM(pinToUse, frequency, dutyCycle);

  // Initialize PWM for fans
  for (int i = 0; i < 3; i++) {
    Fan_PWM[i] = new RP2040_PWM(FAN1_PIN + i, fanFrequency, 0);
  }

  // Load saved fan speeds
  float fanSpeeds[3] = {0, 0, 0};
  loadFanSpeeds(fanSpeeds);
  for (int i = 0; i < 3; i++) {
    currentFanSpeeds[i] = fanSpeeds[i];
    float dutyCycle = min(1.0, max(0.0, fanSpeeds[i] / 100.0));
    Fan_PWM[i]->setPWM(FAN1_PIN + i, fanFrequency, dutyCycle);
  }

  // Initialize the TFT display and set its rotation (Main Menu updated)
  tft.init();  
  tft.setRotation(0);  

  // Calibrate the touch screen and display the initial screen
  touch_calibrate();  
  displayScreen(currentScreen);  
}

void loop(void) {
  // Main loop to handle touch input and update screens
  uint16_t t_x = 0, t_y = 0;  // Variables to store touch coordinates
  bool pressed = tft.getTouch(&t_x, &t_y);  // Boolean indicating if the screen is being touched

  if (currentScreen == 0) {  // Main menu screen (System Info removed)
    for (uint8_t b = 0; b < 3; b++) {  // Adjusted loop to 3 buttons
      mainMenuButtons[b].press(pressed && mainMenuButtons[b].contains(t_x, t_y));  // Update button state
    }

    for (uint8_t b = 0; b < 3; b++) {  // Adjusted loop to 3 buttons
      if (mainMenuButtons[b].justReleased()) mainMenuButtons[b].drawButton();  // Draw normal state
      if (mainMenuButtons[b].justPressed()) {
        unsigned long currentTime = millis();
        if (currentTime - lastButtonPress >= DEBOUNCE_DELAY) {
          lastButtonPress = currentTime + 250; // Add extra delay for button presses
          switch(b) {
            case 0: currentScreen = 7; break;  // Fans (moved from Settings)
            case 1: currentScreen = 3; break;  // Screen 3
            case 2: currentScreen = 5; break;  // Settings
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
        // Return to main menu for screens 2, 4, and 7
        if (currentScreen == 2 || currentScreen == 4 || currentScreen == 7) {
          currentScreen = 0;
        } else currentScreen = 0;
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
  tft.fillScreen(TFT_BLACK);  // Clear the screen
  
  if (neopixelState) {  // Only update NeoPixel if it's enabled
    setNeoPixelColor(screen);  // Update NeoPixel color
  }

  switch (screen) {
    case 0:
      drawMainMenu();
      break;
    case 1:
      displayScreen1();  // Display screen 1
      break;
    case 2:
      displayScreen2();
      break;
    case 3:
      displayScreen3();
      break;
    case 4:
      displayScreen4();  // Display file explorer
      break;
    case 5:
      displayScreen5();  // Display Settings
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

  // Draw the screen switch button
  if(screen!=0){
    tft.setFreeFont(LABEL2_FONT);
    screenButton.initButton(&tft, 200, 20, 60, 30, TFT_WHITE, TFT_BLUE, TFT_WHITE, backButtonLabel, 1);
    screenButton.drawButton();
  }
}

void drawMainMenu() {
  // Draw to sprite instead of directly to screen
  menuSprite.fillSprite(TFT_BLACK);
  menuSprite.setTextColor(TFT_WHITE);
  menuSprite.setFreeFont(LABEL2_FONT);
  menuSprite.setTextSize(1);
  menuSprite.setCursor(10, 50);
  menuSprite.print("Main Menu");

  // Initialize buttons with menuSprite instead of tft
  mainMenuButtons[0].initButton(&menuSprite, 120, 120, 220, 40, TFT_WHITE, TFT_BLUE, TFT_WHITE, (char*)"Fans", 1); // Replaced Screen 1 with Fans
  mainMenuButtons[1].initButton(&menuSprite, 120, 180, 220, 40, TFT_WHITE, TFT_BLUE, TFT_WHITE, (char*)"Screen 3", 1);
  mainMenuButtons[2].initButton(&menuSprite, 120, 240, 220, 40, TFT_WHITE, TFT_DARKGREY, TFT_WHITE, (char*)"Settings", 1);

  for (uint8_t i = 0; i < 3; i++) {  // Adjusted loop to 3 buttons
    mainMenuButtons[i].drawButton();
  }
  
  // Push sprite to display
  menuSprite.pushSprite(0, 0);
}

void displayScreen1() {
  tft.setTextColor(TFT_WHITE);
  tft.setFreeFont(LABEL2_FONT);
  tft.setTextSize(1);
  tft.setCursor(10, 20);
  tft.print("Screen 1");
}

void displayScreen2() {
  // Clear screen and set up title
  tft.fillScreen(TFT_BLACK);
  
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

void displayScreen3() {
  tft.setTextColor(TFT_WHITE);
  tft.setFreeFont(LABEL2_FONT);
  tft.setTextSize(1);
  tft.setCursor(10, 70);
  tft.print("Screen 3");
}

void displayScreen4() {
  tft.fillScreen(TFT_BLACK);

  // Additional delay specifically for file explorer to prevent immediate touch processing
  delay(100);
  
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

void displayScreen5() {
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

  SliderWidget* sliders[] = {&slider1, &slider2, &slider3};
  
  for (int i = 0; i < 3; i++) {
    int yOffset = i * 90;  // Space between fan controls
    
    // Draw fan labels
    tft.drawString("Fan " + String(i + 1), 30, 40 + yOffset);
    tft.drawString("Speed", 30, 60 + yOffset);
    
    // Draw current speed percentage
    tft.setTextColor(TFT_GREEN);
    tft.drawString(String(int(currentFanSpeeds[i])) + "%", 150, 40 + yOffset);
    tft.setTextColor(TFT_WHITE);
    
    param.startPosition = int16_t(currentFanSpeeds[i]);
    sliders[i]->drawSlider(20, 80 + yOffset, param);
    
    int16_t x, y;
    uint16_t w, h;
    sliders[i]->getBoundingRect(&x, &y, &w, &h);
    tft.drawRect(x, y, w, h, TFT_DARKGREY);
    sliders[i]->setSliderPosition(currentFanSpeeds[i]);
  }

  // Add sync checkbox
  tft.fillRect(20, 20, 15, 15, TFT_WHITE);
  tft.drawString("Sync All Fans", 45, 20);
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
          float fanSpeed = min(100.0, max(0.0, round(sliders[i]->getSliderPosition() / 10.0) * 10.0));
          currentFanSpeeds[i] = fanSpeed;
          sliders[i]->setSliderPosition(fanSpeed); // Update slider position to snapped value
          saveFanSpeeds(currentFanSpeeds); // Save fan speeds after changes
          
          // If sync is enabled, update all fans
          if (fanSyncEnabled) {
            for (int j = 0; j < 3; j++) {
              currentFanSpeeds[j] = fanSpeed;
              sliders[j]->setSliderPosition(fanSpeed);
              Fan_PWM[j]->setPWM(FAN1_PIN + j, fanFrequency, min(1.0, fanSpeed / 100.0));
              
              // Update percentage displays for all fans
              int yOffset = j * 90;
              tft.fillRect(150, 40 + yOffset, 60, 20, TFT_BLACK);
              tft.setTextColor(TFT_GREEN);
              tft.drawString(String(int(currentFanSpeeds[j])) + "%", 150, 40 + yOffset);
              tft.setTextColor(TFT_WHITE);
            }
          } else {
            // Original single fan update code
            Fan_PWM[i]->setPWM(FAN1_PIN + i, fanFrequency, min(1.0, fanSpeed / 100.0));
            int yOffset = i * 90;
            tft.fillRect(150, 40 + yOffset, 60, 20, TFT_BLACK);
            tft.setTextColor(TFT_GREEN);
            tft.drawString(String(int(currentFanSpeeds[i])) + "%", 150, 40 + yOffset);
            tft.setTextColor(TFT_WHITE);
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
      displayScreen4();  // Immediately refresh screen after deletion
      return;
    }
  } else {
    displayFileContents(fileName);  // Show file contents if deletion is canceled
  }
}

void displayFileContents(String fileName) {
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
    uint16_t t_x = 0, t_y = 0;
    bool pressed = tft.getTouch(&t_x, &t_y);

    backButton.press(pressed && backButton.contains(t_x, t_y));

    if (backButton.justReleased()) {
      displayScreen4();
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
  tft.setCursor(10, 30);
  tft.print("System");
  tft.setFreeFont(LABEL2_FONT);
  
  // Display CPU frequency
  tft.setCursor(10, 50);
  tft.print("CPU: ");
  tft.setTextColor(TFT_GREEN);
  tft.print(F_CPU / 1000000);
  tft.print(" MHz");
  
  // Display free heap memory
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(10, 90);
  tft.print("Free RAM: ");
  tft.setTextColor(TFT_GREEN);
  tft.print(rp2040.getFreeHeap() / 1024);
  tft.print(" KB");
  
  // Display LittleFS information
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(10, 130);
  tft.print("Files: ");
  
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
  tft.setCursor(10, 190);
  tft.print("I2C0 Devices:");
  tft.setCursor(10, 260);
  tft.print("I2C1 Devices:");
  
  // Scan both I2C buses
  bool foundDevice = false;
  bool foundDevice1 = false;
  int yPos = 210;
  delay(10);  // Small delay before I2C scan
  
  // Scan I2C0
  for (byte address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    byte error = Wire.endTransmission();
    
    if (error == 0) {
      if (!foundDevice) foundDevice = true;
      tft.setTextColor(TFT_GREEN);
      tft.setCursor(20, yPos);
      tft.print("0x");
      if (address < 16) tft.print("0");
      tft.print(address, HEX);
      yPos += 20;
      if (yPos > 300) break;
    }
  }
  
  // Scan I2C1
  yPos = 280;
  for (byte address = 1; address < 127; address++) {
    Wire1.beginTransmission(address);
    byte error = Wire1.endTransmission();
    
    if (error == 0) {
      if (!foundDevice1) foundDevice1 = true;
      tft.setTextColor(TFT_GREEN);
      tft.setCursor(20, yPos);
      tft.print("0x");
      if (address < 16) tft.print("0");
      tft.print(address, HEX);
      yPos += 20;
      if (yPos > 300) break;
    }
  }

  if (!foundDevice) {
    tft.setTextColor(TFT_RED);
    tft.setCursor(20, 210);
    tft.print("No devices found");
  }
  
  if (!foundDevice1) {
    tft.setTextColor(TFT_RED);
    tft.setCursor(20, 280);
    tft.print("No devices found");
  }
}
