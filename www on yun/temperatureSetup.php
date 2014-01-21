<?php
include('./init.php');
include('./inc_debugging.php');
$timer = new splittime();

if ($localTest !== true){
	$db = new SQLite3("/mnt/sda1/temperatures.sqlite");
	$db->busyTimeout(2000);
} else {
	$db = new SQLite3("./temperatures.sqlite"); 
}

$timer->addtime('after db init');

$temperatureSettings = $db->querySingle("SELECT * FROM `thermosetup`",true);
$tempRangeStart = $temperatureSettings['set_min'];
$tempRangeEnd = $temperatureSettings['set_max'];
$tempRefVoltage = $temperatureSettings['referenceVoltage'];

//wemo settings
$wemoIp = $temperatureSettings['wemo_ip']; //ip adress of wemo
$wemoTempMin = $temperatureSettings['wemo_temp_min']; //temperature at which wemo will start
$wemoTempMax = $temperatureSettings['wemo_temp_max']; //temperature at which wemo will stop

//PID settings
$pid_kp = $temperatureSettings['pid_kp']; //P of PID
$pid_ki = $temperatureSettings['pid_ki']; //I of PID
$pid_kd = $temperatureSettings['pid_kd']; //D of PID

$timer->addtime('read database');


if (isset($_GET['tempRangeStart']) && isset($_GET['tempRangeEnd']) && isset($_GET['refVolt'])){
	//read GET
	$newTempRangeStart = $_GET['tempRangeStart'];
	$newTempRangeEnd = $_GET['tempRangeEnd'];
	$newRefVoltage = $_GET['refVolt'];
	$newWemoIp = $_GET['wemoIp'];
	$newWemoTempMin = $_GET['wemoTempMin'];
	$newWemoTempMax = $_GET['wemoTempMax'];
	$newPid_kp = $_GET['pid_kp'];
	$newPid_ki = $_GET['pid_ki'];
	$newPid_kd = $_GET['pid_kd'];
	
	settype($newTempRangeStart,'integer');
	settype($newTempRangeEnd,'integer');
	settype($newRefVoltage,'float');
	settype($newWemoTempMin,'float');
	settype($newWemoTempMax,'float');
	settype($newPid_kp,'float');
	settype($newPid_ki,'float');
	settype($newPid_kd,'float');
	
	if ($newRefVoltage == 0){
		$errorMessage = "voltage must not be 0";
	} else if ($newRefVoltage > 5){
		$errorMessage = "voltage cannot be above 5V";
	} else if ($newTempRangeStart == 0 && $newTempRangeEnd == 0){
		$errorMessage = "both must not be 0 at the same time";
	} else if ($newTempRangeStart < -220 || $newTempRangeStart > 1372){
		$errorMessage = "start must be within range";
	} else if ($newTempRangeEnd < -220 || $newTempRangeEnd > 1372){
		$errorMessage = "end must be within range";
//	} else if (filter_var($newWemoIp,FILTER_VALIDATE_IP) == false){
//		$errorMessage = "validate ip adress";
	} else if ($newWemoTempMin >= $newWemoTempMax) {
		$errorMessage = "end must be larger than start";
	} else if ($newWemoTempMin < 0 || $newWemoTempMin > 100){
		$errorMessage = "end must be within range";
	} else if ($newTempRangeEnd < 0 || $newTempRangeEnd > 100){
		$errorMessage = "end must be within range";
	} else if ($newPid_kp < 0 || $newPid_kp > 100){
		$errorMessage = "PID-kp must be within range";
	} else if ($newPid_ki < 0 || $newPid_ki > 100){
		$errorMessage = "PID-ki must be within range";
	} else if ($newPid_kd < 0 || $newPid_kd > 100){
		$errorMessage = "PID-kd must be within range";
	}
	
	$timer->addtime('validation');
	
	if ($errorMessage){
		echo "Error in values. Nothing was saved";
	} else {
		//Go. But first check, if anything was changed!
		if ($tempRangeStart == $newTempRangeStart
				&& $tempRangeEnd == $newTempRangeEnd
				&& $tempRefVoltage == $newRefVoltage
				&& $wemoIp == $newWemoIp
				&& $wemoTempMin == $newWemoTempMin
				&& $wemoTempMax == $newWemoTempMax
				&& $pid_kp == $newPid_kp
				&& $pid_ki == $newPid_ki
				&& $pid_kd == $newPid_kd){
			$notice = "Same values. Nothing was saved";
		} else {
			$timer->addtime('before saving');
			$query = array();
			$query[] = "BEGIN";
			$query[] = "UPDATE thermosetup SET
				set_min = $newTempRangeStart,
				set_max = $newTempRangeEnd,
				referenceVoltage = $newRefVoltage,
				wemo_ip = '$newWemoIp',
				wemo_temp_min = $newWemoTempMin,
				wemo_temp_max = $newWemoTempMax,
				pid_kp = $newPid_kp,
				pid_ki = $newPid_ki,
				pid_kd = $newPid_kd";
			//clear graph only if thermometer settings where changed
			if ($tempRangeStart != $newTempRangeStart || $tempRangeEnd != $newTempRangeEnd || $tempRefVoltage != $newRefVoltage	){
				$query[] = "DELETE FROM temperatures";
				$query[] = "DELETE FROM SQLITE_SEQUENCE WHERE name = 'temperatures'";
				$notice .= "Database cleared";
			}
			$query[] = "COMMIT TRANSACTION ";
			//$query[] = "VACUUM";
			
			foreach($query AS $q){
				if (!$db->query($q)){
					echo "<div style='color:red'>Error in query:$q</div>";
				}
				$timer->addtime("done: " . $q);
			}
			$notice .= "Values saved";
			$timer->addtime('after saving');
			
			//save for form
			$tempRangeStart = $newTempRangeStart;
			$tempRangeEnd = $newTempRangeEnd;
			$tempRefVoltage = $newRefVoltage;
			$wemoIp = $newWemoIp;
			$wemoTempMin = $newWemoTempMin;
			$wemoTempMax = $newWemoTempMax;
			$pid_kp = $newPid_kp;
			$pid_ki = $newPid_ki;
			$pid_kd = $newPid_kd;

			if (!$localTest){
				$url = $url_arduino . "arduino/refreshConfiguration/1";
				$result = file_get_contents($url);
				$notice .= "<br />" . "Configuration refreshed";
			}
			
		}
	}
}

