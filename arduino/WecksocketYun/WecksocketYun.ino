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

const int ledPin = 13;
int last = 0;

HardwareSerial *port;

unsigned long lastTime;
int sendDelayMillis = 1000;

//safe PID values
float pid_settemp = 0;
float pid_p = 0;
float pid_i = 0;
float pid_d = 0;

//thermo settings for later
float reference_voltage;
float temp_range_max;
float temp_range_min;

//define the size of the median buffer
#define RAWDATASIZE 50
#define RAWDATAMEDIANIDX RAWDATASIZE / 2
int rawData[RAWDATASIZE];
byte rawDataIdx = 0;

int last_cmd = -1;
boolean last_cmd_valid = false;

union {
  unsigned long a;
  byte b[4];
  float f;
} floatUnion;

int fps = 0;

void setup() {
  // We need to use different serial ports on different Arduinos
  //
  // See:
  //   - Arduino/hardware/tools/avr/avr/include/avr/io.h
  //   - http://electronics4dogs.blogspot.de/2011/01/arduino-predefined-constants.html
  //
#ifdef __AVR_ATmega32U4__
  port = &Serial1; // Arduino Yun
#else
  port = &Serial;  // Arduino Mega and others
#endif

  //EXTERNAL AREF Voltage should be 1.134V
  analogReference(EXTERNAL);

  port->begin(9600);

  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);

  //fill data array with zeros
  for (int i = 0; i < RAWDATASIZE; i++) {
    rawData[i] = 0;
  }

  lastTime = millis();
}

float calculateTemperature(int rawTemp) {
  float temp = ((float)rawTemp / 1023.0) * ((float) reference_voltage);
  temp = ((float) temp_range_min) + (temp * (((float)temp_range_max) - ((float)temp_range_min)));
  return round(temp*10.0) / 10.0;
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

float readFourByteFloat() {
  //wait until 4 available
  while (port->available() < 4);
  
  float value;
  port->readBytes((char*)&value, 4);
  return value;
}

void loop() {
  //if receiving new PID values
  if (last_cmd_valid == -1 && port->available()) {
    //read command
    last_cmd = port->read();
  }
  
  if (last_cmd_valid != -1) 
  {    
    if ((last_cmd == 'P') && (port->available() >= 16))
    {
      float data[4];
      port->readBytes((char *)data, 16);      
      pid_settemp = data[0];
      pid_p = data[1];
      pid_i = data[2];
      pid_d = data[3];
      
      Serial.print(F("Received new PID values: Input "));
      Serial.print(pid_settemp);
      Serial.print(F("C - P"));
      Serial.print(pid_p);
      Serial.print(F(" - I"));
      Serial.print(pid_i);
      Serial.print(F(" - D"));
      Serial.println(pid_d);
         
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
  }

  //send serial data once every x seconds
  if (millis() - lastTime > sendDelayMillis) {
    getAnalog(0, A0);
    lastTime = millis();
    Serial.print("FPS");
    Serial.println(fps);
    fps = 0;
  }

  //save last value
  rawData[rawDataIdx] = analogRead(A0);
  rawDataIdx++;
  rawDataIdx %= RAWDATASIZE;

  // limit update frequency to 50Hz
  fps++;
  delay(6);
}
