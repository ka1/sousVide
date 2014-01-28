void refreshConfiguration() {
  Serial.println(F("STARTING REFRESH"));
//  checkIfProcessRunning();

  Process p;

  p.begin(F("php-cli"));
  p.addParameter(F("/mnt/sda1/clientPhp/getSettings.php"));
  p.run();

  //check output
  String str = "";
  while (p.available() > 0) {
    char c = p.read();
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
  pid_kp = getLongValue(str, '|', 8);
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

void tempCommand(YunClient client) {

  // Read analog pin
  int sensorValue = median(rawData);

  client.println(sensorValue);

  // Update datastore key with the current pin value
  //TODO: OK to comment:
  String key = "A";
  key += A0;
  Bridge.put(key, String((int)sensorValue));
}