$timer->addtime('end of php block');

?>

<!DOCTYPE html>
<meta charset="utf-8" />

<body>

<div><?php echo $errorMessage; ?></div>
<div><?php echo $notice; ?></div>

<form method="get" id="tempSettings" >
<p><label>Range start °C </label><input type="number" value="<?php echo $tempRangeStart; ?>" name="tempRangeStart" min="-220" max="1372" step="1" required /></p>
<p><label>Range end °C </label><input type="number" value="<?php echo $tempRangeEnd; ?>" name="tempRangeEnd" min="-220" max="1372" step="1" required /></p>
<p><label>Reference Voltage (default 1.134) </label><input type="number" value="<?php echo $tempRefVoltage; ?>" name="refVolt" min="0" max="5" step=".0001" required /></p>
<p><label>Wemo IP</label><input input type="text" value="<?php echo $wemoIp; ?>" name="wemoIp" pattern="\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}" /></p>
<p><label>Wemo start °C </label><input type="number" value="<?php echo $wemoTempMin; ?>" name="wemoTempMin" min="0" max="100" step="0.01" required /></p>
<p><label>Wemo end °C </label><input type="number" value="<?php echo $wemoTempMax; ?>" name="wemoTempMax" min="0" max="100" step="0.01" required /></p>
<p><label>PID kp </label><input type="number" value="<?php echo $pid_kp; ?>" name="pid_kp" min="0" max="1000" step="0.01" /></p>
<p><label>PID ki </label><input type="number" value="<?php echo $pid_ki; ?>" name="pid_ki" min="0" max="1000" step="0.01" /></p>
<p><label>PID kd </label><input type="number" value="<?php echo $pid_kd; ?>" name="pid_kd" min="0" max="1000" step="0.01" /></p>

<button>Save settings and reset graph</button>
</form>
<br/>
<a href="graph.php">discard changes and go back to graph</a>
<div><?php $timer->stoptimer(); ?></div>
</body>