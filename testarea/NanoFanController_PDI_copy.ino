#include <PID_v1.h>

//Define Variables we'll be connecting to
double Setpoint, input, output;
double errSum, lastInput;

//Define the aggressive and conservative Tuning Parameters
double aggKp = 4, aggKi = 0.2, aggKd = 1;
double consKp = 1, consKi = 0.05, consKd = 0.25;
//Specify the links and initial tuning parameters
PID myPID(&input, &output, &Setpoint, consKp, consKi, consKd, P_ON_M, REVERSE);

//Exxtremors Fan Controller

#include <Wire.h>
#include <avr/wdt.h>
#include <EEPROM.h>

#define SND A1
#define SENSOR A7  //Change this to A0 if using an Arduino Uno

const char MAGIC[] = "FANC3";

float SENSOR_CALIBRATION = 7.95f;

int period = 1000;
unsigned long time_now = 0;

boolean manual = false;
float temp = 0, temp2 = 20, speed = 0.3, speedTemp = 0.3, TempSpeed1 = 0.3, TempSpeed2 = 0.3, TempSpeed3 = 0.3;

float raw = 0;
float raw2 = 0;
float raw3 = 0;
float raw4 = 0;
// LED
boolean sndStatus = false;
boolean manMod = false;
boolean sollTempCTRL = false;
boolean FanCTRL = false;

//Fan Mode & Fan Control Switch
int FanSelect = 0;
boolean Fan1CTRL = false;
boolean Fan2CTRL = false;
boolean Fan3CTRL = false;

//PID
//double Kp = 2, Ki = 0.5, Kd = 0.25;

#define DUTY_MIN 96  // The minimum fans speed (0...320)
float avgTemp = 0;
/*
    Befehlsprotokoll
  1.Byte Befehl   2. bis ... Daten  Sender  Inhalt
  0x01            keine             1       Anforderung IST Temperatur                        //
  0x11            4 Byte (float)    2       Antwort IST Temperatur                            //temp
  0x02            1 Byte (0 oder 1) 1       Setze TON Aus = 0 / EIN = 1                       //sndStatus
  0x03            keine             1       Abfrage Status der LED
  0x12            1 Byte            2       Sende Status der LED (Antwort auf 0x02 und 0x03)
  0x04            1 Byte (0 oder 1) 1       Manueller Modus Aus = 0 / EIN = 1                 //manMod
  0x05            1 Byte (0 oder 1) 1       Soll Temperatur Runter = 0 / Hoch = 1             //sollTempCTRL
  0x06            1 Byte (1-4)      1       FanSelectFAN 1 - 4                               //FanSelect
  0x07            1 Byte (0 oder 1) 1       Anforderung TempSpeed1                           //FanPWR1
  0x08            1 Byte (0 oder 1) 1       Antwort  TempSpeed1                              //FanPWR1
  0x13            1 Byte (0 oder 1) 1       FAN 1,2,3 Runter = 0 / Hoch = 1                  //FanCTRL
  0x14            keine             1       Anforderung Speed                                 //FanPWR
  0x15           4 Byte (float)    2       Antwort Speed                                     //FanPWR
  0x20            keine             1        sollTempCTRL                       //sollTempCTRL
  0x21           4 Byte (float)    2                                  //
  0x22            keine             1       Anforderung TempSpeed2                         //FanPWR2
  0x23           4 Byte (float)    2       Antwort TempSpeed 2                            //FanPWR2
  0x24            keine             1       Anforderung TempSpeed 3                        //FanPWR3
  0x25          4 Byte (float)    2       Antwort TempSpeed 3                        //FanPWR3
  0x09            keine             1       Anforderung SOLL Temperatur                       //
  0x10            4 Byte (float)    2       Antwort SOLL Temperatur                           //temp2
  0xFF            keine             2       unbekannter Befehl / Error //
*/

// Buffer I2C vom MasterSetpoint
char frage[10];
// Buffer I2C zum Master
char antwort[10];
// Antwortlänge
int alaenge;

