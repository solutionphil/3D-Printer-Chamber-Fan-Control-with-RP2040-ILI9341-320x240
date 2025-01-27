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
 */

#include <LittleFS.h>
#include <SPI.h>
#include <TFT_eSPI.h>      // Hardware-specific library
#include <TFT_eWidget.h>  // Widget library for sliders
#include "RP2040_PWM.h"   // PWM library
#include <Adafruit_NeoPixel.h>
#include <Adafruit_BME280.h>
#include <Adafruit_SGP40.h>
#include <Wire.h>
#include <QuickPID.h>


// PID Control Variables
float Setpoint = 25.0;
bool PIDactive = false;
#define PID_SETTINGS_FILE "/pid_settings.txt"


// Initialize TFT
TFT_eSPI tft = TFT_eSPI(); // Invoke custom library
// Create single shared sprite for all UI elements
TFT_eSprite uiSprite = TFT_eSprite(&tft);
TFT_eSprite knob = TFT_eSprite(&tft);
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

// Sensor data structure
struct SensorData {
    float temperature;
    float humidity;
    int32_t vocIndex;
    unsigned long lastUpdate;
} sensorState;

//Sensors
  // Use sensor state values directly
  float temp = sensorState.temperature;
  float hum = sensorState.humidity;
  int32_t voc_index = sensorState.vocIndex;

//Define the aggressive and conservative and POn Tuning Parameters
float aggKp = 4, aggKi = 0.2, aggKd = 1;
float consKp = 1, consKi = 0.05, consKd = 0.25;

float fanSpeed2=30;
//Specify the links
QuickPID myPID(&temp, &fanSpeed2, &Setpoint);


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

    // Only initialize BME280 and SGP40 once
  if (!bme.begin(0x76, &Wire)) {
    tft.setCursor(10, 70);
    Serial.print("BME280 not found!");
    return;
  }
  if (!sgp.begin()) {
    tft.setCursor(10, 90);
    Serial.print("SGP40 not found!");
    return;
  }

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

  //turn the PID on
  myPID.SetMode(myPID.Control::manual);
  myPID.SetControllerDirection(myPID.Action::reverse);
  myPID.SetOutputLimits(30, 100);

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

  // Initialize the SGP40 air quality sensor
  sgp.begin();

  //Calibrate the touch screen and display the initial screen
  touch_calibrate();  
  displayScreen(currentScreen);  

}
void updateSensors() {
  unsigned long currentTime = millis();
  if (currentTime - sensorState.lastUpdate >= SENSOR_UPDATE_INTERVAL) {
    sensorState.temperature = bme.readTemperature();
    sensorState.humidity = bme.readHumidity();
    sensorState.vocIndex = sgp.measureVocIndex(sensorState.temperature, sensorState.humidity);
    sensorState.lastUpdate = currentTime;

  temp = sensorState.temperature;
  hum = sensorState.humidity;
  voc_index = sensorState.vocIndex;
  }
}
void loop(void) {
  // Main loop to handle touch input and update screens
  uint16_t t_x = 0, t_y = 0;  // Variables to store touch coordinates
  bool pressed = tft.getTouch(&t_x, &t_y);  // Boolean indicating if the screen is being touched

  // Update sensors in a non-blocking way
  updateSensors();

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
    for (uint8_t b = 0; b < 4; b++) {  // Adjusted loop to 4 buttons
      mainMenuButtons[b].press(pressed && mainMenuButtons[b].contains(t_x, t_y));  // Update button state
    }

    for (uint8_t b = 0; b < 4; b++) {  // Adjusted loop to 4 buttons
      if (mainMenuButtons[b].justReleased()) mainMenuButtons[b].drawButton();  // Draw normal state
      if (mainMenuButtons[b].justPressed()) {
        unsigned long currentTime = millis();
        if (currentTime - lastButtonPress >= DEBOUNCE_DELAY) {
          lastButtonPress = currentTime + 50; // Add extra delay for button presses
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
  sprite->drawCentreString(buf, x, y-(round(16 * scale)), textSize);
  
  sprite->setTextSize(1);
  sprite->setTextColor(TFT_WHITE, bgColor);
  sprite->drawCentreString(label, x, y+5, 2);
}
