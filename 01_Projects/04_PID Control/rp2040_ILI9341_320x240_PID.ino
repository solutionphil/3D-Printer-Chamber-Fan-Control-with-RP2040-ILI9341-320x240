 /* Program Name: RP2040 TFT Touch UI with PWM Brightness Control
 * Version: 1.0
 * Author: Solutionphil
 * Date: 02/17/2025
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
 * - TFT_eWidget: For Sliders
 * - RP2040_PWM: For PWM-based brightness and fan speed control
 * - Adafruit_NeoPixel: For NeoPixel control
 * - Adafruit_BME280: For environmental sensing
 * - Adafruit_SGP40: For air quality sensing
 * - Wire: For I2C communication
 * - QuickPID for PWM Control
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


//For DIM Display
unsigned long lastActivityTime = 0;
unsigned long inactivityThresholds[] = {0, 3000, 5000, 10000, 15000, 20000, 30000, 60000, 120000, 180000, 300000, 600000, 1200000, 1800000};
unsigned long inactivityFactors[] = {0, 1, 2, 3, 4, 5, 10, 20, 50, 100};
unsigned long inactivityThreshold = 3000; //10sec
unsigned long inactivityThreshold2 =  6000; //20sec
float dimHelper1 = 3; //15sec
float dimHelper2 = 1; //1x
bool isDimmed = false;
bool isOFF = false;
float neoPixelBrightness = 50.0; // Default NeoPixel brightness value
// PID Control Variables
float Setpoint = 25.0;
float Kp = 2, Ki = 5, Kd = 1;

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
TFT_eSprite gauge4 = TFT_eSprite(&tft);
TFT_eSprite gauge5 = TFT_eSprite(&tft);
TFT_eSprite gaugebg = TFT_eSprite(&tft);
TFT_eSprite menuSprite = TFT_eSprite(&tft);
TFT_eSprite pidSprite = TFT_eSprite(&tft);
TFT_eSPI_Button pidToggleBtn;
TFT_eSPI_Button setpointUpBtn;
TFT_eSPI_Button setpointDownBtn;
TFT_eSPI_Button screenButton;  // Button to switch screens
TFT_eSPI_Button fileButtons[10]; // Buttons for file explorer
TFT_eSPI_Button backButton;
TFT_eSPI_Button yesButton;
TFT_eSPI_Button noButton;
TFT_eSPI_Button mainMenuButtons[5]; // Buttons for main menu


String mainMenuLabel[5];

String fileNames[10]; // Array to store file names
String buttonLabels[15];
bool backgroundDrawn = false;

// Define the sprite sizes and positions for P, I, and D bars
const int barWidth = 20;
const int barHeight = 50;
const int barSpacing = 20;
const int barX = 30;
const int barY = 165;

// PWM frequencies
float frequency = 1831;      // For brightness control
float fanFrequency = 20000;  // Separate frequency for fans

// Debounce control
unsigned long lastButtonPress = 0;
const unsigned long DEBOUNCE_DELAY = 100; // 100ms debounce time

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
float tempOffset= 2;
float temp = (sensorState.temperature-tempOffset);
float hum = sensorState.humidity;
int32_t voc_index = sensorState.vocIndex;

// Get VOC reading with temperature/humidity compensation
uint16_t voc_color = TFT_GREEN;

//Define the aggressive and conservative and POn Tuning Parameters
float aggKp = 4, aggKi = 0.2, aggKd = 1;
float consKp = 1, consKi = 0.05, consKd = 0.25;

float fanSpeed2;
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
SliderWidget* sliders[] = {&slider1, &slider2, &slider3};

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
float dutyCycle ;//= 90;  // Default brightness value

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

int currentScreen = 0;

// Function to cleanup sprites and free memory
void cleanupSprites() {
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
    if (gauge4.created()) {
        gauge4.deleteSprite();
        Serial.println("Cleaned gauge4");
    }
    if (gauge5.created()) {
        gauge5.deleteSprite();
        Serial.println("Cleaned gauge5");
    }
    if (gaugebg.created()) {
        gaugebg.deleteSprite();
        Serial.println("Cleaned gaugebg");
    }
    if (menuSprite.created()) {
        menuSprite.deleteSprite();
        Serial.println("Cleaned menuSprite ");
    }
    if (pidSprite.created()) {
        pidSprite.deleteSprite();
        Serial.println("Cleaned pidSprite ");
    }
    if (menuSprite.created()) {
        menuSprite.deleteSprite();
        Serial.println("Cleaned menuSprite");
    }
    if (pidSprite.created()) {
        pidSprite.deleteSprite();
        Serial.println("Cleaned pidSprite");
    }
    // Log available memory after cleanup
    Serial.printf("Free RAM after cleanup: %d bytes\n", rp2040.getFreeHeap());
}

void saveBrightness(float displayBrightness, float neoPixelBrightness, float dimHelper1, float dimHelper2) {
  File f = LittleFS.open(BRIGHTNESS_FILE, "w");
  if (f) {
    f.println(displayBrightness);
    f.println(neoPixelBrightness);
    f.println(dimHelper1);
    f.println(dimHelper2);
    f.close();
  }
}

void loadBrightness(float &displayBrightness, float &neoPixelBrightness, float &dimHelper1, float &dimHelper2) {
  if (LittleFS.exists(BRIGHTNESS_FILE)) {
    File f = LittleFS.open(BRIGHTNESS_FILE, "r");
    if (f) {
      String val = f.readStringUntil('\n');
      displayBrightness = val.toFloat();
      val = f.readStringUntil('\n');
      neoPixelBrightness = val.toFloat();
      val = f.readStringUntil('\n');
      dimHelper1 = val.toFloat();
      inactivityThreshold = inactivityThresholds[(int)dimHelper1];
      val = f.readStringUntil('\n');
      dimHelper2 = val.toFloat();
      inactivityThreshold2 = inactivityFactors[(int)dimHelper2] * inactivityThreshold;
      f.close();
      return;
    }
  }
  displayBrightness = 90.0; // Default display brightness if file doesn't exist
  neoPixelBrightness = 50.0; // Default NeoPixel brightness if file doesn't exist
  dimHelper1 = 3; // Default dimHelper1 value
  inactivityThreshold = inactivityThresholds[(int)dimHelper1];
  dimHelper2 = 1; // Default dimHelper2 value
  inactivityThreshold2 = inactivityFactors[(int)dimHelper2] * inactivityThreshold;
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
    speeds[i] = 30.0; // Default fan speed
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

void LoadnSetFanSpeeds(){
  // Load saved fan speeds
  float fanSpeeds[3] = {30, 30, 30};
  loadFanSpeeds(fanSpeeds);
  for (int i = 0; i < 3; i++) {
    currentFanSpeeds[i] = fanSpeeds[i];
    Fan_PWM[i]->setPWM(FAN1_PIN + i, fanFrequency, fanSpeeds[i]);
  }
}
void displayTime(unsigned long timeInMs, int x, int y) {
  unsigned long seconds = timeInMs / 1000;
  unsigned long minutes = seconds / 60;
  unsigned long hours = minutes / 60;
  unsigned long remainingMinutes = minutes % 60;
  unsigned long remainingSeconds = seconds % 60;

  if (hours > 0) {
    if (remainingMinutes > 0 || remainingSeconds > 0) {
      tft.drawString(String(hours) + " h " + String(remainingMinutes) + " min " , x, y);
    }   
    else {
      tft.drawString(String(hours) + " h", x, y);
    }
   }else if (minutes > 0) {
    if (remainingSeconds > 0) {
      tft.drawString(String(minutes) + " min " + String(remainingSeconds) + " sec", x, y);
    } else {
      tft.drawString(String(minutes) + " min", x, y);
    }
  } else {
    tft.drawString(String(seconds) + " sec", x, y);
  }
}

void drawCurvedText(String text, int x_center, int y_center, int radius, float angle, float start_angle) {
  float radianAngle = angle * DEG_TO_RAD; // Convert angle to radians
  int length = text.length();
  float charAngle = radianAngle / length; // Angle for each character
  float theta = start_angle * DEG_TO_RAD; // Convert start angle to radians

  for (int i = 0; i < length; i++) {
    float x = x_center + radius * cos(theta);
    float y = y_center + radius * sin(theta);

    // Set cursor at calculated position
    tft.setCursor(x, y);

    // Draw character
    tft.drawChar(text.charAt(i), x, y, 2); 

    // Move to the next angle
    theta += charAngle; 
  
    }
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

    // Initialize sprites with 8-bit color depth
    menuSprite.setColorDepth(8);
    knob.setColorDepth(8);
    gauge1.setColorDepth(8);
    gauge2.setColorDepth(8);
    gauge3.setColorDepth(8);
    gauge4.setColorDepth(8);
    gauge5.setColorDepth(8);
    gaugebg.setColorDepth(8);
    pidSprite.setColorDepth(8);

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
  loadBrightness(dutyCycle, neoPixelBrightness, dimHelper1, dimHelper2);
  
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
  myPID.SetSampleTimeUs(1900000);

  LoadnSetFanSpeeds();
  fanSpeed2=currentFanSpeeds[1];

  // Initialize the TFT display and set its rotation (Main Menu updated)
  tft.init();  
  tft.setRotation(0);  

  // Initialize the SGP40 air quality sensor
  sgp.begin();

  //Calibrate the touch screen and display the initial screen
  touch_calibrate();  
  displayScreen(currentScreen);  
}

void checkDim() {
  // Check if the screen is being touched
  uint16_t t_x = 0, t_y = 0;
  bool pressed = tft.getTouch(&t_x, &t_y);

  // If the screen is touched, reset the inactivity timer and restore brightness if dimmed
  if (pressed) {
    if (isDimmed || isOFF) {
      t_x = 0;
      t_y = 0;
      delay(20);
      loadBrightness(dutyCycle, neoPixelBrightness, dimHelper1, dimHelper2);
      Serial.println("RECOVER BRIGHTNESS");
      PWM_Instance->setPWM(pinToUse, frequency, dutyCycle);
      isDimmed = false;
      isOFF = false;
    }
    lastActivityTime = millis();
  }

  // Check if the inactivity threshold has been reached and dim the display
  if (millis() - lastActivityTime > inactivityThreshold && !isDimmed && dutyCycle >= 30 && !inactivityThreshold==0) {
    dutyCycle = 30;
    PWM_Instance->setPWM(pinToUse, frequency, dutyCycle);
    isDimmed = true;
    Serial.println("DIMMED BRIGHTNESS");
  } 
  // Check if the extended inactivity threshold has been reached and turn off the display
  else if (millis() - lastActivityTime > inactivityThreshold2 && isDimmed && !inactivityThreshold==0) {
    dutyCycle = 0;
    PWM_Instance->setPWM(pinToUse, frequency, dutyCycle);
    isOFF = true;
  }
}


void updateSensors() {
  unsigned long currentTime = millis();
  if (currentTime - sensorState.lastUpdate >= SENSOR_UPDATE_INTERVAL) {
    sensorState.temperature = bme.readTemperature();
    sensorState.humidity = bme.readHumidity();
    sensorState.vocIndex = sgp.measureVocIndex(sensorState.temperature, sensorState.humidity);
    sensorState.lastUpdate = currentTime;

  temp = sensorState.temperature-tempOffset;
  hum = sensorState.humidity;
  voc_index = sensorState.vocIndex;
  if(PIDactive){
    myPID.Compute();
            //Serial.println(fanSpeed2);
        Serial.println(myPID.GetPterm());
        Serial.println(myPID.GetIterm());
        Serial.println(myPID.GetDterm());
  }
  }
}
void updatePIDDisplay(bool forceRedraw);

void loop(void) {
  // Main loop to handle touch input and update screens
  uint16_t t_x = 0, t_y = 0;  // Variables to store touch coordinates
  bool pressed = tft.getTouch(&t_x, &t_y);  // Boolean indicating if the screen is being touched

  checkDim();// Call the checkdim function to handle display dimming
   
  updateSensors(); // Update sensors in a non-blocking way
  // Handle temperature and air quality screen updates
  if (currentScreen == 3) {
    unsigned long currentMillis = millis();
    if (currentMillis - lastSensorUpdate >= SENSOR_UPDATE_INTERVAL)
      updateTempAndAirQualityDisplay();
  }
          
    // Handle PID screen updates
    if (currentScreen == 1) {
        unsigned long currentMillis = millis();
        if (currentMillis - lastSensorUpdate >= SENSOR_UPDATE_INTERVAL)
            updatePIDDisplay(false);  // Update the PID display

        // Handle PID control button presses
        pidToggleBtn.press(pressed && pidToggleBtn.contains(t_x, t_y));
        setpointUpBtn.press(pressed && setpointUpBtn.contains(t_x, t_y));
        setpointDownBtn.press(pressed && setpointDownBtn.contains(t_x, t_y));

        if (pidToggleBtn.justPressed()) {
            PIDactive = !PIDactive;
            if (PIDactive)
            {
            myPID.SetMode(myPID.Control::automatic);
            Serial.println("PID ON");
            }
            else
            {
            myPID.SetMode(myPID.Control::manual);
            LoadnSetFanSpeeds();
            fanSpeed2=currentFanSpeeds[1];
            Serial.println("PID OFF");
            }
            savePIDSettings();
            updatePIDDisplay(true);
            Serial.println("Touch");
        }

        if (setpointUpBtn.justPressed()) {
            Setpoint += 0.5;
            savePIDSettings();
            updatePIDDisplay(true);
            Serial.println("Touch");
        }

        if (setpointDownBtn.justPressed()) {
            Setpoint -= 0.5;
            savePIDSettings();
            updatePIDDisplay(true);
            Serial.println("Touch");
        }
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
            
            t_x = 0;
            t_y = 0;
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

    // Handle NeoPixel brightness slider touch
    if (pressed) {
      if (slider2.checkTouch(t_x, t_y)) {
        neoPixelBrightness = round(slider2.getSliderPosition() / 10.0) * 10.0;
        slider2.setSliderPosition(neoPixelBrightness);
        pixels.setBrightness(neoPixelBrightness);
        pixels.show();
        saveBrightness(dutyCycle, neoPixelBrightness, dimHelper1, dimHelper2);
          tft.fillRect(95, 85, 70, 30, TFT_BLACK);
        tft.setTextColor(TFT_GREEN);
        tft.drawString(String(int(neoPixelBrightness)) + "%", 100, 90);
      }
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

    // Check for sync checkbox touch
    if (pressed && t_x >= 20 && t_x <= 35 && t_y >= 20 && t_y <= 35) {
      fanSyncEnabled = !fanSyncEnabled;
      // Redraw checkbox
      tft.fillRect(20, 20, 15, 15, TFT_WHITE);
      if (fanSyncEnabled) {
        tft.drawLine(20, 20, 35, 35, TFT_BLACK);
        tft.drawLine(35, 20, 20, 35, TFT_BLACK);
      }
      delay(100); // Debounce delay
    }

    SliderWidget* sliders[] = {&slider1, &slider2, &slider3};
    
    if (pressed) {
      for (int i = 0; i < 3; i++) {
        if (sliders[i]->checkTouch(t_x, t_y)) {
          delay(10); // Small delay for stable touch reading
          float fanSpeed = min(100.0, max(0.0, round(sliders[i]->getSliderPosition() / 10.0) * 10.0));
          delay(10); // Small delay for stable touch reading
          currentFanSpeeds[i] = fanSpeed;
          sliders[i]->setSliderPosition(fanSpeed); // Update slider position to snapped value
          Serial.printf("Updating Fan %d to %.1f%%\n", i + 1, fanSpeed);

          // If sync is enabled, update all fans
          if (fanSyncEnabled) {
            for (int j = 0; j < 3; j++) {
              currentFanSpeeds[j] = fanSpeed;
              sliders[j]->setSliderPosition(fanSpeed);
              delay(10); // Small delay for stable touch reading
              Fan_PWM[j]->setPWM(FAN1_PIN + j, fanFrequency, fanSpeed);

              // Update percentage displays for all fans
              int yOffset = j * 90;
              tft.fillRect(150, 60 + yOffset, 60, 20, TFT_BLACK);  // Clear previous percentage text area
              tft.setTextColor(TFT_DARKCYAN);
              tft.drawString(String(int(currentFanSpeeds[j])) + "%", 150, 60 + yOffset);
              tft.setTextColor(TFT_WHITE);
              saveFanSpeeds(currentFanSpeeds);
              fanSpeed2 = currentFanSpeeds[1];
            }
          } else {
            // Original single fan update code
            Fan_PWM[i]->setPWM(FAN1_PIN + i, fanFrequency, fanSpeed);
            int yOffset = i * 90;
            tft.fillRect(150, 60 + yOffset, 60, 20, TFT_BLACK);  // Clear previous percentage text area
            tft.setTextColor(TFT_DARKCYAN);
            tft.drawString(String(int(currentFanSpeeds[i])) + "%", 150, 60 + yOffset);
            tft.setTextColor(TFT_WHITE);
            saveFanSpeeds(currentFanSpeeds);
            fanSpeed2 = currentFanSpeeds[1];
          }
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
        saveBrightness(dutyCycle, neoPixelBrightness, dimHelper1, dimHelper2);  // Save the new brightness value
        
        // Update percentage display
        tft.fillRect(110, 65, 80, 25, TFT_BLACK);
        tft.setTextColor(TFT_DARKCYAN);
        tft.drawString(String(int(dutyCycle)) + "%", 120, 75);
      }
       // Check touch for slider2
  if (slider2.checkTouch(t_x, t_y)) {     
    dimHelper1 = slider2.getSliderPosition();
    slider2.setSliderPosition(dimHelper1);
    inactivityThreshold = inactivityThresholds[(int)dimHelper1];
    inactivityThreshold2 = inactivityThreshold * inactivityThresholds[(int)dimHelper2];

    Serial.print("Inactivity Threshold: ");
    Serial.println(inactivityThreshold);

    // Update percentage display
    tft.fillRect(90, 175, 80, 30, TFT_BLACK);
    tft.fillRect(45, 245, 195, 30, TFT_BLACK);
    tft.setTextColor(TFT_DARKCYAN);

    displayTime(inactivityThreshold, 100, 180);
    displayTime(inactivityFactors[(int)dimHelper2] * inactivityThreshold, 110, 250);
    tft.drawString(String(inactivityFactors[(int)dimHelper2]) + "x", 50, 250);
    saveBrightness(dutyCycle, neoPixelBrightness, dimHelper1, dimHelper2);  // Save the new brightness value
  }

  // Check touch for slider3
  if (slider3.checkTouch(t_x, t_y)) {
    dimHelper2 = slider3.getSliderPosition();
    slider3.setSliderPosition(dimHelper2);
    inactivityThreshold2 = inactivityFactors[(int)dimHelper2] * inactivityThreshold;

    Serial.print("Inactivity Factor: ");
    Serial.println(inactivityThreshold2);
    Serial.println(inactivityFactors[(int)dimHelper2]);

    // Update percentage display
    tft.fillRect(45, 245, 195, 30, TFT_BLACK);
    tft.setTextColor(TFT_DARKCYAN);

    displayTime(inactivityFactors[(int)dimHelper2] * inactivityThreshold, 110, 250);
    tft.drawString(String(inactivityFactors[(int)dimHelper2]) + "x", 50, 250);
    saveBrightness(dutyCycle, neoPixelBrightness, dimHelper1, dimHelper2);  // Save the new brightness value
  }
    }}

    
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

    while (tft.getTouch(&t_x, &t_y)) {}

    for (uint8_t i = 0; i < 10; i++) {
      fileButtons[i].press(pressed && fileButtons[i].contains(t_x, t_y));

      // Handle file button interactions
      if (fileButtons[i].justReleased()) fileButtons[i].drawButton(false,fileNames[i]);
      if (fileButtons[i].justPressed()) handleFileButtonPress(i);
    }
  }
}

void drawGaugeToSprite(TFT_eSprite* sprite, int x, int y, float min_val, float max_val, float value, const char* label, uint16_t color, uint16_t bgColor) {
  sprite->fillSprite(TFT_BLACK);
  
  // Determine scaling factor based on sprite dimensions
  float scale = (sprite->width() == 98) ? 0.7 : ((sprite->width() == 112) ? 0.8 : 1.0); // 70% for 98x98,80% für 112 and 100% for 140x140
  int outerRadius = round(62 * scale);
  int innerRadius = round(58 * scale);
  bool isVOC = (sprite->width() == 98);
  bool isPID = (sprite->width() == 112);
  int fillRadius = round(54 * scale);
  int centerRadius = round(35 * scale);
  
  // Adjust fill radius for VOC gauge to ensure visibility
  if (isVOC) {
    fillRadius = round(58 * scale);  // Increased fill radius for VOC
    innerRadius = round(52 * scale); // Adjusted inner radius
    // Adjust fill radius for VOC gauge to ensure visibility
  }else if (isPID) {
    fillRadius = round(61 * scale);  // Increased fill radius for PID
    innerRadius = round(56 * scale); // Adjusted inner radius
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
  int fillStart = isVOC ? round(58 * scale) : isPID ? round(55 * scale) : round(54 * scale);

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
  if (isVOC) {
    sprintf(buf, "%d", (int)value); // Integer format for VOC
  } 
  else if (isPID && max_val >= 80) {
    sprintf(buf, "%d", (int)value);
  }
  else {
    sprintf(buf, "%.1f", value); // Keep decimal for temp and humidity
  }
  
  sprite->setTextColor(TFT_WHITE, bgColor);
  // Adjust text size and position for VOC gauge
  int textSize = isVOC ? 2 : 4;
if (isPID) {
  sprite->drawCentreString(buf, x, y - 15, textSize);
  }
  else {
  sprite->drawCentreString(buf, x, y - round(16 * scale), textSize);
  }
    
  sprite->setTextSize(1);
  sprite->setTextColor(TFT_WHITE, bgColor);
  if (isPID && max_val >= 80) {
  sprite->drawCentreString(label, x, y + 6, 2);
  }
  else {
  sprite->drawCentreString(label, x, y + 5, 2);
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

bool initialDraw = true; // Declare initialDraw outside the function

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
      initialDraw = true; // Reset initialDraw when switching to PID screen
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

    mainMenuLabel[0] = "Fan Control";
    mainMenuLabel[1] = "AQI and Temp";
    mainMenuLabel[2] = "PID Control";
    mainMenuLabel[3] = "Settings";

    // Initialize buttons with menuSprite instead of tft
    mainMenuButtons[0].initButton(&menuSprite, 120, 100, 220, 40, TFT_WHITE, TFT_BLUE, TFT_WHITE, (char*)"", 1); // Replaced Screen 1 with Fans
    mainMenuButtons[1].initButton(&menuSprite, 120, 160, 220, 40, TFT_WHITE, TFT_BLUE, TFT_WHITE, (char*)"", 1); // Temperature
    mainMenuButtons[2].initButton(&menuSprite, 120, 220, 220, 40, TFT_WHITE, TFT_BLUE, TFT_WHITE, (char*)"", 1);
    mainMenuButtons[3].initButton(&menuSprite, 120, 280, 220, 40, TFT_WHITE, TFT_DARKGREY, TFT_WHITE, (char*)"", 1);

    for (uint8_t i = 0; i < 4; i++) {  // Adjusted loop to 3 buttons
        mainMenuButtons[i].drawButton(true, mainMenuLabel[i]);
    }
    
    // Push sprite to display
    menuSprite.pushSprite(0, 0);
}

void displayPID() {

    tft.fillScreen(TFT_BLACK);

    // Draw PID Control screen title
  tft.setTextColor(TFT_CYAN);
  tft.setFreeFont(LABEL2_FONT);
  tft.setTextSize(1);
  tft.drawString("PID Control", 15, 10);

    // Initialize sprites for gauges
    if (!gauge4.created()) {
        gauge4.createSprite(112, 112); // 80% of 120x120
        gauge4.setColorDepth(8);
    }
    if (!gauge5.created()) {
        gauge5.createSprite(112, 112); // 80% of 120x120
        gauge5.setColorDepth(8);
    }

    // Draw PID active toggle button
    pidToggleBtn.initButton(&tft, 60, 300, 100, 40, TFT_WHITE, PIDactive ? TFT_GREEN : TFT_RED, TFT_WHITE, PIDactive ? (char *)"ON" : (char *)"OFF", 1);
    pidToggleBtn.drawButton();

    // Draw setpoint up button
    tft.setFreeFont(&FreeSansBold18pt7b);
    setpointUpBtn.initButton(&tft, 200, 250, 60, 40, TFT_WHITE, TFT_BLUE, TFT_WHITE, (char *)"+", 1);
    setpointUpBtn.drawButton();

    // Draw setpoint down button
    setpointDownBtn.initButton(&tft, 200, 300, 60, 40, TFT_WHITE, TFT_BLUE, TFT_WHITE, (char *)"-", 1);
    setpointDownBtn.drawButton();
    tft.setFreeFont(LABEL2_FONT);

    // Draw back button
    screenButton.initButton(&tft, 200, 20, 60, 30, TFT_WHITE, TFT_BLUE, TFT_WHITE, backButtonLabel, 1);
    screenButton.drawButton();

    // Initial draw of PID values
    updatePIDDisplay(true);

    initialDraw = false; // Set initialDraw to false after the first draw
}
void drawGradientBarWithIndicator(int x, int y, int width, int height, int voc_index, int &lastIndicatorX) {
    // Draw the gradient bar
    for (int i = 0; i < width; i++) {
        uint16_t color = tft.color565(255 * i / width, 255 * (width - i) / width, 0);
        tft.drawFastVLine(x + i, y, height, color);
    }

    // Draw the scale markers
    int scaleMarkers[] = {0, 100, 200, 300, 400, 500};
    for (int i = 0; i < 6; i++) {
        int markerX = x + (scaleMarkers[i] * width / 500);
        tft.drawFastVLine(markerX, y + height, 5, TFT_WHITE);
        tft.setTextColor(TFT_WHITE);
        tft.setFreeFont(&TomThumb);
        tft.setCursor(markerX - 4, y + height + 12);
        tft.print(scaleMarkers[i]);
        tft.setFreeFont(LABEL2_FONT);
    }

    // Overdraw the previous triangle with black
    if (lastIndicatorX != -1) {
        tft.fillTriangle(lastIndicatorX - 5, y - 15, lastIndicatorX + 5, y - 15, lastIndicatorX, y - 5, TFT_BLACK);
    }

    // Draw the indicator for voc_index
    int indicatorX = x + (voc_index * width / 500);
    tft.fillTriangle(indicatorX - 5, y - 15, indicatorX + 5, y - 15, indicatorX, y - 5, TFT_WHITE);
    tft.fillTriangle(indicatorX - 4, y - 14, indicatorX + 4, y - 14, indicatorX, y - 6, TFT_RED);

    // Update the last indicator position
    lastIndicatorX = indicatorX;
}

void updatePIDDisplay(bool forceRedraw) {
    static float lastSetpoint = -1;
    static float lastTemp = -1;
    static float lastFanSpeed = -1;
    static bool lastPIDactive = !PIDactive;
    static int lastVocIndex = -1;
    static int lastIndicatorX = -1;

    // Update temperature gauge
    if (lastTemp != temp || forceRedraw) {
        if (!gauge4.created()) {
            gauge4.createSprite(112, 112); // 80% of 120x120
            gauge4.setColorDepth(8);
        }
        gauge4.fillSprite(TFT_BLACK);
        drawGaugeToSprite(&gauge4, 56, 56, 0, 60, temp, "Temp C", TFT_RED, 0x8800); // 80% of 60x60
        gauge4.pushSprite(5, 45);
        lastTemp = temp;

            float DeltaT = Setpoint - temp;
    if (DeltaT < 0) {
        tft.setTextColor(TFT_RED, TFT_BLACK);
    } else {
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
    }
    tft.setTextSize(1);
    tft.setFreeFont(&TomThumb); // Use the TomThumb font
    tft.setCursor(40, 161);
    tft.print("Delta: ");
    tft.drawFloat(DeltaT, 1, 67, 156); // Wert mit einer Nachkommastelle zeichnen
    tft.setCursor(77, 161);
    tft.print(" C");

    tft.setFreeFont(LABEL2_FONT); // Use the Label2 font
    tft.setTextSize(1);
    }

    // Update duty cycle gauge
    if (lastFanSpeed != fanSpeed2 || forceRedraw) {
        if (!gauge5.created()) {
            gauge5.createSprite(112, 112); // 80% of 120x120
            gauge5.setColorDepth(8);
        }
        gauge5.fillSprite(TFT_BLACK);
        drawGaugeToSprite(&gauge5, 56, 56, 0, 100, fanSpeed2, "Duty %", TFT_BLUE, 0x0011); // 80% of 60x60
        gauge5.pushSprite(123, 45);
        lastFanSpeed = fanSpeed2;
    }

    // Draw gradient bar with scale and indicator for air quality
    if (lastVocIndex != voc_index || forceRedraw) {
        drawGradientBarWithIndicator(30, 190, 180, 12, voc_index, lastIndicatorX);
        tft.drawRect(29, 189, 182, 14, TFT_WHITE);
        lastVocIndex = voc_index;
    }

    // Create a smaller sprite for the setpoint display
    TFT_eSprite setpointSprite = TFT_eSprite(&tft);
    setpointSprite.setColorDepth(8);
    setpointSprite.createSprite(150, 50);
    setpointSprite.fillSprite(TFT_BLACK);
    setpointSprite.fillRoundRect(0, 0, 150, 50, 10, TFT_SILVER);
    setpointSprite.fillRoundRect(2, 4, 146, 40, 5, TFT_BLACK);

    // Draw border for the rounded rectangle
    setpointSprite.drawRoundRect(0, 0, 150, 50, 10, TFT_DARKGREY);
    setpointSprite.drawRoundRect(2, 3, 146, 42, 8, TFT_DARKGREY);

    setpointSprite.setTextColor(TFT_WHITE);
    setpointSprite.setFreeFont(LABEL2_FONT);
    setpointSprite.setTextSize(1);
    setpointSprite.setCursor(10, 29);
    setpointSprite.printf("Target: ");
    setpointSprite.setTextColor(TFT_GREEN);
    setpointSprite.printf("%.2f C", Setpoint);

    // Display current setpoint if it has changed or if it's the initial draw
    if (lastSetpoint != Setpoint || forceRedraw) {
        setpointSprite.setTextColor(TFT_WHITE);
        setpointSprite.setFreeFont(LABEL2_FONT);
        setpointSprite.setTextSize(1);
        setpointSprite.setCursor(10, 29);
        setpointSprite.printf("Target: ");
        setpointSprite.setTextColor(TFT_GREEN);
        setpointSprite.printf("%.2f C", Setpoint);
        lastSetpoint = Setpoint;
    }

    // Push the sprite to the screen
    setpointSprite.pushSprite(10, 225);
    setpointSprite.deleteSprite(); // Delete the sprite to free up memory

    // Update PID toggle button if state has changed
    if (lastPIDactive != PIDactive) {
        pidToggleBtn.initButton(&tft, 60, 300, 100, 40, TFT_WHITE, PIDactive ? TFT_GREEN : TFT_RED, TFT_WHITE, PIDactive ? (char *)"ON" : (char *)"OFF", 1);
        pidToggleBtn.drawButton();
        lastPIDactive = PIDactive;
    }

    // PID control logic
    if (PIDactive) {
        float gap = abs(Setpoint - temp); // distance away from setpoint
        if (gap < 2) { // we're close to setpoint, use conservative tuning parameters
            myPID.SetTunings(consKp, consKi, consKd);
        } else {
            // we're far from setpoint, use aggressive tuning parameters
            myPID.SetTunings(aggKp, aggKi, aggKd);
        }
        Fan_PWM[0]->setPWM(FAN1_PIN, fanFrequency, fanSpeed2);
        Fan_PWM[1]->setPWM(FAN2_PIN, fanFrequency, fanSpeed2);
        Fan_PWM[2]->setPWM(FAN3_PIN, fanFrequency, fanSpeed2);
    }
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
  tft.drawString("Backlight Control", 10, 15);
  tft.drawString("Brightness: ", 10, 75);
  tft.drawString("Delay for DIM and OFF", 10, 150);
  tft.drawFastHLine(5, 168, 230, TFT_BLUE);
  tft.drawFastHLine(5, 169, 230, TFT_DARKCYAN);
  tft.drawFastHLine(5, 170, 230, TFT_BLUE);
  // Update percentage display
  tft.fillRect(110, 65, 80, 25, TFT_BLACK);
  tft.setTextColor(TFT_DARKCYAN);
  tft.drawString(String(int(dutyCycle)) + "%", 120, 75);
  
  //slider2&3
  // Update percentage display
  displayTime(inactivityThreshold, 100, 180);
  displayTime(inactivityFactors[(int)dimHelper2] * inactivityThreshold, 110, 250);
  tft.drawString(String(inactivityFactors[(int)dimHelper2]) + "x", 50, 250);

  // Create slider parameters
  slider_t param;
  
  // Slider slot parameters
  param.slotWidth = 10;
  param.slotLength = 200;
  param.slotColor = TFT_BLUE;
  param.slotBgColor = TFT_BLACK;
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
  param.startPosition = dutyCycle;
  param.sliderDelay = 0;

    // Draw the slider
  slider1.drawSlider(20, 100, param);
  slider1.setSliderPosition(66);
  slider1.setSliderPosition(dutyCycle);
  int16_t x, y;    // x and y can be negative
  uint16_t w, h;   // Width and height
  slider1.getBoundingRect(&x, &y, &w, &h);     // Update x,y,w,h with bounding box
  tft.drawRect(x, y, w, h, TFT_DARKGREY); // Draw rectangle outline

////SLIDER 2&3
  // Slider range and movement
  param.sliderLT = 0;
  param.sliderRB = 13;
  param.startPosition = 5;
  param.sliderDelay = 0;

    // Draw the slider for seconds
  slider2.drawSlider(20, 205, param);
  slider2.setSliderPosition(50);
  slider2.setSliderPosition(dimHelper1);
  slider2.getBoundingRect(&x, &y, &w, &h);     // Update x,y,w,h with bounding box
  tft.drawRect(x, y, w, h, TFT_DARKGREY); // Draw rectangle outline

    // Slider range and movement
  param.sliderLT = 0;
  param.sliderRB = 9;
  param.startPosition = 5;
  param.sliderDelay = 0;

    // Draw the slider for factor
  slider3.drawSlider(20, 275, param);
  slider3.setSliderPosition(66);
  slider3.setSliderPosition(dimHelper2);
  slider3.getBoundingRect(&x, &y, &w, &h);     // Update x,y,w,h with bounding box
  tft.drawRect(x, y, w, h, TFT_DARKGREY); // Draw rectangle outline
}

void displayTempAndAirQuality() {

  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_CYAN);
  tft.setFreeFont(LABEL2_FONT);
  tft.setTextSize(1);
  tft.drawString("AQI", 15, 10);

    if (!backgroundDrawn) {
        tft.setTextColor(TFT_WHITE);
        tft.setFreeFont(LABEL2_FONT);
        tft.setTextSize(1);
        backgroundDrawn = true;
    }

        if (!gauge1.created()) {
            gauge1.createSprite(140, 140);
            gauge1.setColorDepth(8);
        }
        if (!gauge2.created()) {
            gauge2.createSprite(140, 140);
            gauge2.setColorDepth(8);
        }
        if (!gauge3.created()) {
            gauge3.createSprite(98, 98); // Create smaller sprite for VOC gauge
            gauge3.setColorDepth(8);
        }

    // Draw to sprites instead of screen
    drawGaugeToSprite(&gauge1, 70, 70, 0, 60, temp, "Temp ", TFT_RED, 0x8800);
    drawGaugeToSprite(&gauge2, 70, 70, 0, 100, hum, "Humidity %", TFT_BLUE, 0x0011);
    drawGaugeToSprite(&gauge3, 49, 49, 0, 500, voc_index, "VOC", TFT_GREEN, 0x0011); // Adjusted center coordinates for smaller gauge

    // Push sprites to screen with VOC gauge on the right
    gauge1.pushSprite(3, 42);
    gauge2.pushSprite(3, 182);
    gauge3.pushSprite(139, 129);

    // Draw back button
    screenButton.initButton(&tft, 200, 20, 60, 30, TFT_WHITE, TFT_BLUE, TFT_WHITE, backButtonLabel, 1);
    screenButton.drawButton();
    // Log available memory after drawing gauges
    Serial.printf("Free RAM after drawing gauges: %d bytes\n", rp2040.getFreeHeap());

    updateTempAndAirQualityDisplay();
}

void updateTempAndAirQualityDisplay() {
    unsigned long currentMillis = millis();
    lastSensorUpdate = currentMillis;

        if (!gauge1.created()) {
            gauge1.createSprite(140, 140);
            gauge1.setColorDepth(8);
        }
        if (!gauge2.created()) {
            gauge2.createSprite(140, 140);
            gauge2.setColorDepth(8);
        }
        if (!gauge3.created()) {
            gauge3.createSprite(98, 98); // Create smaller sprite for VOC gauge
            gauge3.setColorDepth(8);
        }
    if (voc_index > 100) voc_color = TFT_GREENYELLOW;
    if (voc_index > 200) voc_color = TFT_YELLOW;
    if (voc_index > 300) voc_color = TFT_ORANGE;
    if (voc_index > 400) voc_color = TFT_RED;

    drawGaugeToSprite(&gauge1, 70, 70, 0, 60, temp, "Temp C", TFT_RED, 0x8800);
    drawGaugeToSprite(&gauge2, 70, 70, 0, 100, hum, "Humidity %", TFT_BLUE, 0x0011);
    drawGaugeToSprite(&gauge3, 49, 49, 0, 500, voc_index, "VOC", voc_color, 0x0011); // Adjusted center coordinates for smaller gauge
   
    // Push sprites to screen with VOC gauge on the right
    gauge1.pushSprite(3, 42);
    gauge2.pushSprite(3, 182);
    gauge3.pushSprite(139, 129);

    // Log available memory after updating gauges
    Serial.printf("Free RAM after updating gauges: %d bytes\n", rp2040.getFreeHeap());
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
      // Draw back button
    screenButton.initButton(&tft, 200, 20, 60, 30, TFT_WHITE, TFT_BLUE, TFT_WHITE, backButtonLabel, 1);
    screenButton.drawButton();
}

void displayLEDControl() {
  bool currentState = neopixelState; // Store current state to detect changes
  tft.fillScreen(TFT_BLACK);

  tft.setTextColor(TFT_WHITE);
  
  // Draw LED status
  tft.drawString("Status:", 30, 220);
  tft.setTextColor(currentState ? TFT_GREEN : TFT_RED);
  tft.drawString(currentState ? "ON" : "OFF", 120, 220);
  
  // Initialize toggle button
  mainMenuButtons[0].initButton(&tft, 120, 260, 160, 40, TFT_WHITE, 
    currentState ? TFT_RED : TFT_GREEN, 
    TFT_WHITE, 
    currentState ? ledOffLabel : ledOnLabel, 1);
  mainMenuButtons[0].drawButton();
  
  // Redraw the back button
  screenButton.initButton(&tft, 200, 20, 60, 30, TFT_WHITE, TFT_BLUE, TFT_WHITE, backButtonLabel, 1);
  screenButton.drawButton();

  // Draw NeoPixel brightness slider
  tft.setTextColor(TFT_CYAN);
  tft.setFreeFont(LABEL2_FONT);
  tft.setTextSize(0);
  tft.drawString("Neopixel", 20, 20);
  tft.drawString("Brightness", 30, 60);
  tft.fillRect(95, 85, 70, 30, TFT_BLACK);
  tft.setTextColor(TFT_GREEN);
  tft.drawString(String(int(neoPixelBrightness)) + "%", 100, 90);

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
  param.startPosition = neoPixelBrightness;
  param.sliderDelay = 0;

  // Draw the slider
  slider2.drawSlider(20, 120, param);

  int16_t x, y;    // x and y can be negative
  uint16_t w, h;   // Width and height
  slider2.getBoundingRect(&x, &y, &w, &h);     // Update x,y,w,h with bounding box
  tft.drawRect(x, y, w, h, TFT_DARKGREY); // Draw rectangle outline
  slider2.setSliderPosition(66);
  slider2.setSliderPosition(neoPixelBrightness);
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
  param.slotWidth = 11;
  param.slotLength = 200;
  param.slotColor = TFT_CYAN;
  param.slotBgColor = TFT_BLACK;
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

  SliderWidget* sliders[] = {&slider1, &slider2, &slider3};
  
  for (int i = 0; i < 3; i++) {
    int yOffset = i * 90;  // Space between fan controls
    param.startPosition = 9;    
    // Initialize slider position before drawing
    sliders[i]->drawSlider(20, 80 + yOffset, param);
    sliders[i]->setSliderPosition(currentFanSpeeds[i]);
    // Draw current speed percentage
    tft.setTextColor(TFT_DARKCYAN);
    tft.drawString(String(int(currentFanSpeeds[i])) + "%", 150, 60 + yOffset);
    tft.setTextColor(TFT_WHITE);
    
    // Draw fan labels
    tft.drawString("Fan " + String(i + 1), 30, 60 + yOffset);
    
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
    fileButtons[i].initButton(&tft, 120, 80 + i * 40, 200, 30, TFT_WHITE, TFT_BLUE, TFT_WHITE, (char*)"", 1);
    fileButtons[i].drawButton(false,fileNames[i]);
    i++;
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
      delay(100);
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
