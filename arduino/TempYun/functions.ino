int getIntValue(String data, char separator, int index){
  String temp = getValue(data,separator,index);
  return temp.toInt();
}

long getLongValue(String data, char separator, int index){
  String temp = getValue(data,separator,index);
  return temp.toInt();
}

char *getCharValue(String data, char separator, int index, int bufferSize){
  char charBuf[bufferSize];
  String temp = getValue(data,separator,index);
  temp.toCharArray(charBuf,bufferSize);
  return charBuf;
}

String getValue(String data, char separator, int index)
{
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = data.length() - 1;

  for (int i = 0; i <= maxIndex && found <= index; i++) {
    if (data.charAt(i) == separator || i == maxIndex) {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i + 1 : i;
    }
  }
  return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}

void checkIfProcessRunning(){
  Serial.println(F("checkRunning"));
  printMem();
  if(wemoProcessStarted && (wemoProcess.running() || wemoProcess.available())) {
    int i = 0;
    while(wemoProcess.available() > 0){
      i++;
      char c = wemoProcess.read();
      Serial.print(c);
      if (i > 100) {
        i = 0;
        break;
      }
    }
    while(wemoProcess.running()){
      Serial.println(F("RUNNING"));
      delay(500);
      if (i > 100) {
        i = 0;
        break;
      }
    }
    Serial.println(F("WEMOERROR.STILLRUNNING."));
    Serial.print(F("HEAP:"));
    Serial.println(*heapptr);
    Serial.print(F("STACK (LARGER):"));
    Serial.println(*stackptr);
    wemoProcess.close();
    wemoProcess.flush();
  }
  
}

/* This function places the current value of the heap and stack pointers in the
 * variables. You can call it from any place in your code and save the data for
 * outputting or displaying later. This allows you to check at different parts of
 * your program flow.
 * The stack pointer starts at the top of RAM and grows downwards. The heap pointer
 * starts just above the static variables etc. and grows upwards. SP should always
 * be larger than HP or you'll be in big trouble! The smaller the gap, the more
 * careful you need to be. Julian Gall 6-Feb-2009.
 */
void check_mem() {
  stackptr = (uint8_t *)malloc(4);          // use stackptr temporarily
  heapptr = stackptr;                     // save value of heap pointer
  free(stackptr);      // free up the memory again (sets stackptr to 0)
  stackptr =  (uint8_t *)(SP);           // save value of stack pointer
}

void printMem(){
  check_mem();
  Serial.print(F("H:"));
  Serial.print(*heapptr);
  Serial.print(F(", S(L):"));
  Serial.println(*stackptr);
}

float calculateTemperature(int rawTemp) {
  float temp = ((float)rawTemp / 1023.0) * ((float) reference_voltage / 1000.0);
  temp = ((float) temp_range_min / 10.0) + (temp * (((float)temp_range_max / 10.0) - ((float)temp_range_min / 10.0)));
  return round(temp*10.0) / 10.0;
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

//void setWeMo(bool on) {
//
//  if (on != wemo_on) {
//    wemo_on = on;
//    Process p;
//    p.begin("php-cli");
//    String phpCall = "/mnt/sda1/wemo/wemo_switch_";
//    if (on) {
//      phpCall += "on";
//    } else {
//      phpCall += "off";
//    }
//    phpCall += ".php";
//    p.addParameter(phpCall);
//    p.addParameter(wemo_ip);
//    p.run();
//
//  }
//}

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



float currentTemperature()
{
  int sensorValue = analogRead(A0);
  return sensorValue;
}
