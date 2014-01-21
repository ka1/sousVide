
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
#define RAWDATASIZE 100
#define RAWDATAMEDIANIDX RAWDATASIZE / 2
int rawData[RAWDATASIZE];
int rawDataIdx = 0;
int number = 0; //counter, how many times data is read between the median call

//wemo settings
String wemo_ip;
double wemo_temp_start;
double wemo_temp_end;
bool wemo_on; // current wemo state
unsigned long lastWeMoOnMillis; // last millis wemo was turned on
Process wemoProcess;
bool wemoProcessStarted = false;

//PID
double temp_double;
double Setpoint, Input, Output;
int WindowSize = 10000;
unsigned long windowStartTime;
//unsigned long lastPIDCall;
double pid_kp;
double pid_ki;
double pid_kd;
//byte ATuneModeRemember=2;
//double aTuneStep=500;
//double aTuneNoise=1;
//unsigned int aTuneLookBack=20;
//boolean tuning = false;

//Specify the links and initial tuning parameters
PID myPID(&Input, &Output, &Setpoint,850,0.5,0.1, DIRECT); //2,5,1 or 1,0.05,0.25 (cons)
//PID_ATune aTune(&Input, &Output);

// temperature settings
float temp_range_min;
float temp_range_max;
float reference_voltage;

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
  setWeMo(false);

  for (int i = 0; i < RAWDATASIZE; i++) {
    rawData[i] = 0;
  }

  lastTime = millis();
  lastWeMoOnMillis = millis();
  
  //PID
  windowStartTime = millis();
  //initialize the variables we're linked to
  Setpoint = wemo_temp_end;
  //tell the PID to range between 0 and the full window size
  myPID.SetOutputLimits(0, WindowSize - 1000); //- 1000 for a bit of time for the php process to return value
  //set sample time
  myPID.SetSampleTime(1000);
  //turn the PID on
  myPID.SetMode(AUTOMATIC);
  
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

void refreshConfiguration() {
  Process p;

  String configQuery = "SELECT * FROM `thermosetup`;";

  p.begin("sqlite3");
  p.addParameter("/mnt/sda1/temperatures.sqlite");
  p.addParameter(configQuery);
  p.run();

  //check output
  String str = "";
  while (p.available() > 0) {
    char c = p.read();
    str += c;
  }

  //trim last linebreak ("0.0|100.0|1.134|||192.168.4.39|45.0|50.0")
  str = str.substring(0, (str.length() - 1));

  // read temperature settings
  String reference_voltage_str = getValue(str, '|', 2);
  String temp_range_min_str = getValue(str, '|', 0);
  String temp_range_max_str = getValue(str, '|', 1);

  // parse temperature settings
  char floatbuf[32]; // make this at least big enough for the whole string
  reference_voltage_str.toCharArray(floatbuf, sizeof(floatbuf));
  reference_voltage = atof(floatbuf);
  temp_range_min_str.toCharArray(floatbuf, sizeof(floatbuf));
  temp_range_min = atof(floatbuf);
  temp_range_max_str.toCharArray(floatbuf, sizeof(floatbuf));
  temp_range_max = atof(floatbuf);

  //read WeMo values
  wemo_ip = getValue(str, '|', 5);
  String wemo_temp_start_str = getValue(str, '|', 6);
  String wemo_temp_end_str = getValue(str, '|', 7);

  //parse WeMo start temperature
  wemo_temp_start_str.toCharArray(floatbuf, sizeof(floatbuf));
  wemo_temp_start = atof(floatbuf);
  //parse WeMo end temperature
  wemo_temp_end_str.toCharArray(floatbuf, sizeof(floatbuf));
  wemo_temp_end = atof(floatbuf);
  
  //read PID values
  String pid_kp_str = getValue(str, '|', 8);
  String pid_ki_str = getValue(str, '|', 9);
  String pid_kd_str = getValue(str, '|', 10);
  pid_kp_str.toCharArray(floatbuf, sizeof(floatbuf));
  pid_kp = atof(floatbuf);
  pid_ki_str.toCharArray(floatbuf, sizeof(floatbuf));
  pid_ki = atof(floatbuf);
  pid_kd_str.toCharArray(floatbuf, sizeof(floatbuf));
  pid_kd = atof(floatbuf);


  Serial.println("refreshed");
  Serial.println(wemo_temp_end);
  Serial.println(wemo_temp_start);
  Serial.println(wemo_ip);
  Serial.println(reference_voltage);
  Serial.println(temp_range_min);
  Serial.println(temp_range_max);
  Serial.println(pid_kp);
  Serial.println(pid_ki);
  Serial.println(pid_kd);

  Setpoint = wemo_temp_end;
  myPID.SetTunings(pid_kp,pid_ki,pid_kd);
  setWeMo(false);
}

