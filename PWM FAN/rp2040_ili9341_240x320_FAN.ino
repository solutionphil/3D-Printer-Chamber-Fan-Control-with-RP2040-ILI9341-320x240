/*
 * Program Name: RP2040 TFT Touch UI with PWM Brightness Control
 * Version: 1.0
 * Author: Solutionphil
 * Date: 01/21/2025
 *
 * Description:
 * This program is designed for the RP2040 microcontroller to interface with an ILI9341 TFT display.
 * It provides a touch-based UI with multiple screens, PWM-based brightness control, and a file explorer
 * using LittleFS. The program includes features like touch calibration, slider-based brightness adjustment,
 * and file management.
 *
 * Hardware:
 * - RP2040 microcontroller
 * - ILI9341 TFT display with touch screen
 * - PWM pin for brightness control
 *
 * Libraries:
 * - TFT_eSPI: For TFT display control
 * - RP2040_PWM: For PWM-based brightness control
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
#include <vector>

#define BRIGHTNESS_UPDATE_DELAY 50  // Delay between brightness updates in ms
unsigned long lastBrightnessUpdate = 0;  // Track last brightness update time

// Debounce control
unsigned long lastButtonPress = 0;
const unsigned long DEBOUNCE_DELAY = 250; // 250ms debounce time

#define LED_STATE_FILE "/led_state.txt"
// Global variables and definitions
bool neopixelState = false;  // Track NeoPixel state
const int totalScreens = 7;  // Increase total screens for info screen
char settingsLabels[4][20] = {"Light", "Files", "LED", "System"};  // Add LED control option and system info

// LED control button labels
char ledOnLabel[] = "Turn ON";
char ledOffLabel[] = "Turn OFF";

// Navigation and dialog button labels
char backButtonLabel[] = "Back";
char yesLabel[] = "Yes";
char noLabel[] = "No";
char backLabel[] = "Back";

// Fan PWM pins
#define FAN1_PIN 27
#define FAN2_PIN 28
#define FAN3_PIN 29
bool syncFans = false;
float fan1Duty = 50, fan2Duty = 50, fan3Duty = 50;
RP2040_PWM* Fan1_PWM, *Fan2_PWM, *Fan3_PWM;

// Initialize TFT and slider objects
TFT_eSPI tft = TFT_eSPI(); // Invoke custom library
TFT_eSprite knob = TFT_eSprite(&tft); // Create TFT sprite for slider knob
// Fan control sprites
TFT_eSprite fanKnob1 = TFT_eSprite(&tft);
TFT_eSprite fanKnob2 = TFT_eSprite(&tft);
TFT_eSprite fanKnob3 = TFT_eSprite(&tft);
TFT_eSprite menuSprite = TFT_eSprite(&tft); // Create TFT sprite for menu

SliderWidget slider = SliderWidget(&tft, &knob);

#define _PWM_LOGLEVEL_        1
#define CALIBRATION_FILE "/TouchCalData1"
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
float frequency = 1831;  // Increased to 1831Hz to reduce flickering
float frequency2 = 20000;  // Increased to 20kHz 
float dutyCycle; //= 90;  // Default brightness value

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
    case 7: // System Info
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
  menuSprite.setColorDepth(8);
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
  
  // Initialize Fan PWM instances
  Fan1_PWM = new RP2040_PWM(FAN1_PIN, frequency2, fan1Duty);
  Fan2_PWM = new RP2040_PWM(FAN2_PIN, frequency2, fan2Duty);
  Fan3_PWM = new RP2040_PWM(FAN3_PIN, frequency2, fan3Duty);

  // Initialize the knob sprite early
  knob.setColorDepth(8);
  knob.createSprite(30, 40);  // Size for the slider knob
  
  // Initialize fan control knob sprites
  fanKnob1.createSprite(20, 20);
  fanKnob2.createSprite(20, 20);
  fanKnob3.createSprite(20, 20);

  delay(1000);
  PWM_Instance->setPWM(pinToUse, frequency, dutyCycle);

  // Initialize the TFT display and set its rotation
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

  if (currentScreen == 0) {  // Main menu screen
    for (uint8_t b = 0; b < 3; b++) {
      mainMenuButtons[b].press(pressed && mainMenuButtons[b].contains(t_x, t_y));  // Update button state
    }

    for (uint8_t b = 0; b < 3; b++) {
      if (mainMenuButtons[b].justReleased()) mainMenuButtons[b].drawButton();  // Draw normal state
      if (mainMenuButtons[b].justPressed()) {
        unsigned long currentTime = millis();
        if (currentTime - lastButtonPress >= DEBOUNCE_DELAY) {
          lastButtonPress = currentTime + 250; // Add extra delay for button presses
          switch(b) {
            case 0: currentScreen = 1; break;  // Screen 1
            case 1: currentScreen = 3; break;  // Screen 3
            case 2: currentScreen = 5; break;  // Settings
            default: break;
          }
          displayScreen(currentScreen);  // Display the selected screen
        }
      }
    }
  } else if (currentScreen == 5) {
    for (uint8_t b = 0; b < 4; b++) {
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
          else if (b == 3) currentScreen = 7;  // System Info
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
  } else if (currentScreen == 1) {
    if (pressed) {
      // Clear previous percentage displays
      tft.fillRect(30, 270, 40, 20, TFT_BLACK);
      tft.fillRect(110, 270, 40, 20, TFT_BLACK);
      tft.fillRect(190, 270, 40, 20, TFT_BLACK);
      
      // Clear previous slider knobs
      tft.fillRect(35, 60, 40, 200, TFT_BLACK);
      tft.fillRect(115, 60, 40, 200, TFT_BLACK);
      tft.fillRect(195, 60, 40, 200, TFT_BLACK);
      
      // Check for sync checkbox touch
      if (t_x >= 85 && t_x <= 100 && t_y >= 290 && t_y <= 305) {
        syncFans = !syncFans;
        displayScreen1();  // Redraw screen to update checkbox
        delay(200);  // Debounce
      }
      
      // Handle slider touches
      int newDuty;
      if (t_x >= 30 && t_x <= 50) {  // Fan 1 slider
        newDuty = map(t_y, 60, 260, 100, 0);
        newDuty = constrain(newDuty, 0, 100);
        fan1Duty = newDuty;
        if (syncFans) {
          fan2Duty = fan3Duty = newDuty;
        }
      }
      else if (t_x >= 110 && t_x <= 130) {  // Fan 2 slider
        newDuty = map(t_y, 60, 260, 100, 0);
        newDuty = constrain(newDuty, 0, 100);
        fan2Duty = newDuty;
        if (syncFans) {
          fan1Duty = fan3Duty = newDuty;
        }
      }
      else if (t_x >= 190 && t_x <= 210) {  // Fan 3 slider
        newDuty = map(t_y, 60, 260, 100, 0);
        newDuty = constrain(newDuty, 0, 100);
        fan3Duty = newDuty;
        if (syncFans) {
          fan1Duty = fan2Duty = newDuty;
        }
      }
      
      // Update PWM outputs
      Fan1_PWM->setPWM(FAN1_PIN, frequency, fan1Duty);
      Fan2_PWM->setPWM(FAN2_PIN, frequency, fan2Duty);
      Fan3_PWM->setPWM(FAN3_PIN, frequency, fan3Duty);
      
      displayScreen1();  // Update display
    }
  } else if (currentScreen == 2) {
    if (pressed) {
      unsigned long currentTime = millis();
      if (currentTime - lastBrightnessUpdate >= BRIGHTNESS_UPDATE_DELAY) {
        lastBrightnessUpdate = currentTime;
        
        if (slider.checkTouch(t_x, t_y)) {
          float newDutyCycle = round(slider.getSliderPosition() / 10) * 10;
          if (newDutyCycle != dutyCycle) {
            dutyCycle = newDutyCycle;
            slider.setSliderPosition(dutyCycle);
            PWM_Instance->setPWM(pinToUse, frequency, dutyCycle);
            saveBrightness(dutyCycle);
          }
        }
        displayScreen2();
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
        // Return to settings screen for screens 2 and 4
        if (currentScreen == 2 || currentScreen == 4) {
          currentScreen = 5;
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
      displayScreen1();  // Display fan control screen
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
  mainMenuButtons[0].initButton(&menuSprite, 120, 120, 220, 40, TFT_WHITE, TFT_BLUE, TFT_WHITE, (char*)"Fan Control", 1);
  mainMenuButtons[1].initButton(&menuSprite, 120, 180, 220, 40, TFT_WHITE, TFT_BLUE, TFT_WHITE, (char*)"Screen 3", 1);
  mainMenuButtons[2].initButton(&menuSprite, 120, 240, 220, 40, TFT_WHITE, TFT_DARKGREY, TFT_WHITE, (char*)"Settings", 1);

  for (uint8_t i = 0; i < 3; i++) {
    mainMenuButtons[i].drawButton();
  }
  
  // Push sprite to display
  menuSprite.pushSprite(0, 0);
}

void displayScreen1() {
  tft.fillScreen(TFT_BLACK);
  tft.setFreeFont(LABEL2_FONT);
  
  // Draw back button
  screenButton.initButton(&tft, 200, 20, 60, 30, TFT_WHITE, TFT_BLUE, TFT_WHITE, backButtonLabel, 1);
  screenButton.drawButton();

  // Title
  tft.setTextColor(TFT_WHITE);
  tft.drawString("Fan Control", 20, 30);

  // Draw slider backgrounds
  for(int i = 0; i < 3; i++) {
    int x = 40 + (i * 80);
    tft.fillRect(x, 60, 10, 200, TFT_DARKGREY);
  }

  // Draw slider knobs
  drawFanKnob(&fanKnob1, fan1Duty, 35);
  drawFanKnob(&fanKnob2, fan2Duty, 115);
  drawFanKnob(&fanKnob3, fan3Duty, 195);

  // Labels for fans
  tft.setTextColor(TFT_WHITE);
  tft.drawString("Fan 1", 30, 250);
  tft.drawString("Fan 2", 110, 250);
  tft.drawString("Fan 3", 190, 250);

  // Duty cycle displays
  tft.setTextColor(TFT_GREEN);
  tft.drawString(String(int(fan1Duty)) + "%", 35, 270);
  tft.drawString(String(int(fan2Duty)) + "%", 115, 270);
  tft.drawString(String(int(fan3Duty)) + "%", 195, 270);

  // Sync checkbox
  tft.fillRect(85, 290, 15, 15, TFT_WHITE);
  if (syncFans) {
    tft.drawLine(85, 290, 100, 305, TFT_BLUE);
    tft.drawLine(85, 305, 100, 290, TFT_BLUE);
  }
  tft.setTextColor(TFT_WHITE);
  tft.drawString("Sync Fans", 110, 290);
}

void drawFanKnob(TFT_eSprite* knob, float duty, int x) {
  int y = map(duty, 0, 100, 260, 60);
  
  knob->fillSprite(TFT_BLACK);
  knob->fillRoundRect(0, 0, 20, 20, 5, TFT_WHITE);
  knob->drawRoundRect(0, 0, 20, 20, 5, TFT_BLUE);
  
  // Add visual indicator
  knob->fillRect(9, 2, 2, 16, TFT_BLUE);
  knob->fillRect(2, 9, 16, 2, TFT_BLUE);
  
  // Push sprite to screen
  knob->pushSprite(x, y);
}

void displayScreen2() {
  static float lastDutyCycle = -1;  // Track last duty cycle
  static int16_t lastSliderPos = -1;  // Track last slider position
   
  // Ensure knob sprite is ready
  if (!knob.created()) knob.createSprite(30, 40);
  
  // Only redraw static elements if duty cycle was reset
  if (lastDutyCycle == -1) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_CYAN);
    tft.setFreeFont(LABEL2_FONT);
    tft.setTextSize(0);
    tft.drawString("Brightness", 30, 50);
    
    // Draw static slider parts
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
    param.sliderLT = 10;
    param.sliderRB = 100;
    param.startPosition = int16_t(dutyCycle);
    param.sliderDelay = 0;
    slider.drawSlider(20, 160, param);
  }
   
  // Only update percentage display if value changed
  if (lastDutyCycle != dutyCycle) {
    tft.fillRect(90, 110, 80, 30, TFT_BLACK);
    tft.setTextColor(TFT_GREEN);
    tft.drawString(String(int(dutyCycle)) + "%", 100, 120);
    lastDutyCycle = dutyCycle;
  }
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
  for (int i = 0; i < 4; i++) {  // Changed to 4 for new info button
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
