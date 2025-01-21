#include <PID_v1.h>
#include <Wire.h>
#include <avr/wdt.h>
#include <EEPROM.h>

// Optimize constants and reduce RAM usage
#define SND A1
#define SENSOR A7
#define DUTY_MIN 96
#define MAGIC "FANC3"
#define I2C_ADDRESS 8
#define FAN_MIN 0.0f
#define FAN_MAX 1.0f

const double aggKp = 4, aggKi = 0.2, aggKd = 1;
const double consKp = 1, consKi = 0.05, consKd = 0.25;

// Create PID instance
PID myPID(&Input, &Output, &Setpoint, consKp, consKi, consKd, P_ON_M, REVERSE);

//Exxtremors Fan Controller

const float SENSOR_CALIBRATION = 7.95f;

int period = 1000;
unsigned long time_now = 0;

uint8_t manual = 0;
float temp = 0, temp2 = 20, speed = 0.3;
float fanSpeeds[3] = {0.3, 0.3, 0.3}; // Combines TempSpeed1/2/3

// Control flags
uint8_t flags = 0;  // Use bits instead of bools
#define FLAG_SND (1 << 0)
#define FLAG_MAN (1 << 1)
#define FLAG_TEMP (1 << 2)
#define FLAG_FAN (1 << 3)

