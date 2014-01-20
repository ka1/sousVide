<?php 
include('./init.php');

if ($localTest !== true){
	$db = new SQLite3("/mnt/sda1/temperatures.sqlite");
	$db->busyTimeout(2000);
} else {
	$db = new SQLite3("./temperatures.sqlite"); 
}

$temperatureSettings = $db->querySingle("SELECT * FROM `thermosetup`",true);

?>
<!DOCTYPE html>
<meta charset="utf-8" />
<link href='graph.css' rel='stylesheet' type='text/css' />

<body>
<script>
var thermoRangeMin = <?php echo $temperatureSettings['set_min']; ?>;
var thermoRangeMax = <?php echo $temperatureSettings['set_max']; ?>;
var thermoRefVoltage = <?php echo $temperatureSettings['referenceVoltage']; ?>;
var thermoAlarmMin= <?php echo $temperatureSettings['alarm_min'] ? $temperatureSettings['alarm_min'] : "null"; ?>;
var thermoAlarmMax = <?php echo $temperatureSettings['alarm_max'] ? $temperatureSettings['alarm_max'] : "null"; ?>;
</script>
<script src="http://d3js.org/d3.v3.js"></script>
<script src="graph.js"></script>
<br />
<div><a href="temperatureSetup.php">Hardware settings</a></div>
</body>