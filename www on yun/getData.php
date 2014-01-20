<?php
include('./init.php');

//-----------------------------------------
//Get the (entire) graph data from database
//-----------------------------------------

$getTotal = 500; //total number of values to receive. this will affect the resolution of the graph in x axis

if ($localTest !== true){
	$db = new SQLite3("/mnt/sda1/temperatures.sqlite");
	$db->busyTimeout(2000);
} else {
	$db = new SQLite3("./temperatures.sqlite"); 
}

echo "Ident\tZeitpunkt\tTemperatur\n";

$rowCount = $db->querySingle("SELECT COUNT(*) FROM `temperatures`");
$get_every_n = round($rowCount / $getTotal);
if ($get_every_n < 1) {
	$get_every_n = 1;
}

//select subquery for every nth row
$results = $db->query("SELECT temperature,datetime(timestamp,'unixepoch','localtime') AS timestamp,temp_ID
	FROM temperatures
	WHERE temperature != 0
	AND temperature != 1023
	AND temp_ID %$get_every_n = 0
	AND temp_ID > 60
	ORDER BY temp_ID ASC"); //skip first 60 seconds, because the time might be wrong
while ($row = $results->fetchArray()) {
	echo $row['temp_ID'] . "\t" . $row['timestamp'] . "\t" . $row['temperature'] . "\n";
}

?>