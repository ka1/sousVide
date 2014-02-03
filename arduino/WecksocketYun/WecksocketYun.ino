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
#include <PID_AutoTune_v0.h>

const int ledPin = 13;
int last = 0;

HardwareSerial *port;

unsigned long lastTime;
int sendDelayMillis = 1000;

//thermo settings for later
float reference_voltage;
float temp_range_max;
float temp_range_min;

//define the size of the median buffer
#define RAWDATASIZE 100
#define RAWDATAMEDIANIDX RAWDATASIZE / 2
int rawData[RAWDATASIZE];
byte rawDataIdx = 0;

//last serial command
int last_cmd = -1;

//direct memory access
byte *pPidData;
float *pPid_settemp;
float *pPid_p;
float *pPid_i;
float *pPid_d;
float *range_min;
float *range_max;
float *referenceVoltage;

//PID
double Input, Output, Setpoint;
byte WindowSize = 10; //in seconds = *1000 MUST BE LARGER THAN 2 SECONDS! (or adjust the subtraction of 2 seconds)
unsigned long windowStartTime;
PID myPID(&Input, &Output, &Setpoint, 0, 0, 0, DIRECT); //2,5,1 or 1,0.05,0.25 (cons)
bool pidStarted = false;
int minOutput = 100; //ignore any outputs less than minOutput milliseconds

//Autotune
byte ATuneModeRemember = 2;
double outputStart = 5;
double aTuneStep = 200, aTuneNoise = 0.1, aTuneStartValue = 1000;
unsigned int aTuneLookBack = 60;
boolean tuning = false;
PID_ATune aTune(&Input, &Output);

//count how often the loop runs in a second
int fps = 0;

void setup() {
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, HIGH);

  port = &Serial1; // Arduino Yun
  //Wait! Otherwise, at startup, the connection to the python script will not be established
  while (!Serial1);

  //EXTERNAL AREF Voltage should be 1.134V
  analogReference(EXTERNAL);

  port->begin(9600);

  //fill data array with zeros
  for (int i = 0; i < RAWDATASIZE; i++) {
    rawData[i] = 0;
  }

  lastTime = millis();

  //set the memory adresses of the pid settings
  pPidData = (byte *)malloc(28);
  pPid_settemp = (float *)pPidData;
  pPid_p = (float *)(pPidData + 4);
  pPid_i = (float *)(pPidData + 8);
  pPid_d = (float *)(pPidData + 12);
  range_min = (float *)(pPidData + 16);
  range_max = (float *)(pPidData + 20);
  referenceVoltage = (float *)(pPidData + 24);

  //PID
  windowStartTime = millis();
  //initialize the variables we're linked to
  Setpoint = 0;
  //tell the PID to range between 0 and the full window size
  myPID.SetOutputLimits(0, (WindowSize * 1000) - 2000); //- 1000 for a bit of time for the php process to return value
  //set sample time
  myPID.SetSampleTime(1000);
  //turn the PID on
  myPID.SetMode(AUTOMATIC);

  digitalWrite(ledPin, LOW);
}

float calculateTemperature(int rawTemp) {
  float temp = ((float)rawTemp / 1023.0) * ((float) * referenceVoltage);
  temp = ((float) * range_min) + (temp * (((float) * range_max) - ((float) * range_min)));
  return round(temp * 10.0) / 10.0;
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
  // read analog value and map/constrain to output range
  int cur = median(rawData);

  port->print(id);
  port->print('\t');
  port->print(cur);
  port->println();
  //}
}


void changeAutoTune()
{
 if(!tuning)
  {
    //Set the output to the desired starting frequency.
    Output=aTuneStartValue;
    aTune.SetNoiseBand(aTuneNoise);
    aTune.SetOutputStep(aTuneStep);
    aTune.SetLookbackSec((int)aTuneLookBack);
    AutoTuneHelper(true);
    tuning = true;
    Serial.println(F("tuning started"));
  }
  else
  { //cancel autotune
    aTune.Cancel();
    tuning = false;
    AutoTuneHelper(false);
    Serial.println(F("tuning stopped"));
  }
}

void AutoTuneHelper(boolean start)
{
  if(start)
    ATuneModeRemember = myPID.GetMode();
  else
    myPID.SetMode(ATuneModeRemember);
}

