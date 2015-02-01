//fuer den DS18B20 Temperatursensor:
#include <DallasTemperature.h>
#include <OneWire.h>

//fuer das Display:
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

//1 if using the max31855 thermocouple, set 0 otherwise
//#define THERMOMAX 0

///////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2012-2013 Tavendo GmbH
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
//
///////////////////////////////////////////////////////////////////////////////

#include <PID_v1.h>
//#include <PID_AutoTune_v0.h> //autotune
//#include <Adafruit_MAX31855.h>

//Adafruit_MAX31855.h:
//int thermoDO = 5;
//int thermoCS = 6;
//int thermoCLK = 7;

const int ledPin = 13;
const int soundPin = 8;
const int heaterPin = 12;
int last = 0;

HardwareSerial *port;

unsigned long lastTime;
int sendDelayMillis = 1000; //how often to send a temperature value. be careful to change that, as a lot is depending on this timing

//thermo settings for later
float reference_voltage;
float temp_range_max;
float temp_range_min;

//define the size of the median buffer
//#if THERMOMAX
//  #define RAWDATASIZE 10
//  bool thermoMax = true;
//#else
  #define RAWDATASIZE 100
//  bool thermoMax = false;
//#endif
#define RAWDATAMEDIANIDX RAWDATASIZE / 2
int rawData[RAWDATASIZE];
byte rawDataIdx = 0;

int currentTemperature1023;
#define LASTANALYSISSIZE 60
int lastTemperatures[LASTANALYSISSIZE];
int sendCounter = 0; //for the analysisDelay to run every N sends
int lastTemperaturesSize = 0;
int analysisDelay;

//MAX31855
//Adafruit_MAX31855 thermocouple(thermoCLK, thermoCS, thermoDO);

//last serial command
int last_cmd = -1;

//direct memory access
byte *pPidData;
float *pPid_settemp;
float *pPid_p;
float *pPid_i;
float *pPid_d;
float *pPid_near_p;
float *pPid_near_i;
float *pPid_near_d;
float *pPid_nearfardelta;
float *pPid_nearfartimewindow;
float *range_min;
float *range_max;
float *referenceVoltage;
float *aTuneStep;
float *aTuneNoise;
float *aTuneStartValue;
float *aTuneLookBack;

//PID
double Input, Output, Setpoint;
byte WindowSize = 10; //in seconds = *1000 MUST BE LARGER THAN 2 SECONDS! (or adjust the subtraction of 2 seconds)
unsigned long windowStartTime;
PID myPID(&Input, &Output, &Setpoint, 0, 0, 0, DIRECT); //2,5,1 or 1,0.05,0.25 (cons)
bool pidStarted = false;
bool pidNear = false; //the near PID should be used NEAR the settemp. the parameter set should be more aggressive (contain more I or I at all)
bool pidSettingsReceived = false; //only turn on if settings where received.
int minOutput = 100; //ignore any outputs less than minOutput milliseconds

//relais
unsigned long onUntilMillis = 0;
bool relaisIsOn = false;

//Autotune
//byte ATuneModeRemember = 2;
//double aTuneStep = 200, aTuneNoise = 0.1, aTuneStartValue = 1000;
//unsigned int aTuneLookBack = 60;

//boolean tuning = false;
//PID_ATune aTune(&Input, &Output);

//count how often the loop runs in a second
int fps = 0;

boolean alarmTriggered = false;

//I2C
LiquidCrystal_I2C lcd(0x27, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);  // Set the LCD I2C address

//Temperatursensor DS18B20:
//Digitalport Pin 2 definieren
#define ONE_WIRE_BUS 4
//Ini oneWire instance
OneWire ourWire(ONE_WIRE_BUS);
//Dallas Temperature Library für Nutzung der oneWire Library vorbereiten 
DallasTemperature sensors(&ourWire);
DeviceAddress insideThermometer;


