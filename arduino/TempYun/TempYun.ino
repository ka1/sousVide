
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

//check_mem vars
uint8_t * heapptr, * stackptr;

void setup() {
  //EXTERNAL AREF Voltage should be 1.134V
  analogReference(EXTERNAL);

  // Bridge startup
  pinMode(13, OUTPUT);
  digitalWrite(13, LOW);
  Bridge.begin();
  digitalWrite(13, HIGH);

  // Listen for incoming connection only from localhost
  server.listenOnLocalhost();
  server.begin();

  //load settings from database
  refreshConfiguration();

  //fill data array with zeros
  for (int i = 0; i < RAWDATASIZE; i++) {
    rawData[i] = 0;
  }

  //init time counter
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

  //if not yet ready (the time might not have been updated yet)
  if (!is_ready) {
    //check every 5 seconds if ready
    if (millis() - lastTime > 5000) {
      Serial.println(F("before"));
      printMem();
      
      wemoProcess.begin(F("sqlite3"));
      wemoProcess.addParameter(F("/mnt/sda1/temperatures.sqlite"));
      wemoProcess.addParameter(F("SELECT (SELECT SEQ FROM SQLITE_SEQUENCE WHERE NAME='temperatures') IS NULL OR (SELECT STRFTIME('%s','NOW') > (SELECT timestamp FROM temperatures WHERE temp_ID = (SELECT SEQ FROM SQLITE_SEQUENCE WHERE NAME='temperatures')));"));
      wemoProcess.run();

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
//  }

  if (is_ready) {
    //PID
    Input = calculateTemperature(median(rawData));
    myPID.Compute();
    
    //see if the window has passed
    if(millis() - windowStartTime > (WindowSize * 1000))
    { //time to shift the Relay Window
      windowStartTime += (WindowSize * 1000);
      
      //see if process is still running and handle any response
      checkIfProcessRunning();
      
      //write temperature to db
      //TODO: do this every second, again
      digitalWrite(13, HIGH);
      String sqlValue = (String) median(rawData); //calculate median from last N read values
      wemoProcess.begin(F("sqlite3"));
      wemoProcess.addParameter(F("/mnt/sda1/temperatures.sqlite"));
      wemoProcess.addParameter("INSERT INTO temperatures (temperature) VALUES (" + sqlValue + ");");
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
      
        wemoProcess.begin(F("php-cli"));
        wemoProcess.addParameter(F("/mnt/sda1/wemo/wemoTimed.php"));
        wemoProcess.addParameter(wemo_ip);
        wemoProcess.addParameter((String) Output);
        wemoProcess.runAsynchronously();
        wemoProcessStarted = true;
      }
    }
  }
  delay(6); // Poll every x ms
}