// Auswerten I2C-Befehl und Antwortdaten vorbereiten
// return: Länge der Antwort
void AntwortBilden(int frageLen) {
  byte befehl;

  alaenge = 1;  // mindestens das Befehlsbyte
  befehl = frage[0];
  switch (befehl) {

    case 0x1:
      antwort[0] = 0x11;
      memcpy(antwort + 1, &temp, sizeof(temp));
      alaenge += sizeof(temp);
      break;

    case 0x2:
      sndStatus = frage[1];
      if (sndStatus) {
        digitalWrite(SND, HIGH);
        delay(100);
        digitalWrite(SND, LOW);
      } else {
        digitalWrite(SND, LOW);
      }
      break;

    case 0x3:
      antwort[0] = 0x12;
      antwort[1] = sndStatus;
      alaenge += 1;
      break;

    case 0x4:
      manMod = frage[1];
      if (!manMod) {
        manual = true;

      } else {
        manual = false;
      }
      break;

    case 0x6:
      FanSelect = frage[1];
      Serial.print("Mode: ");
      Serial.println(FanSelect);

    case 0x9:
      antwort[0] = 0x10;
      memcpy(antwort + 1, &temp2, sizeof(temp2));
      alaenge += sizeof(temp2);
      break;

    case 0x13:
      FanCTRL = frage[1];
      if (FanCTRL) {
        speed = speed + 0.1;
      } else if (!FanCTRL) {
        speed = speed - 0.1;
      }
      break;

    case 0x14:
      antwort[0] = 0x15;
      memcpy(antwort + 1, &speed, sizeof(speed));
      alaenge += sizeof(speed);
      break;

    case 0x07:
      antwort[0] = 0x08;
      memcpy(antwort + 1, &TempSpeed1, sizeof(TempSpeed1));
      alaenge += sizeof(TempSpeed1);
      break;

    case 0x20:
      sollTempCTRL = frage[1];
      if (sollTempCTRL) {
        temp2 = temp2 + 1;
        Setpoint = (temp2 * SENSOR_CALIBRATION);
        // myPID.(temp2 * SENSOR_CALIBRATION);
      } else if (!sollTempCTRL) {
        temp2 = temp2 - 1;
        Setpoint = (temp2 * SENSOR_CALIBRATION);
        //   myPID.Setpoint(temp2 * SENSOR_CALIBRATION);
      }
      break;

    case 0x22:
      antwort[0] = 0x23;
      memcpy(antwort + 1, &TempSpeed2, sizeof(TempSpeed2));
      alaenge += sizeof(TempSpeed2);
      break;

    case 0x24:
      antwort[0] = 0x25;
      memcpy(antwort + 1, &TempSpeed3, sizeof(TempSpeed3));
      alaenge += sizeof(TempSpeed3);
      break;

    default:
      antwort[0] = 0xFF;
  }
}

// Frage empfangen
void receiveEvent(int wieviel) {
  for (int i = 0; i < wieviel; i++) {
    frage[i] = Wire.read();
  }
  AntwortBilden(wieviel);
}

// Antwort senden auf Wire.requestFrom(SLAVE_ADR, menge);
void requestEvent() {
  Wire.write(antwort, alaenge);
}

void printConfig() {
  Serial.print(SENSOR_CALIBRATION);
  Serial.print(F(" "));
}

void printStatus() {
  Serial.print(F("Raw:"));
  Serial.print(raw);
  Serial.print(F(",Temp:"));
  Serial.print(temp);
  Serial.print(F(",Fan1:"));
  Serial.print(TempSpeed1 * 100.0f);
  Serial.print(F(",Fan2:"));
  Serial.print(TempSpeed2 * 100.0f);
  Serial.print(F(",Fan3:"));
  Serial.println(TempSpeed3 * 100.0f);
}

bool loadSettings() {

  uint16_t eadr = 0;
  auto eepromReadFloat = [&]() {
    float f;
    EEPROM.get(eadr, f);
    eadr += sizeof(f);
    return f;
  };
  bool magicOk = true;
  char m[sizeof(MAGIC)];
  EEPROM.get(eadr, m);
  if (strcmp(m, MAGIC) != 0) {
    return false;
  }
  eadr += sizeof(MAGIC);
  SENSOR_CALIBRATION = eepromReadFloat();
  return true;
}

