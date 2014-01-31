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
$outlet->setIsOn(false); // Outlet will shut off!
?>