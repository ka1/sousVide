<?php
include('./init.php');

//-------------------------------
//Get current datum (1) from database
//-------------------------------

date_default_timezone_set('Europe/Berlin');

if ($localTest !== true){
	$url = $url_arduino . "arduino/temp/1/";
	$result = file_get_contents($url);
} else {
	$db = new SQLite3("./temperatures.sqlite"); 
	$previousResult = $db->querySingle("SELECT temperature FROM `temperatures` ORDER BY timestamp DESC LIMIT 1",false);
	if ($previousResult == null){
		$result = rand(100,150); //fake temperature
	} else {
		$result = $previousResult + rand(-1,1); //fake temperature
	}
	$db->query("INSERT INTO temperatures (temperature) VALUES ($result)");
}

echo "Zeitpunkt\tTemperatur\n";
echo date("Y-m-d H:i:s") . "\t" . $result . "\n";

?>