void loop() {
  //if receiving new PID values
  if (last_cmd == -1 && port->available()) {
    //read command
    last_cmd = port->read();
  }

  //Compute commands
  if (last_cmd != -1)
  {
    if ((last_cmd == 'P') && (port->available() >= 16))
    {
      port->readBytes((char *)pPidData, 28);

      Serial.print(F("Received new PID values: Setpoint "));
      Serial.print(*pPid_settemp);
      Serial.print(F("C - P"));
      Serial.print(*pPid_p);
      Serial.print(F(" - I"));
      Serial.print(*pPid_i);
      Serial.print(F(" - D"));
      Serial.println(*pPid_d);
      Serial.print(F("Received new Thermo values: MIN "));
      Serial.print(*range_min);
      Serial.print(F(" - MAX "));
      Serial.print(*range_max);
      Serial.print(F(" - REFVOLT "));
      Serial.println(*referenceVoltage);

      //safe to PID controller
      Setpoint = *pPid_settemp;
      myPID.SetTunings(*pPid_p, *pPid_i, *pPid_d);
      pidStarted = true;
      last_cmd = -1;
    }
    else if ((last_cmd == 'O') && (port->available() >= 1))
    {
      //only switch LED on and off
      int inByte = port->read();
      if (inByte == '0') {
        digitalWrite(ledPin, LOW);
      } else if (inByte == '1') {
        digitalWrite(ledPin, HIGH);
      }

      last_cmd = -1;
    }
    else if ((last_cmd == 'A') && (port->available() >= 1)){
      //only switch LED on and off
      int inByte = port->read();
      //switch tuning, if value received is different from the tuning state
      if((inByte == '1' && !tuning) || (inByte!='1' && tuning)) changeAutoTune();
      last_cmd = -1;
    }
  }

  //send serial data once every x seconds
  //also compute PID
  if (millis() - lastTime > sendDelayMillis) {
    getAnalog(A0, 0);
    lastTime = millis();

    if (pidStarted) {
      //compute PID
      Input = calculateTemperature(median(rawData));

      //Autotune?
      if (tuning)
      {
        byte val = (aTune.Runtime());
        if (val != 0)
        {
          tuning = false;
        }
        if (!tuning)
        { //we're done, set the tuning parameters
          *pPid_p = aTune.GetKp();
          *pPid_i = aTune.GetKi();
          *pPid_d = aTune.GetKd();
          Serial.println("done tuning");
          Serial.println(*pPid_p);
          Serial.println(*pPid_i);
          Serial.println(*pPid_d);
          //myPID.SetTunings(kp,ki,kd);
          AutoTuneHelper(false);
        }
      }
      else {
        myPID.Compute();
      }

      //echo tuning parameters
      Serial.print(F("FPS")); Serial.print(fps); Serial.print(F(" "));
      Serial.print(F("setpoint: ")); Serial.print(Setpoint); Serial.print(F(" "));
      Serial.print(F("input: ")); Serial.print(Input); Serial.print(F(" "));
      Serial.print(F("output: ")); Serial.print(Output); Serial.print(F(" "));
      if (tuning) {
        Serial.println("tuning mode");
      } else {
        Serial.print("kp: "); Serial.print(myPID.GetKp()); Serial.print(" ");
        Serial.print("ki: "); Serial.print(myPID.GetKi()); Serial.print(" ");
        Serial.print("kd: "); Serial.print(myPID.GetKd()); Serial.println();
      }

      //see if the window has passed
      if (millis() - windowStartTime > (WindowSize * 1000))
      { //time to shift the Relay Window
        windowStartTime += (WindowSize * 1000);

        //if the time is STILL more than one windowsSize ahead, send error and reset window
        if (millis() - windowStartTime > (WindowSize * 1000)) {
          Serial.println(F("---RESETTING WINDOW---"));
          windowStartTime = millis();
        }

        if (Output > (WindowSize * 1000)) {
          Serial.println(F("PID LIB ERR")); // sanity check
        }

        if (Output > minOutput) {
          Serial.print(F("Input: "));
          Serial.print(Input);
          Serial.print(F(", sending output: "));
          Serial.println(Output);
          port->print(F("P"));
          port->println(Output);
        } else {
          Serial.print(F("Input: "));
          Serial.print(Input);
          Serial.print(F(", output small: "));
          Serial.println(Output);
        }
      }

    }
    fps = 0;
  }

  //save last value
  rawData[rawDataIdx] = analogRead(A0);
  rawDataIdx++;
  rawDataIdx %= RAWDATASIZE;

  // limit update frequency to around 100Hz
  fps++;
  delay(9);
}