void saveSettings() {

  uint16_t eadr = 0;
  auto eepromWriteFloat = [&](float f) {
    EEPROM.put(eadr, f);
    eadr += sizeof(f);
  };
  EEPROM.put(eadr, MAGIC);
  eadr += sizeof(MAGIC);
  eepromWriteFloat(SENSOR_CALIBRATION);
}
void FansAllOff() {
  OCR1A = (uint16_t)(320 * speed);  //set PWM width on pin 9
  OCR1B = (uint16_t)(320 * speed);  //set PWM width on pin 10
  OCR2B = (uint8_t)(79 * speed);    //set PWM width on pin 3
}
void SetFansAll() {
  OCR1A = (uint16_t)(320 * TempSpeed1);  //set PWM width on pin 9
  OCR1B = (uint16_t)(320 * TempSpeed2);  //set PWM width on pin 10
  OCR2B = (uint8_t)(79 * TempSpeed3);    //set PWM width on pin 3
}
void SetFan1() {
  OCR1A = (uint16_t)(320 * TempSpeed1);  //set PWM width on pin 9
}
void SetFan2() {
  OCR1B = (uint16_t)(320 * TempSpeed2);  //set PWM width on pin 10
}
void SetFan3() {
  OCR2B = (uint8_t)(79 * TempSpeed3);  //set PWM width on pin 3
}
void BoostFansAll() {
  OCR1A = 320;
  OCR1B = 320;
  OCR2B = 79;
  delay(1000);
  OCR1A = 96;
  OCR1B = 96;
  OCR2B = 24;
}
void BoostFans12() {
  OCR1A = 320;
  OCR1B = 320;
  delay(1000);
  OCR1A = 96;
  OCR1B = 96;
}
void BoostFans13() {
  OCR1A = 320;
  OCR2B = 79;
  delay(1000);
  OCR1A = 96;
  OCR2B = 24;
}
void BoostFans23() {
  OCR1B = 320;
  OCR2B = 79;
  delay(1000);
  OCR1B = 96;
  OCR2B = 24;
}
void BoostFan1() {
  OCR1A = 320;
  delay(1000);
  OCR1A = 96;
}
void BoostFan2() {
  OCR1B = 320;
  delay(1000);
  OCR1B = 96;
}
void BoostFan3() {
  OCR2B = 79;
  delay(1000);
  OCR2B = 24;
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  wdt_enable(WDTO_2S);
  Serial.begin(9600);
  // I2C
  Wire.begin(8);
  // Funktionen zuordnen
  Wire.onReceive(receiveEvent);
  Wire.onRequest(requestEvent);

  digitalWrite(SND, LOW);
  pinMode(SND, OUTPUT);

  ////pid
  //initialize the variables we're linked to
  input = analogRead(SENSOR);
  Setpoint = (temp2 * SENSOR_CALIBRATION);

  //turn the PID on
  myPID.SetMode(AUTOMATIC);


  //myPID.Start(raw,                            // input
  //            0,                              // current output
  //           (temp2 * SENSOR_CALIBRATION));  // setpoint
  myPID.SetOutputLimits(DUTY_MIN, 320);
  myPID.SetSampleTime(997);

  ///
  pinMode(9, OUTPUT);
  pinMode(10, OUTPUT);
  pinMode(3, OUTPUT);
  loadSettings();
  //Set PWM frequency to about 25khz on pins 9,10 (timer 1 mode 10, no prescale, count to 320)
  TCCR1A = (1 << COM1A1) | (1 << COM1B1) | (1 << WGM11);
  TCCR1B = (1 << CS10) | (1 << WGM13);
  ICR1 = 320;
  //Set PWM frequency to about 25khz on pin 3 (timer 2 mode 5, prescale 8, count to 79)
  TIMSK2 = 0;
  TIFR2 = 0;
  TCCR2A = (1 << COM2B1) | (1 << WGM21) | (1 << WGM20);
  TCCR2B = (1 << WGM22) | (1 << CS21);
  OCR2A = 79;

  //startup pulse
  BoostFansAll();
  digitalWrite(LED_BUILTIN, LOW);
}

