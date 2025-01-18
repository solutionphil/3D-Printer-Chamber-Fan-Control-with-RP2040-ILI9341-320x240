#include "FS.h"
#include <SPI.h>
#include <TFT_eSPI.h>      // Hardware-specific library
#include <TFT_eWidget.h>      // Hardware-specific library
#include "RP2040_PWM.h"    // PWM library
TFT_eSPI tft = TFT_eSPI(); // Invoke custom library
// Create TFT sprite for slider knob and slider widget object
TFT_eSprite knob = TFT_eSprite(&tft);
SliderWidget slider = SliderWidget(&tft, &knob);

#define _PWM_LOGLEVEL_        1
#define CALIBRATION_FILE "/TouchCalData1"
#define REPEAT_CAL false
#define pinToUse      7

// Keypad parameters
#define KEY_X 45
#define KEY_Y 90
#define KEY_W 66
#define KEY_H 40
#define KEY_SPACING_X 10
#define KEY_SPACING_Y 10
#define KEY_TEXTSIZE 1
#define DISP_X 1
#define DISP_Y 10
#define DISP_W 238
#define DISP_H 50
#define DISP_TSIZE 3
#define DISP_TCOLOR TFT_CYAN
#define NUM_LEN 12
#define STATUS_X 120
#define STATUS_Y 65
#define LABEL1_FONT &FreeSansOblique12pt7b // Key label font 1 
#define LABEL2_FONT &FreeSansBold12pt7b // Key label font 2


RP2040_PWM* PWM_Instance;

float frequency = 915.53;
float dutyCycle = 90;

char numberBuffer[NUM_LEN + 1] = "";
uint8_t numberIndex = 0;

char keyLabel[15][5] = {"New", "Del", "Send", "1", "2", "3", "4", "5", "6", "7", "8", "9", ".", "0", "#" };
uint16_t keyColor[15] = {TFT_RED, TFT_BLACK, TFT_DARKGREEN, TFT_BLUE, TFT_BLUE, TFT_BLUE,
                         TFT_BLUE, TFT_BLUE, TFT_BLUE, TFT_BLUE, TFT_BLUE, TFT_BLUE,
                         TFT_BLUE, TFT_BLUE, TFT_BLUE};

TFT_eSPI_Button key[15];  // Invoke the TFT_eSPI button class and create all the button objects
TFT_eSPI_Button screenButton;  // Button to switch screens
TFT_eSPI_Button increaseButton;  // Button to switch screens
TFT_eSPI_Button decreaseButton;  // Button to switch screens
TFT_eSPI_Button fileButtons[10]; // Buttons for file explorer
String fileNames[10]; // Array to store file names
String buttonLabels[10];
TFT_eSPI_Button backButton;
TFT_eSPI_Button yesButton;
TFT_eSPI_Button noButton;

int currentScreen = 0;
const int totalScreens = 5;
char nextButtonLabel[] = "Next"; // Define the button label as a mutable char array
char incButtonLabel[] = "Increase"; // Define the button label as a mutable char array
char decButtonLabel[] = "Decrease"; // Define the button label as a mutable char array
char yesLabel[] = "YES"; // Define the button label as a mutable char array
char noLabel[] = "NO"; // Define the button label as a mutable char array
char backLabel[] = "Back"; // Define the button label as a mutable char array



void setup() {
  Serial.begin(9600);  // Use serial port
  PWM_Instance = new RP2040_PWM(pinToUse, frequency, dutyCycle);
  
  // Initialize the knob sprite early
  knob.setColorDepth(8);
  knob.createSprite(30, 40);  // Size for the slider knob
  knob.fillSprite(TFT_BLACK);
  
  delay(1000);
  PWM_Instance->setPWM(pinToUse, frequency, dutyCycle);
  tft.init();  // Initialize the TFT screen
  tft.setRotation(0);  // Set the rotation before we calibrate
  touch_calibrate();  // Calibrate the touch screen and retrieve the scaling factors
  displayScreen(currentScreen);  // Display the initial screen
  delay(1000);

  
  slider.setSliderPosition(dutyCycle);


}