float calculateTemperature(int rawTemp) {

  float raw = rawTemp;
  float temp = rawTemp / 1023.0 * reference_voltage;
  temp = temp_range_min + (temp * (temp_range_max - temp_range_min));
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
  number++;

//  Process p;

  //if not yet ready (the time might not have been updated yet)
  if (!is_ready) {
    //check every 5 seconds if ready
    if (millis() - lastTime > 5000) {
      Process p;
      String answer = "";
      //See if there is no entry yet, or if the last entry is younger than the current time. if the current time is younger than the last entry, we can not have the write time yet.
      String timecheckQuery = "SELECT (SELECT SEQ FROM SQLITE_SEQUENCE WHERE NAME='temperatures') IS NULL";
      timecheckQuery += " OR (SELECT STRFTIME('%s','NOW') > (SELECT timestamp FROM temperatures WHERE temp_ID = (SELECT SEQ FROM SQLITE_SEQUENCE WHERE NAME='temperatures')));";

      p.begin("sqlite3");
      p.addParameter("/mnt/sda1/temperatures.sqlite");
      p.addParameter(timecheckQuery);
      p.run();

      Serial.println(timecheckQuery);

      //check output
      while (p.available() > 0) {
        char c = p.read();
        answer += c;
      }

      Serial.println(answer);

      if (answer == "1\n") {
        is_ready = true;
      }
      
      Serial.println("not ready");

      lastTime = millis();
    }
  }
  //run every second, after ready
  else if (is_ready && millis() - lastTime > 1000) {
    //LED ON
    digitalWrite(13, HIGH);

    String sql = "INSERT INTO temperatures (temperature) VALUES (";
    sql += median(rawData); //calculate median from last N read values
    sql += ");";

    Serial.println(sql);
    
    Process p;
    p.begin("sqlite3");

    // the database file name
    // requires a microSD card with accessible filesystem
    p.addParameter("/mnt/sda1/temperatures.sqlite");

    // the query to run
    p.addParameter(sql);
    p.run();

    //safe last time and switch of LED
    lastTime = millis();

    //LED OFF
    digitalWrite(13, LOW);


    number = 0;
  }

  if (is_ready) {
    //PID
    temp_double = calculateTemperature(median(rawData));
    Input = temp_double;
    myPID.Compute();
  
    /************************************************
     * turn the output pin on/off based on pid output
     ************************************************/
    if(millis() - windowStartTime > WindowSize)
    { //time to shift the Relay Window
      windowStartTime += WindowSize;
      
      if(wemoProcessStarted && (wemoProcess.running() || wemoProcess.available())) {
        Serial.println("WEMO ERROR. STILL RUNNING AFTER WINDOW");
//        wemoProcess.close();
      }
      
      if (Output > (WindowSize - 1000)) {
        Serial.println("PID LIBRARY ERROR"); // sanity check
      }
      
      if (Output > 50) {
              
        Serial.print("ON OFF Output (MS) - ");
        Serial.println(Output);
        Serial.print("Input - ");
        Serial.println(Input);    
      
        wemoProcess.begin("php-cli");
        String phpCall = "/mnt/sda1/wemo/wemo_switch_onOffTimed.php";
        wemoProcess.addParameter(phpCall);
        wemoProcess.addParameter(wemo_ip);
        String strOutput = String(Output);
        wemoProcess.addParameter(strOutput);
        wemoProcess.runAsynchronously();
        wemoProcessStarted = true;
      }
    }
  }

  delay(6); // Poll every 50ms
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

void setWeMo(bool on) {

  if (on != wemo_on) {
    wemo_on = on;
    Process p;
    p.begin("php-cli");
    String phpCall = "/mnt/sda1/wemo/wemo_switch_";
    if (on) {
      phpCall += "on";
    } else {
      phpCall += "off";
    }
    phpCall += ".php";
    p.addParameter(phpCall);
    p.addParameter(wemo_ip);
    p.run();

  }
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