float avgTemp = 0;
/*
    Befehlsprotokoll
  1.Byte Befehl   2. bis ... Daten  Sender  Inhalt
  0x01            keine             1       Anforderung IST Temperatur                        //
  0x11            4 Byte (float)    2       Antwort IST Temperatur                            //temp
  0x02            1 Byte (0 oder 1) 1       Setze TON Aus = 0 / EIN = 1                       //flags.sndStatus
  0x03            keine             1       Abfrage Status der LED
  0x12            1 Byte            2       Sende Status der LED (Antwort auf 0x02 und 0x03)
  0x04            1 Byte (0 oder 1) 1       Manueller Modus Aus = 0 / EIN = 1                 //flags.manMod
  0x05            1 Byte (0 oder 1) 1       Soll Temperatur Runter = 0 / Hoch = 1             //flags.sollTempCTRL
  0x06            1 Byte (1-4)      1       FanSelectFAN 1 - 4                               //FanSelect
  0x07            1 Byte (0 oder 1) 1       Anforderung TempSpeed1                           //FanPWR1
  0x08            1 Byte (0 oder 1) 1       Antwort  TempSpeed1                              //FanPWR1
  0x13            1 Byte (0 oder 1) 1       FAN 1,2,3 Runter = 0 / Hoch = 1                  //flags.fanCTRL
  0x14            keine             1       Anforderung Speed                                 //FanPWR
  0x15           4 Byte (float)    2       Antwort Speed                                     //FanPWR
  0x20            keine             1        flags.sollTempCTRL                       //flags.sollTempCTRL
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
void buildResponse(int queryLen) {
  byte befehl;
  alaenge = 1;  // Minimum length is command byte
  befehl = frage[0];
  switch (befehl) {
    case 0x01: 
      antwort[0] = 0x11;
      memcpy(antwort + 1, &temp, sizeof(temp));
      alaenge += sizeof(temp);
      break;

    case 0x02:
      flags |= FLAG_SND;
      if (flags & FLAG_SND) {
        digitalWrite(SND, HIGH);
        delay(100);
        digitalWrite(SND, LOW);
      } else {
        digitalWrite(SND, LOW);
      }
      break;

    case 0x03:
      antwort[0] = 0x12;
      antwort[1] = (flags & FLAG_SND) ? 1 : 0;
      alaenge += 1;
      break;

    case 0x04:
      flags |= FLAG_MAN;
      manual = !(flags & FLAG_MAN);
      break;

    case 0x06:
      FanSelect = frage[1];
      Serial.print("Mode: ");
      Serial.println(FanSelect);

    case 0x09:
      antwort[0] = 0x10;
      memcpy(antwort + 1, &temp2, sizeof(temp2));
      alaenge += sizeof(temp2);
      break;

    case 0x13:
      flags |= FLAG_FAN;
      if (flags & FLAG_FAN) {
        speed = speed + 0.1;
      } else if (!(flags & FLAG_FAN)) {
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
      memcpy(antwort + 1, &fanSpeeds[0], sizeof(fanSpeeds[0]));
      alaenge += sizeof(fanSpeeds[0]);
      break;

    case 0x20:
      flags |= FLAG_TEMP;
      if (flags & FLAG_TEMP) {
        temp2 = temp2 + 1;
        Setpoint = (temp2 * SENSOR_CALIBRATION);
      } else if (!(flags & FLAG_TEMP)) {
        temp2 = temp2 - 1;
        Setpoint = (temp2 * SENSOR_CALIBRATION);
      }
      break;

    case 0x22:
      antwort[0] = 0x23;
      memcpy(antwort + 1, &fanSpeeds[1], sizeof(fanSpeeds[1]));
      alaenge += sizeof(fanSpeeds[1]);
      break;

    case 0x24:
      antwort[0] = 0x25;
      memcpy(antwort + 1, &fanSpeeds[2], sizeof(fanSpeeds[2]));
      alaenge += sizeof(fanSpeeds[2]);
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
  buildResponse(wieviel);
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
  Serial.print(fanSpeeds[0] * 100.0f);
  Serial.print(F(",Fan2:"));
  Serial.print(fanSpeeds[1] * 100.0f);
  Serial.print(F(",Fan3:"));
  Serial.println(fanSpeeds[2] * 100.0f);
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

// Combine fan control functions
void setFanSpeed(uint8_t fan, float speed) {
  speed = constrain(speed, FAN_MIN, FAN_MAX);
  switch(fan) {
    case 0: OCR1A = (uint16_t)(320 * speed); break;
    case 1: OCR1B = (uint16_t)(320 * speed); break;
    case 2: OCR2B = (uint8_t)(79 * speed); break;
  }
  if(fan < 3) fanSpeeds[fan] = speed;
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  wdt_enable(WDTO_2S);
  Serial.begin(9600);
  // I2C
  Wire.begin(I2C_ADDRESS);
  // Funktionen zuordnen
  Wire.onReceive(receiveEvent);
  Wire.onRequest(requestEvent);

  digitalWrite(SND, LOW);
  pinMode(SND, OUTPUT);

  ////pid
  //initialize the variables we're linked to
  Input = analogRead(SENSOR);
  Setpoint = (temp2 * SENSOR_CALIBRATION);

  //turn the PID on
  myPID.SetMode(AUTOMATIC);

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
      Input = analogRead(SENSOR);

      double gap = abs(Setpoint - Input);  //distance away from setpoint
      if (gap < 10) {                      //we're close to setpoint, use conservative tuning parameters
        myPID.SetTunings(consKp, consKi, consKd);
      } else {
        //we're far from setpoint, use aggressive tuning parameters
        myPID.SetTunings(aggKp, aggKi, aggKd);
      }

      myPID.Compute();

      Serial.println("Input");
      Serial.println(Input);
      Serial.println("SetPoint");
      Serial.println(Setpoint);
      Serial.println("Output");
      Serial.println(Output);
      Serial.println("get PID");
      Serial.print(myPID.GetKp());
      Serial.print(", ");
      Serial.print(myPID.GetKi());
      Serial.print(", ");
      Serial.print(myPID.GetKd());

      temp = (float)Input / SENSOR_CALIBRATION;
      raw = 320 / 1024 * Output;

      setFanSpeed(0, raw);  //set PWM width on pin 9
      setFanSpeed(1, raw);  //set PWM width on pin 10
      setFanSpeed(2, raw);    //set PWM width on pin 3
    }
    /////////////////////////////////////////////////////////////////////////
    /////////////////////            Manual Control           ///////////////
    /////////////////////////////////////////////////////////////////////////
  } else {
    if (FanSelect == 0) {
      fanSpeeds[0] = 0.3;
      fanSpeeds[1] = 0.3;
      fanSpeeds[2] = 0.3;
      setFanSpeed(0, fanSpeeds[0]);  //set PWM width on pin 9
      setFanSpeed(1, fanSpeeds[1]);  //set PWM width on pin 10
      setFanSpeed(2, fanSpeeds[2]);    //set PWM width on pin 3
    }

    if (FanSelect == 1) {
      if (speed < 0) { speed = 0.00; }
      if (speed > 1.00) { speed = 1.00; }
      if (fanSpeeds[0] > 1.00) { fanSpeeds[0] = 1.00; }
      if (fanSpeeds[1] > 1.00) { fanSpeeds[1] = 1.00; }
      if (fanSpeeds[2] > 1.00) { fanSpeeds[2] = 1.00; }
      if (fanSpeeds[0] < 0) { fanSpeeds[0] = 0.00; }
      if (fanSpeeds[1] < 0) { fanSpeeds[1] = 0.00; }
      if (fanSpeeds[2] < 0) { fanSpeeds[2] = 0.00; }

      float speedDiff = speed - fanSpeeds[0];
      if (abs(speedDiff) > 0.01) {
        for(uint8_t i = 0; i < 3; i++) {
          setFanSpeed(i, fanSpeeds[i] + (speedDiff > 0 ? 0.1 : -0.1));
        }
      }

      //ALL Off
      if (((fanSpeeds[0] < 0.29) && (fanSpeeds[0] > 0.1)) && ((fanSpeeds[1] < 0.29) && (fanSpeeds[1] > 0.1)) && ((fanSpeeds[2] < 0.29) && (fanSpeeds[2] > 0.1))) {
        Serial.println("Aktuell: ");
        Serial.println(speed);
        Serial.println("");
        speed = 0.00;
        fanSpeeds[0] = 0.00;
        fanSpeeds[1] = 0.00;
        fanSpeeds[2] = 0.00;
        setFanSpeed(0, fanSpeeds[0]);  //set PWM width on pin 9
        setFanSpeed(1, fanSpeeds[1]);  //set PWM width on pin 10
        setFanSpeed(2, fanSpeeds[2]);    //set PWM width on pin 3

        Serial.println(speed);
        Serial.println(fanSpeeds[0]);
        Serial.println(fanSpeeds[1]);
        Serial.println(fanSpeeds[2]);

        speedTemp = speed;
      }
      // FAN1 off
      if (((fanSpeeds[0] < 0.29) && (fanSpeeds[0] > 0.1)) || ((fanSpeeds[1] < 0.29) && (fanSpeeds[1] > 0.1)) || ((fanSpeeds[2] < 0.29) && (fanSpeeds[2] > 0.1))) {
        if (((fanSpeeds[0] < 0.29) && (fanSpeeds[0] > 0.1)) && ((!fanSpeeds[1] < 0.29) && (fanSpeeds[1] > 0.1)) && ((!fanSpeeds[2] < 0.29) && (fanSpeeds[2] > 0.1))) {
          Serial.println("Aktuell: ");
          Serial.println(speed);
          Serial.println("");
          speed = 0.00;
          fanSpeeds[0] = 0.00;
          setFanSpeed(0, fanSpeeds[0]);  //set PWM width on pin 9
          Serial.println(speed);
          Serial.println(fanSpeeds[0]);

          speedTemp = speed;
        }
        ///FAN2 off
        if (((!fanSpeeds[0] < 0.29) && (fanSpeeds[0] > 0.1)) && ((fanSpeeds[1] < 0.29) && (fanSpeeds[1] > 0.1)) && ((!fanSpeeds[2] < 0.29) && (fanSpeeds[2] > 0.1))) {
          Serial.println("Aktuell: ");
          Serial.println(speed);
          Serial.println("");
          speed = 0.00;
          fanSpeeds[1] = 0.00;
          setFanSpeed(1, fanSpeeds[1]);  //set PWM width on pin 10
          Serial.println(speed);
          Serial.println(fanSpeeds[1]);

          speedTemp = speed;
        }
        //FAN3 off
        if (((!fanSpeeds[0] < 0.29) && (fanSpeeds[0] > 0.1)) && ((!fanSpeeds[1] < 0.29) && (fanSpeeds[1] > 0.1)) && ((fanSpeeds[2] < 0.29) && (fanSpeeds[2] > 0.1))) {
          Serial.println("Aktuell: ");
          Serial.println(speed);
          Serial.println("");
          speed = 0.00;
          fanSpeeds[2] = 0.00;
          setFanSpeed(2, fanSpeeds[2]);  //set PWM width on pin 3
          Serial.println(speed);
          Serial.println(fanSpeeds[2]);

          speedTemp = speed;
        }
        ///FAN 1 & 2 off
        if (((fanSpeeds[0] < 0.29) && (fanSpeeds[0] > 0.1)) && ((fanSpeeds[1] < 0.29) && (fanSpeeds[1] > 0.1)) && ((!fanSpeeds[2] < 0.29) && (fanSpeeds[2] > 0.1))) {
          Serial.println("Aktuell: ");
          Serial.println(speed);
          Serial.println("");
          speed = 0.00;
          fanSpeeds[0] = 0.00;
          fanSpeeds[1] = 0.00;

          setFanSpeed(0, fanSpeeds[0]);  //set PWM width on pin 9
          setFanSpeed(1, fanSpeeds[1]);  //set PWM width on pin 10

          Serial.println(speed);
          Serial.println(fanSpeeds[0]);
          Serial.println(fanSpeeds[1]);

          speedTemp = speed;
        }
        ////// FAN 1& 3 off
        if (((fanSpeeds[0] < 0.29) && (fanSpeeds[0] > 0.1)) && ((!fanSpeeds[1] < 0.29) && (fanSpeeds[1] > 0.1)) && ((fanSpeeds[2] < 0.29) && (fanSpeeds[2] > 0.1))) {
          Serial.println("Aktuell: ");
          Serial.println(speed);
          Serial.println("");
          speed = 0.00;
          fanSpeeds[0] = 0.00;
          fanSpeeds[1] = 0.00;

          setFanSpeed(0, fanSpeeds[0]);  //set PWM width on pin 9
          setFanSpeed(1, fanSpeeds[1]);  //set PWM width on pin 10

          Serial.println(speed);
          Serial.println(fanSpeeds[0]);
          Serial.println(fanSpeeds[1]);

          speedTemp = speed;
        }
        /////FAN 2 & 3 off
        if (((!fanSpeeds[0] < 0.29) && (fanSpeeds[0] > 0.1)) && ((fanSpeeds[1] < 0.29) && (fanSpeeds[1] > 0.1)) && ((fanSpeeds[2] < 0.29) && (fanSpeeds[2] > 0.1))) {
          Serial.println("Aktuell: ");
          Serial.println(speed);
          Serial.println("");
          speed = 0.00;

          fanSpeeds[1] = 0.00;
          fanSpeeds[2] = 0.00;

          setFanSpeed(1, fanSpeeds[1]);  //set PWM width on pin 10
          setFanSpeed(2, fanSpeeds[2]);    //set PWM width on pin 3

          Serial.println(speed);

          Serial.println(fanSpeeds[1]);
          Serial.println(fanSpeeds[2]);

          speedTemp = speed;
        }
      }
      // Boost All Fans
      if ((fanSpeeds[0] == 0.1) || (fanSpeeds[1] == 0.1) || (fanSpeeds[2] == 0.1)) {
        if ((fanSpeeds[0] == 0.1) && (fanSpeeds[1] != 0.1) && (fanSpeeds[2] != 0.1)) {
          BoostFan1();
          speed = 0.3;
          fanSpeeds[0] = speed;

          fanSpeeds[0] = 0.3;
          setFanSpeed(0, fanSpeeds[0]);
          speedTemp = speed;
        }
        if ((fanSpeeds[0] == 0.1) && (fanSpeeds[1] == 0.1) && (fanSpeeds[2] != 0.1)) {
          BoostFans12();
          speed = 0.3;
          fanSpeeds[0] = speed;
          fanSpeeds[1] = speed;

          fanSpeeds[1] = 0.3;
          fanSpeeds[0] = 0.3;
          setFanSpeed(0, fanSpeeds[0]);
          setFanSpeed(1, fanSpeeds[1]);
          speedTemp = speed;
        }
        if ((fanSpeeds[0] == 0.1) && (fanSpeeds[1] != 0.1) && (fanSpeeds[2] == 0.1)) {
          BoostFans13();
          speed = 0.3;
          fanSpeeds[0] = speed;
          fanSpeeds[2] = speed;
          fanSpeeds[0] = 0.3;

          fanSpeeds[2] = 0.3;
          fanSpeeds[0] = 0.3;
          setFanSpeed(0, fanSpeeds[0]);
          setFanSpeed(2, fanSpeeds[2]);
          speedTemp = speed;
        }
        if ((fanSpeeds[0] != 0.1) && (fanSpeeds[1] == 0.1) && (fanSpeeds[2] != 0.1)) {
          BoostFan2();
          speed = 0.3;
          fanSpeeds[1] = speed;

          fanSpeeds[1] = 0.3;
          setFanSpeed(1, fanSpeeds[1]);
          speedTemp = speed;
        }
        if ((fanSpeeds[0] != 0.1) && (fanSpeeds[1] == 0.1) && (fanSpeeds[2] == 0.1)) {
          BoostFans23();
          speed = 0.3;
          fanSpeeds[1] = speed;
          fanSpeeds[2] = speed;

          fanSpeeds[2] = 0.3;
          fanSpeeds[1] = 0.3;
          setFanSpeed(1, fanSpeeds[1]);
          setFanSpeed(2, fanSpeeds[2]);
          speedTemp = speed;
        }
        if ((fanSpeeds[0] != 0.1) && (fanSpeeds[1] != 0.1) && (fanSpeeds[2] == 0.1)) {
          BoostFan3();
          speed = 0.3;

          fanSpeeds[2] = speed;

          fanSpeeds[2] = 0.3;
          setFanSpeed(2, fanSpeeds[2]);
          speedTemp = speed;
        }
      }

      if ((fanSpeeds[0] == 0.1) && (fanSpeeds[1] == 0.1) && (fanSpeeds[2] == 0.1)) {
        BoostFansAll();
        speed = 0.30;
        fanSpeeds[0] = speed;
        fanSpeeds[1] = speed;
        fanSpeeds[2] = speed;
        setFanSpeed(0, fanSpeeds[0]);  //set PWM width on pin 9
        setFanSpeed(1, fanSpeeds[1]);  //set PWM width on pin 10
        setFanSpeed(2, fanSpeeds[2]);    //set PWM width on pin 3
        Serial.println(speed);
        Serial.println(fanSpeeds[0]);
        Serial.println(fanSpeeds[1]);
        Serial.println(fanSpeeds[2]);
        speedTemp = speed;
      }
    }

    if (FanSelect == 2) {
      if (speed < 0) { speed = 0.00; }
      if (speed > 1.00) { speed = 1.00; }
      if (fanSpeeds[0] > 1.00) { fanSpeeds[0] = 1.00; }
      if (fanSpeeds[0] < 0) { fanSpeeds[0] = 0.00; }

      fanSpeeds[0] = speed;
      setFanSpeed(0, fanSpeeds[0]);  //set PWM width on pin 9
      speedTemp = speed;
      if ((fanSpeeds[0] < 0.29) && (fanSpeeds[0] > 0.1)) {
        speed = 0.0;
        fanSpeeds[0] = 0.0;
        setFanSpeed(0, fanSpeeds[0]);  //set PWM width on pin 3
        speedTemp = speed;
      }
      if (fanSpeeds[0] == 0.1) {
        BoostFan1();
        speed = 0.3;
        fanSpeeds[0] = speed;
      }
    }

    if (FanSelect == 3) {
      if (speed < 0) { speed = 0.00; }
      if (speed > 1.00) { speed = 1.00; }

      if (fanSpeeds[1] > 1.00) { fanSpeeds[1] = 1.00; }
      if (fanSpeeds[1] < 0) { fanSpeeds[1] = 0.00; }

      fanSpeeds[1] = speed;
      setFanSpeed(1, fanSpeeds[1]);  //set PWM width on pin 10
      speedTemp = speed;
      if ((fanSpeeds[1] < 0.29) && (fanSpeeds[1] > 0.1)) {
        fanSpeeds[1] = 0.0;
        speed = 0.0;
        setFanSpeed(1, fanSpeeds[1]);  //set PWM width on pin 3
        speedTemp = speed;
      }
      if (fanSpeeds[1] == 0.1) {
        BoostFan2();
        speed = 0.3;
        fanSpeeds[1] = speed;
      }
    }
    if (FanSelect == 4) {
      if (speed < 0) { speed = 0.00; }
      if (speed > 1.00) { speed = 1.00; }

      if (fanSpeeds[2] > 1.00) { fanSpeeds[2] = 1.00; }
      if (fanSpeeds[2] < 0) { fanSpeeds[2] = 0.00; }

      fanSpeeds[2] = speed;
      setFanSpeed(2, fanSpeeds[2]);  //set PWM width on pin 3
      speedTemp = speed;

      if ((fanSpeeds[2] < 0.29) && (fanSpeeds[2] > 0.1)) {
        fanSpeeds[2] = 0.0;
        speed = 0.0;
        setFanSpeed(2, fanSpeeds[2]);
        speedTemp = speed;
      }
      if (fanSpeeds[2] == 0.1) {
        BoostFan3();
        speed = 0.3;
        fanSpeeds[2] = speed;
      }
    }
  }
}