void loop(void) {
  uint16_t t_x = 0, t_y = 0;  // To store the touch coordinates
  bool pressed = tft.getTouch(&t_x, &t_y);  // Check for valid touch

  if (currentScreen == 0) {  // Only check keypad buttons on the keypad screen
    for (uint8_t b = 0; b < 15; b++) {
      key[b].press(pressed && key[b].contains(t_x, t_y));  // Update button state
    }

    for (uint8_t b = 0; b < 15; b++) {
      tft.setFreeFont(b < 3 ? LABEL1_FONT : LABEL2_FONT);

      if (key[b].justReleased()) key[b].drawButton();  // Draw normal state
      if (key[b].justPressed()) handleKeyPress(b);  // Handle key press
    }
  } else if (currentScreen == 2) {
    if (pressed) {
      if (slider.checkTouch(t_x, t_y)) {
        dutyCycle = slider.getSliderPosition();
        PWM_Instance->setPWM(pinToUse, frequency, dutyCycle);
        
        // Update percentage display
        tft.fillRect(90, 110, 80, 30, TFT_BLACK);
        tft.setTextColor(TFT_GREEN);
        tft.drawString(String(int(dutyCycle)) + "%", 100, 120);
      }
    }
  }

  // Check for touch on the screen switch button
  screenButton.press(pressed && screenButton.contains(t_x, t_y));
  if (screenButton.justReleased()) screenButton.drawButton();
  if (screenButton.justPressed()) {
    currentScreen = (currentScreen + 1) % totalScreens;
    displayScreen(currentScreen);
    delay(500);  // Debounce delay
  }

  // Check for touch on brightness control buttons only on screen 3
  if (currentScreen == 3) {
    increaseButton.press(pressed && increaseButton.contains(t_x, t_y));
    decreaseButton.press(pressed && decreaseButton.contains(t_x, t_y));

    if (increaseButton.justReleased()) increaseButton.drawButton();
    if (increaseButton.justPressed()) adjustBacklight(10);  // Increase brightness

    if (decreaseButton.justReleased()) decreaseButton.drawButton();
    if (decreaseButton.justPressed()) adjustBacklight(-10);  // Decrease brightness
  }
// Check for touch on file explorer buttons only on screen 4
  if (currentScreen == 4) {
    for (uint8_t i = 0; i < 10; i++) {
      fileButtons[i].press(pressed && fileButtons[i].contains(t_x, t_y));
      if (fileButtons[i].justReleased()) fileButtons[i].drawButton();
      if (fileButtons[i].justPressed()) handleFileButtonPress(i);
    }
}}


void displayScreen(int screen) {
  tft.fillScreen(TFT_BLACK);  // Clear the screen

  switch (screen) {
    case 0:
      drawKeypad();
      break;
    case 1:
      displayScreen1();
      break;
    case 2:
      displayScreen2();
      break;
    case 3:
      displayScreen3();
      break;
    case 4:
      displayScreen4();
      break;
  }

  // Draw the screen switch button
  tft.setFreeFont(LABEL2_FONT);
  screenButton.initButton(&tft, 200, 20, 60, 30, TFT_WHITE, TFT_BLUE, TFT_WHITE, nextButtonLabel, 1);
  screenButton.drawButton();
}

void displayScreen1() {
  tft.setTextColor(TFT_WHITE);
  tft.setFreeFont(LABEL2_FONT);
  tft.setTextSize(1);
  tft.setCursor(10, 20);
  tft.print("Screen 1");
  // Add more elements for Screen 1
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
  
  // Create slider parameters
  slider_t param;
  
  // Slider slot parameters
  param.slotWidth = 9;
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
  param.sliderLT = 0;
  param.sliderRB = 100;
  param.startPosition = dutyCycle;
  param.sliderDelay = 0;
  
  // Draw the slider
  slider.drawSlider(20, 160, param);
  
  // Update percentage display
  tft.fillRect(90, 110, 80, 30, TFT_BLACK);
  tft.setTextColor(TFT_GREEN);
  tft.drawString(String(int(dutyCycle)) + "%", 100, 120);

}

void displayScreen3() {
  tft.setTextColor(TFT_WHITE);
  tft.setFreeFont(LABEL2_FONT);
  tft.setTextSize(1);
  tft.setCursor(10, 70);
  tft.print("Control Brightness");
  // Add more elements for Screen 2
  // Add brightness control buttons 
  tft.setFreeFont(LABEL2_FONT); 

  increaseButton.initButton(&tft, 120, 150, 190, 80, TFT_WHITE, TFT_GREEN, TFT_WHITE, incButtonLabel, 1); 
  decreaseButton.initButton(&tft, 120, 250, 190, 80, TFT_WHITE, TFT_RED, TFT_WHITE, decButtonLabel, 1); 
  increaseButton.drawButton(); 
  decreaseButton.drawButton(); 
}