void setup() {
  //I2C
  lcd.begin(16,2);
  lcd.setCursor(0,0); //Start at character 4 on line 0
  lcd.print("Kaisousvide");
  lcd.setCursor(0,1);
  lcd.print("WAITING FOR PORT");


  pinMode(ledPin, OUTPUT);
  pinMode(soundPin, OUTPUT);
  pinMode(heaterPin, OUTPUT);
  digitalWrite(heaterPin, HIGH);
  digitalWrite(ledPin, HIGH);

  port = &Serial1; // Arduino Yun
  //Wait! Otherwise, at startup, the connection to the python script will not be established
  while (!port);

  //EXTERNAL AREF Voltage should be 1.134V
  analogReference(EXTERNAL);

  //wait for UBOOT finish
  port->begin(115200);
  Serial.println(F("CHECKING UBOOT"));
  lcd.setCursor(0,1);
  lcd.print("LINUX STARTING  ");
  do {
    while (port->available() > 0) {
      port->read();
    }

    Serial.println(F("WAITING FOR UBOOT"));
    delay(30000);
  } while (port->available() > 0);
  //  port->begin(9600);

  lcd.setCursor(0,1);
  lcd.print("UBOOT FOUND      ");

  //fill data array with zeros
  for (int i = 0; i < RAWDATASIZE; i++) {
    rawData[i] = 0;
  }

  lastTime = millis();

  //set the memory adresses of the pid settings
  pPidData = (byte *)malloc(64);
  pPid_settemp = (float *)pPidData;
  pPid_p = (float *)(pPidData + 4);
  pPid_i = (float *)(pPidData + 8);
  pPid_d = (float *)(pPidData + 12);
  pPid_near_p = (float *)(pPidData + 16);
  pPid_near_i = (float *)(pPidData + 20);
  pPid_near_d = (float *)(pPidData + 24);
  pPid_nearfardelta = (float *)(pPidData + 28);
  pPid_nearfartimewindow = (float *)(pPidData + 32);
  range_min = (float *)(pPidData + 36);
  range_max = (float *)(pPidData + 40);
  referenceVoltage = (float *)(pPidData + 44);
  aTuneStep = (float *)(pPidData + 48);
  aTuneNoise = (float *)(pPidData + 52);
  aTuneStartValue = (float *)(pPidData + 56);
  aTuneLookBack = (float *)(pPidData + 60);

  lcd.setCursor(0,1);
  lcd.print("LAST SETTINGS...");

  //PID
  windowStartTime = millis();
  //initialize the variables we're linked to
  Setpoint = 0;
  //tell the PID to range between 0 and the full window size
  myPID.SetOutputLimits(0, (WindowSize * 1000) - 2000); //- 1000 for a bit of time for the php process to return value
  //set sample time
  myPID.SetSampleTime(WindowSize * 1000);
  //turn the PID on
  myPID.SetMode(AUTOMATIC);

  //set all values in last temperatures array to -1
  resetLastTemperaturesArray();

  //Autotune
  //aTune.SetControlType(1); //1=PID, 0=PI

  //relais
  digitalWrite(heaterPin, HIGH);

  //LED
  digitalWrite(ledPin, LOW);
  
  //DS18B20 Temperatursensor:
  sensors.begin();/* Inizialisieren der Dallas Temperature library */
  sensors.getAddress(insideThermometer, 0);
  sensors.setResolution(insideThermometer, 12);
  sensors.setWaitForConversion(false);
  
  lcd.setCursor(0,1);
  lcd.print("ENDING SETUP");
  lcd.clear();
}

float calculateTemperature(int rawTemp) {
  float temp = ((float)rawTemp / 1023.0) * ((float) * referenceVoltage);
  temp = ((float) * range_min) + (temp * (((float) * range_max) - ((float) * range_min)));
  return round(temp * 10.0) / 10.0;
}

float revertTemperature(double celsius) {
  float temp = (celsius - (float) *range_min) / (((float) *range_max) - ((float) *range_min));
  temp = (temp * 1023) / (float) * referenceVoltage;
  return temp;
}

int indexOfMax(int array[]) {
  int maxi = -1;
  int maxIndex = -1;
  for (int i = 0; i < RAWDATASIZE; i++) {
    if (array[i] > maxi) {
      maxi = array[i];
      maxIndex = i;
    }
  }

  return maxIndex;
}


int median(int array[]) {

  int localArray[RAWDATASIZE];
  int sortedData[RAWDATASIZE];

  //copy the global array
  for (int i = 0; i < RAWDATASIZE; i++) {
    localArray[i] = array[i];
  }

  //loop through array and sort the array in each run, filling with -1 and returning the max
  for (int i = 0; i < RAWDATASIZE; i++) {
    int maxIdx = indexOfMax(localArray);
    sortedData[i] = localArray[maxIdx];
    localArray[maxIdx] = -1;
  }

  return sortedData[RAWDATAMEDIANIDX];
}

