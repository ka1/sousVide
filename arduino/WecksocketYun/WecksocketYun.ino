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

   port->begin(9600);

   pinMode(ledPin, OUTPUT);
   digitalWrite(ledPin, LOW);
   
   lastTime = millis();
}


void getAnalog(int pin, int id) {
   // read analog value and map/constrain to output range
   int cur = analogRead(pin);
  
   // if value changed, forward on serial (as ASCII)
   // changed to: always send!
   // if (cur != last) {
      last = cur;
      port->print(id);
      port->print('\t');
      port->print(last);
      port->println();
   //}  
}


void loop() {
  
   // control LED via commands read from serial  
   if (port->available()) {
      int inByte = port->read();
      if (inByte == '0') {
         digitalWrite(ledPin, LOW);
      } else if (inByte == '1') {
         digitalWrite(ledPin, HIGH);
      }
   }
  
  //send serial data once every x seconds
  if (millis() - lastTime > sendDelayMillis) {
   getAnalog(0, A0);
   lastTime = millis();
  }

   // limit update frequency to 50Hz
   delay(20);
}