void displayScreen4() {
    tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.setFreeFont(LABEL2_FONT);
  tft.setTextSize(1);
  tft.setCursor(10, 20);
  tft.print("File Explorer");
    tft.setFreeFont(LABEL2_FONT);
  screenButton.initButton(&tft, 200, 20, 60, 30, TFT_WHITE, TFT_BLUE, TFT_WHITE, nextButtonLabel, 1);
  screenButton.drawButton();

  // List files in SPIFFS
  File root = SPIFFS.open("/", "r");
  File file = root.openNextFile();
  uint8_t i = 0;
  while (file && i < 10) {
    String fileName = file.name();
    fileNames[i] = fileName; // Store file name in array
    buttonLabels[i] = fileName; // Store label in array
    fileButtons[i].initButton(&tft, 120, 80 + i * 30, 200, 30, TFT_WHITE, TFT_BLUE, TFT_WHITE, (char*)fileName.c_str(), 1);
    fileButtons[i].drawButton();
    file = root.openNextFile();
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
    if (SPIFFS.exists(fileName)) {
      SPIFFS.remove(fileName);
    }
  } else {
    displayFileContents(fileName);  // Show file contents if deletion is canceled
  }
  displayScreen4();  // Refresh the file explorer screen
}

void displayFileContents(String fileName) {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.setFreeFont(LABEL2_FONT);
  tft.setTextSize(1);
  tft.setCursor(10, 20);
  tft.print("File: ");
  tft.print(fileName);

  File file = SPIFFS.open(fileName, "r");
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


void handleKeyPress(uint8_t b) {
  key[b].drawButton(true);  // Draw inverted state

  if (b >= 3 && numberIndex < NUM_LEN) {
    numberBuffer[numberIndex++] = keyLabel[b][0];
    numberBuffer[numberIndex] = 0;  // Zero terminate

  } else if (b == 1 && numberIndex > 0) {
    numberBuffer[--numberIndex] = 0;  // Delete last character
    adjustBacklight(10);  // Increase backlight

  } else if (b == 2) {

    Serial.println(numberBuffer);
  } else if (b == 0) {

    numberIndex = 0;  // Reset buffer
    numberBuffer[numberIndex] = 0;  // Zero terminate
    adjustBacklight(-10);  // Decrease backlight
  }

  updateDisplay();
}

void updateDisplay() {
  tft.setTextDatum(TL_DATUM);
  tft.setFreeFont(&FreeSans18pt7b);
  tft.setTextColor(DISP_TCOLOR);

  int xwidth = tft.drawString(numberBuffer, DISP_X + 4, DISP_Y + 12);
  tft.fillRect(DISP_X + 4 + xwidth, DISP_Y + 1, DISP_W - xwidth - 5, DISP_H - 2, TFT_BLACK);
}

void adjustBacklight(int change) {
  dutyCycle = constrain(dutyCycle + change, 0, 100);
  PWM_Instance->setPWM(pinToUse, frequency, dutyCycle);
}

void drawKeypad() {
  for (uint8_t row = 0; row < 5; row++) {
    for (uint8_t col = 0; col < 3; col++) {
      uint8_t b = col + row * 3;
      tft.setFreeFont(b < 3 ? LABEL1_FONT : LABEL2_FONT);
      key[b].initButton(&tft, KEY_X + col * (KEY_W + KEY_SPACING_X),
                        KEY_Y + row * (KEY_H + KEY_SPACING_Y),
                        KEY_W, KEY_H, TFT_WHITE, keyColor[b], TFT_WHITE,
                        keyLabel[b], KEY_TEXTSIZE);
      key[b].drawButton();
    }
  }
}

void touch_calibrate() {
  uint16_t calData[5];
  uint8_t calDataOK = 0;

  if (!SPIFFS.begin()) {
    Serial.println("formatting file system");
    SPIFFS.format();
    SPIFFS.begin();
  }

  if (SPIFFS.exists(CALIBRATION_FILE)) {
    if (REPEAT_CAL) {
      SPIFFS.remove(CALIBRATION_FILE);
    } else {
      File f = SPIFFS.open(CALIBRATION_FILE, "r");
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
    File f = SPIFFS.open(CALIBRATION_FILE, "w");
    if (f) {
      f.write((const unsigned char *)calData, 14);
      f.close();
    }
  }
}
