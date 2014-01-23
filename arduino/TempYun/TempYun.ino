
/*
  Arduino YUN - WEMO - Sous Vide
  Felix Heidrich and Kai Kasugai
  2014
 */

#include <Bridge.h>
#include <YunServer.h>
#include <YunClient.h>
#include <PID_v1.h>

// Listen on default port 5555, the webserver on the Yun
// will forward there all the HTTP requests for us.
YunServer server;
unsigned long lastTime;
bool is_ready = false;

//define the size of the median buffer
#define RAWDATASIZE 50
#define RAWDATAMEDIANIDX RAWDATASIZE / 2
int rawData[RAWDATASIZE];
byte rawDataIdx = 0;
//byte number = 0; //counter, how many times data is read between the median call

//wemo settings
String wemo_ip;
int wemo_temp_start; //*10
int wemo_temp_end; //*10
bool wemo_on; // current wemo state
Process wemoProcess;
bool wemoProcessStarted = false;

//PID
double Setpoint, Input, Output;
byte WindowSize = 10; //in seconds = *1000
unsigned long windowStartTime;
//unsigned long lastPIDCall;
int pid_kp; //*100
int pid_ki; //*100
int pid_kd; //*100
//byte ATuneModeRemember=2;
//double aTuneStep=500;
//double aTuneNoise=1;
//unsigned int aTuneLookBack=20;
//boolean tuning = false;

//Specify the links and initial tuning parameters
PID myPID(&Input, &Output, &Setpoint,850,0.5,0.1, DIRECT); //2,5,1 or 1,0.05,0.25 (cons)
//PID_ATune aTune(&Input, &Output);

// temperature settings
int temp_range_min; //*10
int temp_range_max; //*10
int reference_voltage; //*1000

/* This function places the current value of the heap and stack pointers in the
 * variables. You can call it from any place in your code and save the data for
 * outputting or displaying later. This allows you to check at different parts of
 * your program flow.
 * The stack pointer starts at the top of RAM and grows downwards. The heap pointer
 * starts just above the static variables etc. and grows upwards. SP should always
 * be larger than HP or you'll be in big trouble! The smaller the gap, the more
 * careful you need to be. Julian Gall 6-Feb-2009.
 */
uint8_t * heapptr, * stackptr;
void check_mem() {
  stackptr = (uint8_t *)malloc(4);          // use stackptr temporarily
  heapptr = stackptr;                     // save value of heap pointer
  free(stackptr);      // free up the memory again (sets stackptr to 0)
  stackptr =  (uint8_t *)(SP);           // save value of stack pointer
}

void printMem(){
  check_mem();
  Serial.print(F("H:"));
  Serial.println(*heapptr);
  Serial.print(F("S(L):"));
  Serial.println(*stackptr);
}

void setup() {
  //EXTERNAL AREF Voltage should be 1.134V
  analogReference(EXTERNAL);

  // Bridge startup
  pinMode(13, OUTPUT);
  digitalWrite(13, LOW);
  Bridge.begin();
  digitalWrite(13, HIGH);

  // Listen for incoming connection only from localhost
  // (no one from the external network could connect)
  server.listenOnLocalhost();
  server.begin();

  refreshConfiguration();
//  setWeMo(false);

  for (int i = 0; i < RAWDATASIZE; i++) {
    rawData[i] = 0;
  }

  lastTime = millis();
  
  //PID
  windowStartTime = millis();
  //initialize the variables we're linked to
  Setpoint = (float)wemo_temp_end / 10.0;
  //tell the PID to range between 0 and the full window size
  myPID.SetOutputLimits(0, (WindowSize * 1000) - 1000); //- 1000 for a bit of time for the php process to return value
  //set sample time
  myPID.SetSampleTime(1000);
  //turn the PID on
  myPID.SetMode(AUTOMATIC);
  
}

