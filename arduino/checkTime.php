<?php
$localTest = false;

if ($localTest !== true){
	$db = new SQLite3("/mnt/sda1/temperatures.sqlite");
	$db->busyTimeout(2000);
} else {
	$db = new SQLite3("../www on yun/temperatures.sqlite"); 
}

$sql = "SELECT (SELECT SEQ FROM SQLITE_SEQUENCE WHERE NAME='temperatures') IS NULL OR (SELECT STRFTIME('%s','NOW') > (SELECT timestamp FROM temperatures WHERE temp_ID = (SELECT SEQ FROM SQLITE_SEQUENCE WHERE NAME='temperatures')))";
$temperatureSettings = $db->querySingle($sql);
echo $temperatureSettings;
?>