void getAnalog(int pin, int id) {
//  if (thermoMax){
//    currentTemperature1023 = revertTemperature(median(rawData) / 100.0);
//  } else {
    currentTemperature1023 = median(rawData);
//  }

  port->print(id);
  port->print('\t');
  port->print(currentTemperature1023);
  port->println();
}


//void changeAutoTune()
//{
//  if (!tuning)
//  {
//    //Set the output to the desired starting frequency.
//    Output = *aTuneStartValue;
//    aTune.SetNoiseBand(*aTuneNoise);
//    aTune.SetOutputStep(*aTuneStep);
//    aTune.SetLookbackSec((int)*aTuneLookBack);
//    AutoTuneHelper(true);
//    tuning = true;
//    Serial.println(F("tuning started"));
//  }
//  else
//  { //cancel autotune
//    aTune.Cancel();
//    tuning = false;
//    AutoTuneHelper(false);
//    Serial.println(F("tuning stopped"));
//  }
//}

//void AutoTuneHelper(boolean start)
//{
//  if (start)
//    ATuneModeRemember = myPID.GetMode();
//  else
//    myPID.SetMode(ATuneModeRemember);
//}

int getMaxValue(int theArray[]) {
  int maxValue = 0;
  for (int i = 0; i < LASTANALYSISSIZE; i++) {
    if (theArray[i] > -1) {
      maxValue = max(theArray[i], maxValue);
    }
  }
  return maxValue;
}

int getMinValue(int theArray[]) {
  int minValue = 1023;
  for (int i = 0; i < LASTANALYSISSIZE; i++) {
    if (theArray[i] > -1) {
      minValue = min(theArray[i], minValue);
    }
  }
  return minValue;
}

float getAverageValue(int theArray[]) {
  int totalSum = 0;
  int totalCount = 0;

  for (int i = 0; i < LASTANALYSISSIZE; i++) {
    if (theArray[i] > -1) {
      totalSum += theArray[i];
      totalCount++;
    }
  }

  return (float) totalSum / (float) totalCount;
}

void resetLastTemperaturesArray() {
  resetLastTemperaturesArray(-1, true);
}

void resetLastTemperaturesArray(int presetValue, bool resetLastTemperatureSize) {
  for (int i = 0; i < LASTANALYSISSIZE; i++) {
    lastTemperatures[i] = presetValue;
  }

  if (resetLastTemperatureSize){
    lastTemperaturesSize = 0;
  }
}