int getIntValue(String data, char separator, int index){
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

void refreshConfiguration() {
  Serial.println(F("STARTING REFRESH"));
  checkIfProcessRunning();


  String configQuery = "SELECT set_min * 10,set_max * 10, referenceVoltage * 1000, alarm_min * 10, alarm_max * 10, wemo_ip, wemo_temp_min * 10, wemo_temp_max * 10, pid_kp * 100, pid_ki * 100, pid_kd * 100 FROM `thermosetup`;";

  Process wemoProcess;

  wemoProcess.begin("sqlite3");
  wemoProcess.addParameter("/mnt/sda1/temperatures.sqlite");
  wemoProcess.addParameter(configQuery);
  wemoProcess.run();

  //check output
  String str = "";
  while (wemoProcess.available() > 0) {
    char c = wemoProcess.read();
    str += c;
    Serial.print(c);
  }
  
  Serial.println(F("READ"));

  //trim last linebreak ("0.0|100.0|1.134|||192.168.4.39|45.0|50.0")
  str = str.substring(0, (str.length() - 1));

  // read temperature settings
  reference_voltage = getIntValue(str, '|', 2);
  //read temp values
  temp_range_min = getIntValue(str, '|', 0);
  temp_range_max = getIntValue(str, '|', 1);
  //read WeMo values
  wemo_ip = getValue(str, '|', 5);
  wemo_temp_start = getIntValue(str, '|', 6);
  wemo_temp_end = getIntValue(str, '|', 7);
  //read PID values
  pid_kp = getIntValue(str, '|', 8);
  pid_ki = getIntValue(str, '|', 9);
  pid_kd = getIntValue(str, '|', 10);

  Serial.println(F("REFR"));
//  Serial.println(wemo_temp_end);
//  Serial.println(wemo_temp_start);
  Serial.println(wemo_ip);
//  Serial.println(reference_voltage);
//  Serial.println(temp_range_min);
//  Serial.println(temp_range_max);
//  Serial.println(pid_kp);
//  Serial.println(pid_ki);
//  Serial.println(pid_kd);

  Setpoint = (float)wemo_temp_end / 10.0;
  myPID.SetTunings((float) pid_kp / 100.0,(float) pid_ki / 100.0,(float) pid_kd / 100.0);
//  setWeMo(false);
}

float calculateTemperature(int rawTemp) {
  float temp = ((float)rawTemp / 1023.0) * ((float) reference_voltage / 1000.0);
  temp = ((float) temp_range_min / 10.0) + (temp * (((float)temp_range_max / 10.0) - ((float)temp_range_min / 10.0)));
  return temp;
}

void loop() {
  //turn off pin 13
  digitalWrite(13, LOW);

  // Get clients coming from server
  YunClient client = server.accept();

  // There is a new client?
  if (client) {
    // Process request
    doProcess(client);

    // Close connection and free resources.
    client.stop();
  }

  //save last value
  rawData[rawDataIdx] = currentTemperature();
  rawDataIdx++;
  rawDataIdx %= RAWDATASIZE;
//  number++;

//  Process p;

  //if not yet ready (the time might not have been updated yet)
  if (!is_ready) {
    //check every 5 seconds if ready
    if (millis() - lastTime > 5000) {
      Serial.println(F("before"));
      printMem();
//      Process p;
      
      //See if there is no entry yet, or if the last entry is younger than the current time. if the current time is younger than the last entry, we can not have the write time yet.
//      String timecheckQuery = "SELECT (SELECT SEQ FROM SQLITE_SEQUENCE WHERE NAME='temperatures') IS NULL";
//      timecheckQuery += " OR (SELECT STRFTIME('%s','NOW') > (SELECT timestamp FROM temperatures WHERE temp_ID = (SELECT SEQ FROM SQLITE_SEQUENCE WHERE NAME='temperatures')));";

      wemoProcess.begin(F("sqlite3"));
      wemoProcess.addParameter(F("/mnt/sda1/temperatures.sqlite"));
//      wemoProcess.addParameter(F("SELECT '1'"));
      wemoProcess.addParameter(F("SELECT (SELECT SEQ FROM SQLITE_SEQUENCE WHERE NAME='temperatures') IS NULL OR (SELECT STRFTIME('%s','NOW') > (SELECT timestamp FROM temperatures WHERE temp_ID = (SELECT SEQ FROM SQLITE_SEQUENCE WHERE NAME='temperatures')));"));
      wemoProcess.run();

//      Serial.println(timecheckQuery);

      //check output
      int maxI = 0;
      String answer = "";
      while (wemoProcess.available() > 0) {
        char c = wemoProcess.read();
        answer += c;
        if (maxI > 20){
          Serial.println("ABORT");
          break;
        }
      }

//      Serial.println(answer);

      if (answer == "1\n") {
        is_ready = true;
      }

      
      Serial.println(F("not ready"));
      printMem();

      lastTime = millis();
    }
  }
//  //run every second, after ready
//  else if (is_ready && millis() - lastTime > 1000) {
//    //LED ON
//    digitalWrite(13, HIGH);
//
//    String sql = "INSERT INTO temperatures (temperature) VALUES (";
//    sql += median(rawData); //calculate median from last N read values
//    sql += ");";
//
////    Serial.println(sql);
//    
//    Process p;
//    p.begin("sqlite3");
//
//    // the database file name
//    // requires a microSD card with accessible filesystem
//    p.addParameter("/mnt/sda1/temperatures.sqlite");
//
//    // the query to run
//    p.addParameter(sql);
//    p.run();
//
//    //safe last time and switch of LED
//    lastTime = millis();
//
//    //LED OFF
//    digitalWrite(13, LOW);
//
//
////    number = 0;
//  }

  if (is_ready) {
    //PID
    Input = calculateTemperature(median(rawData));
    myPID.Compute();
  
    /************************************************
     * turn the output pin on/off based on pid output
     ************************************************/
    if(millis() - windowStartTime > (WindowSize * 1000))
    { //time to shift the Relay Window
      windowStartTime += (WindowSize * 1000);
      
      checkIfProcessRunning();
      
      //write temperature to db
      digitalWrite(13, HIGH);
      String sql = "INSERT INTO temperatures (temperature) VALUES (";
      sql += median(rawData); //calculate median from last N read values
      sql += ");";
      wemoProcess.begin("sqlite3");
      wemoProcess.addParameter("/mnt/sda1/temperatures.sqlite");
      wemoProcess.addParameter(sql);
      wemoProcess.run();
      digitalWrite(13, LOW);

      
      
      
      if (Output > ((WindowSize * 1000) - 1000)) {
        Serial.println(F("PID LIB ERR")); // sanity check
      }
      
      if (Output > 50) {
//        Serial.print(F("ON OFF Output (MS) - "));
//        Serial.println(Output);
//        Serial.print(F("Input - "));
//        Serial.println(Input);    
      
        wemoProcess.begin("php-cli");
        wemoProcess.addParameter(F("/mnt/sda1/wemo/wemo_switch_onOffTimed.php"));
        wemoProcess.addParameter(wemo_ip);
        wemoProcess.addParameter((String) Output);
        wemoProcess.runAsynchronously();
        wemoProcessStarted = true;
      }
    }
  }
  delay(6); // Poll every x ms
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

void doProcess(YunClient client) {
  // read the command
  String command = client.readStringUntil('/');

  if (command == "temp") {
    tempCommand(client);
  }
  else if (command == "refreshConfiguration") {
    refreshConfiguration();
  }
}


float currentTemperature()
{
  int sensorValue = analogRead(A0);
  return sensorValue;
}


void tempCommand(YunClient client) {

  // Read analog pin
  int sensorValue = median(rawData);

  client.println(sensorValue);

  // Update datastore key with the current pin value
  String key = "A";
  key += A0;
  Bridge.put(key, String((int)sensorValue));
}
