<?php
$localTest = false;

if ($localTest !== true){
	$db = new SQLite3("/mnt/sda1/temperatures.sqlite");
	$db->busyTimeout(2000);
} else {
	$db = new SQLite3("../www on yun/temperatures.sqlite"); 
}

$sql = "SELECT set_min * 10 AS set_min,set_max * 10 AS set_max, referenceVoltage * 1000 AS referenceVoltage, alarm_min * 10 AS alarm_min, alarm_max * 10 AS alarm_max, wemo_ip, wemo_temp_min * 10 AS wemo_temp_min, wemo_temp_max * 10 AS wemo_temp_max, pid_kp * 100 AS pid_kp, pid_ki * 100 AS pid_ki, pid_kd * 100 AS pid_kd FROM `thermosetup`";

$temperatureSettings = $db->querySingle($sql,true);

//$returnValues = array();
//$returnFields = array('set_min','set_max','referenceVoltage','alarm_min','alarm_max','wemo_ip','wemo_temp_min','wemo_temp_max','pid_kp','pid_ki','pid_kd');
//foreach($returnFields AS $returnField){
//	$returnValues[] = $temperatureSettings[$returnField];
//}

echo implode($temperatureSettings,'|');
echo "\n";

?>