void loop() {
  
  //if receiving new PID values
  if (last_cmd == -1 && port->available()) {
    //read command
    last_cmd = port->read();
  }

  //switch relais if necessary
  if (relaisIsOn && millis() >= onUntilMillis) {
    digitalWrite(heaterPin, HIGH);
    relaisIsOn = false;
    lcd.setCursor(11,0);
    lcd.print("| OFF");
  }

  //Compute commands
  if (last_cmd != -1)
  {
    //Serial.println(port->available());
    if (last_cmd == 'P')
    {
      if (port->available() >= 48) {
        port->readBytes((char *)pPidData, 48);

        Serial.print(F("Received new PID values: Setpoint ")); Serial.print(*pPid_settemp);
        Serial.print(F("C - P")); Serial.print(*pPid_p);
        Serial.print(F(" - I"));  Serial.print(*pPid_i);
        Serial.print(F(" - D"));  Serial.print(*pPid_d);
        Serial.print(F(" - NP")); Serial.print(*pPid_near_p);
        Serial.print(F(" - NI"));  Serial.print(*pPid_near_i);
        Serial.print(F(" - ND"));  Serial.print(*pPid_near_d);
        Serial.print(F(" - NDT"));  Serial.print(*pPid_nearfardelta);
        Serial.print(F(" - NTW"));  Serial.println(*pPid_nearfartimewindow);
        Serial.print(F("Received new Thermo values: MIN ")); Serial.print(*range_min);
        Serial.print(F(" - MAX "));  Serial.print(*range_max);
        Serial.print(F(" - REFVOLT ")); Serial.println(*referenceVoltage);

        //safe to PID controller
        Setpoint = *pPid_settemp;
        //set tuning, depending on which parameter set we are running
        if (pidNear == true) {
          myPID.SetTunings(*pPid_near_p, *pPid_near_i, *pPid_near_d);
        } else {
          myPID.SetTunings(*pPid_p, *pPid_i, *pPid_d);
        }
        pidStarted = true;
        pidSettingsReceived = true;
        //calculate how long the delay has to be in order to cover the set time window
        analysisDelay = *pPid_nearfartimewindow * (1000 / sendDelayMillis);
        last_cmd = -1;
      }
    }
//    else if (last_cmd == 'Q') {
//      if (port->available() >= 16) {
//        //sending more PID parameters in a different string, because the (default) serial buffer is 64 bytes large
//        port->readBytes((char *)pPidData + 48, 16);
//
//        Serial.print(F("Received autoTune values: Start ")); Serial.print(*aTuneStartValue);
//        Serial.print(F(" - Noise "));  Serial.print(*aTuneNoise);
//        Serial.print(F(" - Step "));  Serial.print(*aTuneStep);
//        Serial.print(F(" - SetLookbackSec "));  Serial.println(*aTuneLookBack);
//
//        if (tuning) {
//          Serial.println(F("Tuning parameters reset. Restarting tuning."));
//          changeAutoTune(); //first call will cancel the tuning
//          changeAutoTune(); //second call with set the values and start again
//        }
//        last_cmd = -1;
//      }
//    }
    else if (last_cmd == 'O')
    {
      if (port->available() >= 1) {
        //only switch LED on and off
        int inByte = port->read();
        if (inByte == '0') {
          digitalWrite(ledPin, LOW);
        } else if (inByte == '1') {
          digitalWrite(ledPin, HIGH);
        }

        last_cmd = -1;
      }
    }
//    else if (last_cmd == 'A') {
//      if (port->available() >= 1) {
//        //only switch LED on and off
//        int inByte = port->read();
//        //switch tuning, if value received is different from the tuning state
//        if ((inByte == '1' && !tuning) || (inByte != '1' && tuning)) changeAutoTune();
//        last_cmd = -1;
//      }
//    }
    else if (last_cmd == 'X') {
      if (port->available() >= 1) {
        //only switch Sound on and off
        int inByte = port->read();
        //switch tuning, if value received is different from the tuning state
        if (inByte == '1') {
          alarmTriggered = true;
        } else {
          alarmTriggered = false;
          digitalWrite(soundPin, LOW);
          digitalWrite(ledPin, LOW);
        }
        last_cmd = -1;
      }
    }
    else if (last_cmd == 'R') {
      Serial.println(F("RESETTING PID"));
      //Reset PID Controler
      Setpoint = *pPid_settemp;

      myPID.ResetIterm();
      last_cmd = -1;
    }
    //toggle PID (was toggle, now switch on if 1, off if 0)
    else if (last_cmd == 'T') {
      if (port->available() >= 1) {
        int inByte = port->read();
        if (inByte == '0') {
          //switch off
          if (pidStarted == true) {
            //first switch off autotune
//            if (tuning) changeAutoTune();
            pidStarted = false;
            Serial.println(F("PID turned off"));
          }
        } else if (inByte == '1') {
          //switch on
          if (pidSettingsReceived && pidStarted == false) {
            pidStarted = true;
            Serial.println(F("PID turned on"));
          } else {
            Serial.println(F("PID settings not yet received. Still turned off"));
          }
        }
      }
      last_cmd = -1;
    }
    //power socket switch (for safety reasons only for 5 seconds
    else if (last_cmd == 'S') {
      if (port->available() >= 1) {
        int inByte = port->read();
        if (inByte == '0') {
          Serial.println(F("POWER OFF"));
          onUntilMillis = 0;
          relaisIsOn = false;
          digitalWrite(heaterPin, HIGH);
        } else {
          Serial.println(F("POWER ON FOR 5 SECONDS"));
          //switch on relais from now on
          onUntilMillis = millis() + (int) 5000;
          relaisIsOn = true;
          digitalWrite(heaterPin, LOW);
        }
        last_cmd = -1;
      }
    }
    //empty one char from buffer
    else {
      if (port->available() > 0) {
        port->read();
      }
      last_cmd = -1;
    }
  }

  //send serial data once every x seconds
  //also compute PID and Autotune
  if (millis() - lastTime >= sendDelayMillis) {
    sendCounter++;
    lastTime = millis();

    //read temperature value (Greisinger)
    getAnalog(A1, 0);
    
    //read temperature value (DS18B20)
    sensors.requestTemperatures(); // Temp abfragen
    Serial.println(sensors.getTempC(insideThermometer));
    float temperatureNow = sensors.getTempC(insideThermometer);
    
//    Serial.print("Sending ");
//    Serial.println(millis() / 1000);
    lcd.setCursor(0,0);
    lcd.print(millis() / 1000);
    lcd.print(" S, ");
    lcd.print(temperatureNow);
    
    lcd.setCursor(0,1);
    
    if (pidSettingsReceived) {
      static char cTemp[5]; //string mit aktueller temperatur (fuer display)
      dtostrf(calculateTemperature(currentTemperature1023),3,1,cTemp);
      static char sTemp[5]; //string mit gesetzter temperatur (fuer display)
      dtostrf(Setpoint,3,1,sTemp);
      lcd.print(cTemp);
      lcd.print((char)223);
      lcd.print("C | ");
      lcd.print(sTemp);
      lcd.print((char)223);
      lcd.print("C  ");
    } else {
      lcd.print("WARTE AUF WERTE");
    }
    

    //analyse last temperatures every N runs (depending on near/far analysis window size)
    if (sendCounter % analysisDelay == 0) {
      //move all values to 1 above, so that we can save to the 0 value
      for (int i = LASTANALYSISSIZE - 1; i >= 0; i--) {
        lastTemperatures[i + 1] = lastTemperatures[i];
      }
      lastTemperatures[0] = currentTemperature1023;
      lastTemperaturesSize++;
      sendCounter = 0; //reset counter
    }

    if (pidStarted) {
      //see if the window has passed. only then, compute pid or autotune
      if (millis() - windowStartTime > (WindowSize * 1000))
      { //time to shift the Relay Window
        windowStartTime += (WindowSize * 1000);

        //if the time is STILL more than one windowsSize ahead, send error and reset window
        if (millis() - windowStartTime > (WindowSize * 1000)) {
          Serial.println(F("---RESETTING WINDOW---"));
          windowStartTime = millis();
        }

        // -------- PID AND AUTOTUNE --------
        Input = calculateTemperature(currentTemperature1023);

        // See if we have to change from near to far PID parameter set
        //but only if we have filled the array
        if (lastTemperaturesSize > LASTANALYSISSIZE) {
          float theMax = calculateTemperature(getMaxValue(lastTemperatures));
          float theMin = calculateTemperature(getMinValue(lastTemperatures));
          float theAverage = calculateTemperature(getAverageValue(lastTemperatures));
          if (!pidNear) {
            //if the maximum value was lower than now + deltaT
            //or if the minimum value was larger than now - deltaT
            //ie. was the temperature always within the deltaT, relative to now
            if (theMax < (theAverage + (*pPid_nearfardelta / (float) 2)) || theMin > (theAverage - (*pPid_nearfardelta / (float) 2))) {
              Serial.println(F("CHANGING TO NEAR PARAMETER SET"));
              port->print(F("F"));
              port->println(F("NEAR"));

              pidNear = true;
              myPID.SetTunings(*pPid_near_p, *pPid_near_i, *pPid_near_d);
              //reset and fill with the current temperature, so that we can immediately switch back to far, instead of waiting for x minutes before array is filled
              resetLastTemperaturesArray(currentTemperature1023, false);
            }
          }
          //if already in near set
          else {
            //emergency fallback: go to far parameter set, if we are more than 0.6C above settemp
            if (Input >= Setpoint + 0.6) {
              Serial.println(F("EMERGENCY CHANGING TO FAR PARAMETER SET BECAUSE TOO HIGH. SET ITERM TO 0"));
              pidNear = false;
              myPID.ResetIterm();
              myPID.SetTunings(*pPid_p, *pPid_i, *pPid_d);
              resetLastTemperaturesArray();
              port->print(F("F"));
              port->println(F("FAR(EMERGENCY)"));
            }
            else {
              //if the maximum value was higher than theMin + deltaT
              //if the minimum value was lower than theMin - deltaT
              if (theMax > (theMin + *pPid_nearfardelta) || theMin < (theMax - *pPid_nearfardelta)) {
                Serial.print(F("CHANGING TO FAR PARAMETER SET, MIN WAS "));
                Serial.print(theMin);
                Serial.print(F(", MAX WAS "));
                Serial.println(theMax);
                pidNear = false;
                myPID.ResetIterm();
                myPID.SetTunings(*pPid_p, *pPid_i, *pPid_d);
                resetLastTemperaturesArray();
                port->print(F("F"));
                port->println(F("FAR"));
              }
            }
          }
        }

        // ------------ AUTOTUNE ------------
//        if (tuning)
//        {
//          byte val = (aTune.Runtime());
//          if (val != 0)
//          {
//            tuning = false;
//          }
//          if (!tuning)
//          { //we're done, set the tuning parameters
//            *pPid_p = aTune.GetKp();
//            *pPid_i = aTune.GetKi();
//            *pPid_d = aTune.GetKd();
//            Serial.println(F("Done tuning. Using new settings"));
//            Serial.println(*pPid_p);
//            Serial.println(*pPid_i);
//            Serial.println(*pPid_d);
//            myPID.SetTunings(*pPid_p, *pPid_i, *pPid_d);
//            AutoTuneHelper(false);
//            //Sending values to python
//            port->print(F("A"));
//            port->print(*pPid_p);
//            port->print('\t');
//            port->print(*pPid_i);
//            port->print('\t');
//            port->println(*pPid_d);
//          }
//        }
        // -------------- PID --------------
//        else {
          myPID.Compute();
//       }

        //echo tuning parameters
        Serial.print(F("FPS")); Serial.print(fps); Serial.print(F(" "));
        Serial.print(F("setpoint: ")); Serial.print(Setpoint); Serial.print(F(" "));
        Serial.print(F("input: ")); Serial.print(Input); Serial.print(F(" "));
        Serial.print(F("output: ")); Serial.print(Output); Serial.print(F(" "));
//        if (tuning) {
//          Serial.println(F("tuning mode"));
//        } else {
          Serial.print(F("kp: ")); Serial.print(myPID.GetKp()); Serial.print(F(" "));
          Serial.print(F("ki: ")); Serial.print(myPID.GetKi()); Serial.print(F(" "));
          Serial.print(F("kd: ")); Serial.print(myPID.GetKd()); Serial.print(F(" ||| "));
          Serial.print(F("pfactor: ")); Serial.print(myPID.GetLastPFactor()); Serial.print(F(" "));
          Serial.print(F("iterm: ")); Serial.print(myPID.GetIterm()); Serial.print(F(" "));
          Serial.print(F("dfactor: ")); Serial.print(myPID.GetLastDFactor()); Serial.print(F(" "));
          //        Serial.print(F("akp: ")); Serial.print(aTune.GetKp()); Serial.print(F(" "));
          //        Serial.print(F("aki: ")); Serial.print(aTune.GetKi()); Serial.print(F(" "));
          //        Serial.print(F("akd: ")); Serial.print(aTune.GetKd()); Serial.print(F(" "));
//        }

        if (Output > (WindowSize * 1000)) {
          Serial.println(F("PID LIB ERR")); // sanity check
        }

        if (Output > minOutput) {
          Serial.println(F(", sending signal "));
          port->print(F("P"));
          port->println(Output);

          //switch on relais from now on
          onUntilMillis = millis() + (int) Output;
          relaisIsOn = true;
          digitalWrite(heaterPin, LOW);
          
          lcd.setCursor(11,0);
          lcd.print("| ON ");
          
        } else {
          Serial.println(F(", output too small "));
          port->print(F("N"));
          port->println(Output);
        }
      }

    }
    else {
      Serial.print(F("FPS")); Serial.println(fps);
    }
    fps = 0;
  }

  //save last value
//  if (thermoMax){
    //rawData[rawDataIdx] = thermocouple.readCelsius() * 100;
//  } else {
    rawData[rawDataIdx] = analogRead(A1);
// }
  rawDataIdx++;
  rawDataIdx %= RAWDATASIZE;

  //do alarm
  if (alarmTriggered) {
    if (fps == 0) {
      digitalWrite(soundPin, HIGH);
      digitalWrite(ledPin, HIGH);
    } else if (fps == 20) {
      digitalWrite(soundPin, LOW);
      digitalWrite(ledPin, LOW);
    }
  }

  // limit update frequency to around 100Hz
  fps++;
  delay(9);
}