void loop() {
  wdt_reset();

  while (Serial.available()) {

    switch (Serial.read()) {
      case 'p': printStatus(); break;
      case 'g': printConfig(); break;
      case 's':
        {
          SENSOR_CALIBRATION = Serial.parseFloat();
          saveSettings();
        }
        break;

      case 'R':
        {
          void (*r)() = 0;
          r();
        }
        break;
      default: break;
    }
  }

  ////////////////////////////////////////////////////////////////
  /////////////////////            PID           /////////////////
  ////////////////////////////////////////////////////////////////

  if ((!manual) && (FanSelect == 1)) {

    if(millis() >= time_now + period){
     time_now += period;
    input = analogRead(SENSOR);

    double gap = abs(Setpoint - input);  //distance away from setpoint
    if (gap < 10) {                      //we're close to setpoint, use conservative tuning parameters
      myPID.SetTunings(consKp, consKi, consKd);
    } else {
      //we're far from setpoint, use aggressive tuning parameters
      myPID.SetTunings(aggKp, aggKi, aggKd);
    }

    myPID.Compute();

    Serial.println("input");
    Serial.println(input);
    Serial.println("SetPoint");
    Serial.println(Setpoint);
    Serial.println("output");
    Serial.println(output);
    Serial.println("get PID");
    Serial.print(myPID.GetKp());
    Serial.print(", ");
    Serial.print(myPID.GetKi());
    Serial.print(", ");
    Serial.print(myPID.GetKd());

    // raw = analogRead(SENSOR);
    temp = (float)input / SENSOR_CALIBRATION;
    //const float input = raw;
    raw = 320 / 1024 * output;
    // if(output>320){output=320;}
    //raw2 = output / 320;
    //raw3 = raw2 * 10;
    //raw4 = roundf(raw3);
    // raw2 += 0.03;
    //if (raw2 > 1.00) { raw2 = 1.00; }
    //avgTemp = raw4 / 10;

    /*Serial.println("input");
    Serial.println(input / SENSOR_CALIBRATION);
    Serial.println("output");
    Serial.println(output);
    Serial.println("raw2");
    Serial.println(raw2);
    Serial.println("raw3");
    Serial.println(raw3);
    Serial.println("raw4");
    Serial.println(raw4);
    Serial.println("avgtemp");
    Serial.println(avgTemp);
*/
    OCR1A = (uint16_t)(320 * raw);  //set PWM width on pin 9
    OCR1B = (uint16_t)(320 * raw);  //set PWM width on pin 10
    OCR2B = (uint8_t)(79 * raw);    //set PWM width on pin 3
    }
   /// delay(1500);
    /////////////////////////////////////////////////////////////////////////
    /////////////////////            Manual Control           ///////////////
    /////////////////////////////////////////////////////////////////////////
  } else {

    if (FanSelect == 0) {
      TempSpeed1 = 0.3;
      TempSpeed2 = 0.3;
      TempSpeed3 = 0.3;
      OCR1A = (uint16_t)(320 * TempSpeed1);  //set PWM width on pin 9
      OCR1B = (uint16_t)(320 * TempSpeed2);  //set PWM width on pin 10
      OCR2B = (uint8_t)(79 * TempSpeed3);    //set PWM width on pin 3
    }

    if (FanSelect == 1) {

      if (speed < 0) { speed = 0.00; }
      if (speed > 1.00) { speed = 1.00; }
      if (TempSpeed1 > 1.00) { TempSpeed1 = 1.00; }
      if (TempSpeed2 > 1.00) { TempSpeed2 = 1.00; }
      if (TempSpeed3 > 1.00) { TempSpeed3 = 1.00; }
      if (TempSpeed1 < 0) { TempSpeed1 = 0.00; }
      if (TempSpeed2 < 0) { TempSpeed2 = 0.00; }
      if (TempSpeed3 < 0) { TempSpeed3 = 0.00; }


      //-10%
      if (speed < speedTemp) {
        TempSpeed1 = TempSpeed1 - 0.1;
        TempSpeed2 = TempSpeed2 - 0.1;
        TempSpeed3 = TempSpeed3 - 0.1;
        SetFansAll();
        Serial.println(speed);
        Serial.println(TempSpeed1);
        Serial.println(TempSpeed2);
        Serial.println(TempSpeed3);
        speedTemp = speed;
      }
      //+10%
      if (speed > speedTemp) {
        TempSpeed1 = TempSpeed1 + 0.1;
        TempSpeed2 = TempSpeed2 + 0.1;
        TempSpeed3 = TempSpeed3 + 0.1;
        SetFansAll();
        Serial.println(speed);
        Serial.println(TempSpeed1);
        Serial.println(TempSpeed2);
        Serial.println(TempSpeed3);
        speedTemp = speed;
      }


      //ALL Off
      if (((TempSpeed1 < 0.29) && (TempSpeed1 > 0.1)) && ((TempSpeed2 < 0.29) && (TempSpeed2 > 0.1)) && ((TempSpeed3 < 0.29) && (TempSpeed3 > 0.1))) {
        Serial.println("Aktuell: ");
        Serial.println(speed);
        Serial.println("");
        speed = 0.00;
        TempSpeed1 = 0.00;
        TempSpeed2 = 0.00;
        TempSpeed3 = 0.00;
        OCR1A = (uint16_t)(320 * TempSpeed1);  //set PWM width on pin 9
        OCR1B = (uint16_t)(320 * TempSpeed2);  //set PWM width on pin 10
        OCR2B = (uint8_t)(79 * TempSpeed3);    //set PWM width on pin 3

        Serial.println(speed);
        Serial.println(TempSpeed1);
        Serial.println(TempSpeed2);
        Serial.println(TempSpeed3);

        speedTemp = speed;
      }
      // FAN1 off
      if (((TempSpeed1 < 0.29) && (TempSpeed1 > 0.1)) || ((TempSpeed2 < 0.29) && (TempSpeed2 > 0.1)) || ((TempSpeed3 < 0.29) && (TempSpeed3 > 0.1))) {
        if (((TempSpeed1 < 0.29) && (TempSpeed1 > 0.1)) && ((!TempSpeed2 < 0.29) && (TempSpeed2 > 0.1)) && ((!TempSpeed3 < 0.29) && (TempSpeed3 > 0.1))) {
          Serial.println("Aktuell: ");
          Serial.println(speed);
          Serial.println("");
          speed = 0.00;
          TempSpeed1 = 0.00;
          OCR1A = (uint16_t)(320 * TempSpeed1);  //set PWM width on pin 9
          Serial.println(speed);
          Serial.println(TempSpeed1);

          speedTemp = speed;
        }
        ///FAN2 off
        if (((!TempSpeed1 < 0.29) && (TempSpeed1 > 0.1)) && ((TempSpeed2 < 0.29) && (TempSpeed2 > 0.1)) && ((!TempSpeed3 < 0.29) && (TempSpeed3 > 0.1))) {
          Serial.println("Aktuell: ");
          Serial.println(speed);
          Serial.println("");
          speed = 0.00;
          TempSpeed2 = 0.00;
          OCR1B = (uint16_t)(320 * TempSpeed2);  //set PWM width on pin 9
          Serial.println(speed);
          Serial.println(TempSpeed2);

          speedTemp = speed;
        }
        //FAN3 off
        if (((!TempSpeed1 < 0.29) && (TempSpeed1 > 0.1)) && ((!TempSpeed2 < 0.29) && (TempSpeed2 > 0.1)) && ((TempSpeed3 < 0.29) && (TempSpeed3 > 0.1))) {
          Serial.println("Aktuell: ");
          Serial.println(speed);
          Serial.println("");
          speed = 0.00;
          TempSpeed3 = 0.00;
          OCR2B = (uint8_t)(79 * TempSpeed3);  //set PWM width on pin 3
          Serial.println(speed);
          Serial.println(TempSpeed3);

          speedTemp = speed;
        }
        ///FAN 1 & 2 off
        if (((TempSpeed1 < 0.29) && (TempSpeed1 > 0.1)) && ((TempSpeed2 < 0.29) && (TempSpeed2 > 0.1)) && ((!TempSpeed3 < 0.29) && (TempSpeed3 > 0.1))) {
          Serial.println("Aktuell: ");
          Serial.println(speed);
          Serial.println("");
          speed = 0.00;
          TempSpeed1 = 0.00;
          TempSpeed2 = 0.00;

          OCR1A = (uint16_t)(320 * TempSpeed1);  //set PWM width on pin 9
          OCR1B = (uint16_t)(320 * TempSpeed2);  //set PWM width on pin 10


          Serial.println(speed);
          Serial.println(TempSpeed1);
          Serial.println(TempSpeed2);

          speedTemp = speed;
        }
        ////// FAN 1& 3 off
        if (((TempSpeed1 < 0.29) && (TempSpeed1 > 0.1)) && ((!TempSpeed2 < 0.29) && (TempSpeed2 > 0.1)) && ((TempSpeed3 < 0.29) && (TempSpeed3 > 0.1))) {
          Serial.println("Aktuell: ");
          Serial.println(speed);
          Serial.println("");
          speed = 0.00;
          TempSpeed1 = 0.00;
          TempSpeed2 = 0.00;

          OCR1A = (uint16_t)(320 * TempSpeed1);  //set PWM width on pin 9
          OCR1B = (uint16_t)(320 * TempSpeed2);  //set PWM width on pin 10


          Serial.println(speed);
          Serial.println(TempSpeed1);
          Serial.println(TempSpeed2);


          speedTemp = speed;
        }
        /////FAN 2 & 3 off
        if (((!TempSpeed1 < 0.29) && (TempSpeed1 > 0.1)) && ((TempSpeed2 < 0.29) && (TempSpeed2 > 0.1)) && ((TempSpeed3 < 0.29) && (TempSpeed3 > 0.1))) {
          Serial.println("Aktuell: ");
          Serial.println(speed);
          Serial.println("");
          speed = 0.00;

          TempSpeed2 = 0.00;
          TempSpeed3 = 0.00;

          OCR1B = (uint16_t)(320 * TempSpeed2);  //set PWM width on pin 10
          OCR2B = (uint8_t)(79 * TempSpeed3);    //set PWM width on pin 3

          Serial.println(speed);

          Serial.println(TempSpeed2);
          Serial.println(TempSpeed3);

          speedTemp = speed;
        }
      }
      // Boost All Fans
      if ((TempSpeed1 == 0.1) || (TempSpeed2 == 0.1) || (TempSpeed3 == 0.1)) {
        if ((TempSpeed1 == 0.1) && (TempSpeed2 != 0.1) && (TempSpeed3 != 0.1)) {
          BoostFan1();
          speed = 0.3;
          TempSpeed1 = speed;

          TempSpeed1 = 0.3;
          SetFan1();
          speedTemp = speed;
        }
        if ((TempSpeed1 == 0.1) && (TempSpeed2 == 0.1) && (TempSpeed3 != 0.1)) {
          BoostFans12();
          speed = 0.3;
          TempSpeed1 = speed;
          TempSpeed2 = speed;

          TempSpeed2 = 0.3;
          TempSpeed1 = 0.3;
          SetFan1();
          SetFan2();
          speedTemp = speed;
        }
        if ((TempSpeed1 == 0.1) && (TempSpeed2 != 0.1) && (TempSpeed3 == 0.1)) {
          BoostFans13();
          speed = 0.3;
          TempSpeed1 = speed;
          TempSpeed3 = speed;
          TempSpeed1 = 0.3;

          TempSpeed3 = 0.3;
          TempSpeed1 = 0.3;
          SetFan3();
          SetFan1();
          speedTemp = speed;
        }
        if ((TempSpeed1 != 0.1) && (TempSpeed2 == 0.1) && (TempSpeed3 != 0.1)) {
          BoostFan2();
          speed = 0.3;
          TempSpeed2 = speed;


          TempSpeed2 = 0.3;
          SetFan2();
          speedTemp = speed;
        }
        if ((TempSpeed1 != 0.1) && (TempSpeed2 == 0.1) && (TempSpeed3 == 0.1)) {
          BoostFans23();
          speed = 0.3;
          TempSpeed2 = speed;
          TempSpeed3 = speed;


          TempSpeed3 = 0.3;
          TempSpeed2 = 0.3;
          SetFan3();
          SetFan2();
          speedTemp = speed;
        }
        if ((TempSpeed1 != 0.1) && (TempSpeed2 != 0.1) && (TempSpeed3 == 0.1)) {
          BoostFan3();
          speed = 0.3;

          TempSpeed3 = speed;

          TempSpeed3 = 0.3;
          SetFan3();
          speedTemp = speed;
        }
      }

      if ((TempSpeed1 == 0.1) && (TempSpeed2 == 0.1) && (TempSpeed3 == 0.1)) {
        BoostFansAll();
        speed = 0.30;
        TempSpeed1 = speed;
        TempSpeed2 = speed;
        TempSpeed3 = speed;
        OCR1A = (uint16_t)(320 * TempSpeed1);  //set PWM width on pin 9
        OCR1B = (uint16_t)(320 * TempSpeed2);  //set PWM width on pin 10
        OCR2B = (uint8_t)(79 * TempSpeed3);    //set PWM width on pin 3
        Serial.println(speed);
        Serial.println(TempSpeed1);
        Serial.println(TempSpeed2);
        Serial.println(TempSpeed3);
        speedTemp = speed;
      }
    }

    if (FanSelect == 2) {

      if (speed < 0) { speed = 0.00; }
      if (speed > 1.00) { speed = 1.00; }
      if (TempSpeed1 > 1.00) { TempSpeed1 = 1.00; }
      if (TempSpeed1 < 0) { TempSpeed1 = 0.00; }


      TempSpeed1 = speed;
      SetFan1();  //set PWM width on pin 9
      speedTemp = speed;
      if ((TempSpeed1 < 0.29) && (TempSpeed1 > 0.1)) {
        speed = 0.0;
        TempSpeed1 = 0.0;
        SetFan1();  //set PWM width on pin 3
        speedTemp = speed;
      }
      if (TempSpeed1 == 0.1) {
        BoostFan1();
        speed = 0.3;
        TempSpeed1 = speed;
      }
    }


    if (FanSelect == 3) {

      if (speed < 0) { speed = 0.00; }
      if (speed > 1.00) { speed = 1.00; }

      if (TempSpeed2 > 1.00) { TempSpeed2 = 1.00; }
      if (TempSpeed2 < 0) { TempSpeed2 = 0.00; }


      TempSpeed2 = speed;
      SetFan2();  //set PWM width on pin 10
      speedTemp = speed;
      if ((TempSpeed2 < 0.29) && (TempSpeed2 > 0.1)) {
        TempSpeed2 = 0.0;
        speed = 0.0;
        SetFan2();  //set PWM width on pin 3
        speedTemp = speed;
      }
      if (TempSpeed2 == 0.1) {
        BoostFan2();
        speed = 0.3;
        TempSpeed2 = speed;
      }
    }
    if (FanSelect == 4) {

      if (speed < 0) { speed = 0.00; }
      if (speed > 1.00) { speed = 1.00; }

      if (TempSpeed3 > 1.00) { TempSpeed3 = 1.00; }
      if (TempSpeed3 < 0) { TempSpeed3 = 0.00; }

      TempSpeed3 = speed;
      SetFan3();  //set PWM width on pin 3
      speedTemp = speed;

      if ((TempSpeed3 < 0.29) && (TempSpeed3 > 0.1)) {
        TempSpeed3 = 0.0;
        speed = 0.0;
        SetFan3();
        speedTemp = speed;
      }
      if (TempSpeed3 == 0.1) {
        BoostFan3();
        speed = 0.3;
        TempSpeed3 = speed;
      }
    }
  }
}
