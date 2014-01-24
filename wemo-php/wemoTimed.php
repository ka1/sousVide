<?php
$localTest = false;

if ($localTest){
	include('./models/Device.php');
	include('./models/Outlet.php');
} else {
	include('/mnt/sda1/wemo/models/Device.php');
	include('/mnt/sda1/wemo/models/Outlet.php');
}

$outlet = new \wemo\models\Outlet($argv[1]);

//get millis
$timeMs = $argv[2];
settype($timeMs,'integer');
//calculate seconds
$timeSeconds = floor($timeMs / 1000);
$timeMs -= $timeSeconds * 1000;
//calculate nanoseconds 
$timeNano = $timeMs * 1000000;

$outlet->setIsOn(true); // Outlet will be switched on!
$nano = time_nanosleep($timeSeconds, $timeNano);
$outlet->setIsOn(false); // Outlet will shut off!
?>