<?php
class errorcollector
{
	var $errors = FALSE;
	var $received_errors = FALSE;
	var $name_of_get = "errors";
	var $error_msgs = array();
	var $error_msgs_js = FALSE;

	/**
	 * Contructor of class
	 *
	 * @param string $name_of_get Defines how the GET Variable will be called ('errors' is default)
	 * @param array $error_msgs Array with Errormessages
	 * @param array $error_msgs_with_jswarning Specifies, whitch warnings come with the message to enable javascript
	 * @return bool TRUE
	 */
	function errorcollector($name_of_get = FALSE,$error_msgs = FALSE,$error_msgs_with_jswarning = FALSE)
	{
		if ($name_of_get) $this->name_of_get = $name_of_get;
		if (isset($_GET[$this->name_of_get])) $geterrors = $_GET[$this->name_of_get];
		if (isset($geterrors)) $this->received_errors = explode(",",$geterrors);

		//Zunaechst die defaults erstellen...
		$this->error_msgs = array(
			0 =>	"successfully",
			'jswarning' =>	"You missed some values in your entry. All data has been reset (sorry, you'll have to retype everything). To avoid this in the future, please activate JavaScript. In most cases, you will then be warned before submitting the form"
		);
		//...dann jeden Wert einzeln durch das (optional) uebergebene Array $error_msgs ueberschreiben
		if (is_array($error_msgs)) foreach ($error_msgs as $key => $value) {
			$this->error_msgs[$key] = $value;
		}

		$this->error_msgs_js = $error_msgs_with_jswarning;

		return TRUE;
	}

	/**
	 * Adds an erroridentifier to the errors variable
	 *
	 * @param integer $errornumber
	 */
	function adderror($errornumber)
	{
		$this->errors .= ($this->errors ? ",$errornumber" : $this->name_of_get . "=$errornumber");
	}

	/**
	 * This method returns TRUE, if any errors have been collected so far.
	 *
	 * @return bool
	 */
	function iserrors()
	{
		if ($this->errors AND $this->errors != $this->name_of_get . "=0") return TRUE;	//0 ist Erfolgsnachricht
		else return FALSE;
	}

	/**
	 * This method returns TRUE, if any errors have been received (via GET)
	 *
	 * @return unknown
	 */
	function goterrors()
	{
		if ($this->received_errors[0] == 0 AND count($this->received_errors) == 1) return FALSE;
		elseif ($this->received_errors) return TRUE;
		else return FALSE;
	}

	/**
	 * Returns TRUE, if one of the errormessages received is bound to the js warning box
	 *
	 * @return bool
	 */
	function show_js_warning()
	{
		if ($this->error_msgs_js){
			foreach($this->received_errors AS $key => $value)
			{
				if (in_array($value,$this->error_msgs_js)) return TRUE;
			}
		}
		return FALSE;
	}

	/**
	 * Returns a string with all errors OR success message. If no errors were received, returns FALSE
	 *
	 * @return string|bool
	 */
	function echoerrors($js_warning = FALSE)
	{
		if (is_array($this->received_errors)){
			foreach ($this->received_errors AS $errornumber)
			{
				if ($errornumber == '0' || $errornumber === 0){
					$return .= "<div class='pos'>" . $this->error_msgs[$errornumber] . "</div>";
				}
				elseif ($this->error_msgs[$errornumber]){
					$return .= "<div class='neg'>" . $this->error_msgs[$errornumber] . "</div>";
				}
				else{
					$return .= "<div class='neg'>Error #" . $errornumber . "</div>";
				}
			}
			if ($js_warning AND $this->goterrors() AND $this->error_msgs['jswarning'] AND $this->show_js_warning()){
				$return .= "<div class='neg' style='border:1px solid black; margin: 5px;'>" . $this->error_msgs['jswarning'] . "</div>";
			}
			return $return;
		}
		else return FALSE;
	}

	/**
	 * Just returns the success Errornumber
	 *
	 * @return unknown
	 */
	function returnsuccessonly()
	{
		return $this->name_of_get . "=" . 0;
	}
}

function drucke2($array,$return = false){
	$echo = "<pre>";
	$echo .= print_r($array,$return);
	$echo .= "</pre>";
	if ($return) return $return;
	else echo $return;
}
//depth wird uebergeben, da druckeobjekt print_k aufruft und print_k wiederum druckeobjekt aufruft, wenn ein objekt ausgegeben werden soll. und druckeobjekt ruft wiederum drucke auf um das objektarray auszugeben. depth muss also durchgeschliffen werden
function drucke ($array,$echomode = true,$html = true, $divclass = false, $depth = 0)
{
	if (!is_array($array)) { echo "kein array"; return FALSE; }
	elseif ($echomode === 'noecho' or $echomode == false)
	{
		if ($html) $return .= "<pre>";
		$return .= print_k($array,$depth,false,($html ? "<br />" : "\n"));
		if ($html) $return .= "</pre>";
		return $return;
	}
	else
	{
		if ($divclass) echo "<div class='$divclass'>";
		echo "<pre>";
		print_r ($array);
		echo "</pre>";
		if ($divclass) echo "</div>";
	}
}

function kanalyse(&$item, $echo = true) {
	$return .= "<div style='border:1px solid black; margin:.3em; padding:.1em; font-size:.8em; text-align:left'>";
	$return .= "<div style='background-color:#f07; color:white; padding:.1em; font-family:courier; margin:.3em;'>";
	
	if ($name_of_var = vname($item)) $return .= '$' . $name_of_var . ' is of type ';
	
	//wenn array
	if (is_object($item)){
		$return .= "OBJECT. CLASS: \"" . get_class($item) . "\"</div>";
		$return .= druckeobjekt($item,false);
	} elseif (is_array($item)){
		$return .= "ARRAY</div>";
		$return .= drucke($item, false);
	} elseif (is_bool($item)){
		$return .= "BOOL</div>";
		$return .= $item === false ? 'set to: FALSE' : ($item === true ? 'set to: TRUE' : 'neither TRUE nor FALSE. run var_dump().');
	} elseif (is_string($item)){
		$return .= "STRING</div>";
		$return .= "set to: \"" . htmlentities($item) . "\"";
	} else {
		if (is_int($item)) $return .= 'INTEGER: ' . $item;
		elseif (is_double($item)) $return .= 'DOUBLE: ' . $item;
		elseif (is_bool($item)) $return .= 'BOOL';
		elseif (is_float($item)) $return .= 'FLOAT: ' . $item;
		elseif (is_null($item)) $return .= 'NULL';
		elseif (is_string($item)) $return .= 'STRING: "' . $item . '"';
		$return .= '<span style="font-size:.8em;">. get_type() returns: ' . gettype($item) . "</span></div>";
	}
	
	
	$return .= "</div>";
	if ($echo){
		echo $return;
		return null;
	}
	else {
		return $return;
	}
}

/**
 * Naeheres zur Funktion kann unter http://us2.php.net/manual/en/language.variables.php#49997 gefunden werden
 */
function vname(&$var, $scope=false, $prefix='unique', $suffix='value')
{
	if($scope)	$vals = $scope;
	else		$vals = $GLOBALS;
	$old = $var;
	$var = $new = $prefix.rand().$suffix;
	$vname = FALSE;
	foreach($vals as $key => $val) {
		if($val === $new) $vname = $key;
	}
	$var = $old;
	return $vname;
}

function print_k ($array,$depth = 0,$showdepth = FALSE,$break = "<br />")
{
	if (!is_array($array)) return FALSE;
	if ($showdepth) $viewdepth = "$depth: ";
	foreach ($array AS $key => $value)
	{
		while ($temp < $depth)
		{
			$division .= "        ";
			$temp++;
		}
		if (is_object($value)) {
			$value_string = get_class($value) . ' Object';
		}
		else $value_string = $value;
		if (is_string($value_string)) $value_string = htmlentities($value_string);
		$return .= "$viewdepth $division [$key] => $value_string" . $break;
		if (is_array($value)){
			$return .= $viewdepth . $division . "     {\n";
			$return .= print_k($value,$depth+1,$showdepth);
			$return .= $viewdepth . $division . "     }\n";
		} elseif (is_object($value)){
			$return .= $viewdepth . $division . "      (\n";
			$return .= druckeobjekt($value, false, $depth+1);
			$return .= $viewdepth . $division . "      )\n";
		}
	}
	return $return;
}

//depth wird durchgeschliffen
function druckeobjekt ($objekt, $echo = true, $depth = 0)
{
	if (!is_object($objekt)) return FALSE;
	else
	{
		$array = get_object_vars($objekt);
		if ($echo === 'noecho' | $echo == false) return drucke($array, false, true, false, $depth);
		else drucke($array);
	}
}

/**
* Funktion schickt eine Benachrichtigungsemail an den Administrator
* @param $message string Nachricht
* @param $subject string Betreff
* @param $admin_email string eMail Adresse(n)
* @param $scope string Welches System schickt diese Mail (BSP: dencity.net)
*/
function admin_notify_email ($message,$subject,$admin_email,$scope = 'STD',$error = FALSE)
{
	$ip = getenv('REMOTE_ADDR');
	$host = gethostbyaddr($ip);
	$message = eregi_replace("(cc *:)","XFRAUDX",$message);
    //email vorbereiten:
    $header= "From:PHP Event $error Notify <antispam@kasugai.de>\n";
    #$header.="Reply-To: $mailsender\n";
    #$header.="Bcc: pwdreq-bcc@kasugai.de\n";
    $header.="X-Mailer: PHP/" . phpversion() . "\n";
    $header.="X-Sender-IP: $ip\n";
    $mailbetreff="PHP $error Notify ($scope): $subject";
    $mailto="$admin_email";
    $mailbody="$message
__________________________________________________________________________
security log:
IP: $ip
HOST: $host";
    //email senden
    $mailed = mail($mailto,$mailbetreff,$mailbody,$header);
    return $mailed;
}

/**
 * Generiert die Errorseite mit dem angegebenen Errorcode und gibt die Seite als String zurueck.
 *
 * @param integer $errornumber
 * @return string
 */
function generate_errorpage($errornumber,$basedir = 'webni')
{
	global $global_userenv_islocal, $user_ID, $login, $global_mymail;

	settype($errornumber,'integer');
	switch ($errornumber){
		CASE 404:	$errortext = "Sorry, the requested URL <b>{{url}}</b> was not found";
					break;
		CASE 403:	$errortext = "You don't have permission to access the requested directory. There is either no index document or the directory is read-protected.";
					$bigcomment = "access forbidden";
					break;
		default:	$errortext = "Error while requesting URL <b>{{url}}</b>";
					break;
	}

	$ip = getenv ("REMOTE_ADDR");
	$requri = getenv ("REQUEST_URI");
	$servname = getenv ("SERVER_NAME");
	$httpref = getenv ("HTTP_REFERER");
	$browser = getenv("HTTP_USER_AGENT");

	$page = "	<div style='background-color:#f30; color: white; font-weight: bold; font-size:30pt; padding-left: 10px; line-height:25pt; letter-spacing: -4px; font-family: helvetica,verdana,arial'>ERROR $errornumber " . ($bigcomment ? " - " . $bigcomment : "") . "</div>
				<div style='font-weight:normal; font-size:13pt; padding-left: 10px; line-height:20pt; letter-spacing: -1px; font-family: helvetica,verdana,arial'>";
	$page .= preg_replace('/{{url}}/', $servname . $requri,$errortext) . "<br />";
	if ($httpref) $page .= "The linking page was: <i>$httpref</i><br />";
	$page .= "<a href='/$basedir/'>back to start</a>";


	$emailtext = "	An Error Nr. $errornumber occured.
	The requested URL was " . $servname . $requri . ".
	IP: $ip 
	Browser: " . ($browser ? $browser : "no information") .
	($httpref ? "\nThe user came from (referer:) $httpref." : "") .
	($user_ID ? "\nThe user was logged in with the ID $user_ID ($login)\n" : "");


	if (!$global_userenv_islocal)
	admin_notify_email($emailtext,"error 404",$global_mymail,$basedir,'Error');
	else $page .= "<div style='border:1px solid black; margin:20px;'>Running local - no eMail was sent to admin</div>";

	$page .= "</div>";
	return $page;
}

//in myreiff gibt es ebenfalls bereits die klasse
if (!class_exists('splittime'))
{
	class splittime
	#Klasse, um Zwischenzeite zu speichern und dann am Ende auszugeben
	#Aufruf durch $timer = new splittime();
	#$timer->addtime('string Beschreibung') Fuegt eine neue Zwischenzeit ein (mit Beschreibung)
	#$timer->stoptimer wird am Ende ausgefuehrt und gibt zu dem Zeitpunkt dann alle Zwischenzeiten und eine Endzeit aus.
	{
		#initiiert die Klasse
		function splittime()
		{
			$this->starttime = $this->timenow();
		}

		#Gibt die aktuelle Zeit aus
		function timenow()
		{
			list($msec, $sec) = explode(' ',microtime());
	   		return(((float)$msec + (float)$sec));
		}

		function addtime($zeiger)
		{
			$this->zwischenzeit[] = array('name' => $zeiger, 'zeit' => $this->timenow());
		}

		function stoptimer($nodetail = FALSE,$return = FALSE)
		{
			$this->addtime('Ende');

			if (!$nodetail)
			{
				$echo .= "<b>Timelog</b>:<br />";
				for($i=0;$i<count($this->zwischenzeit);$i++)
				{
					if ($i==0) $previoustime = $this->starttime;
					else $previoustime = $this->zwischenzeit[($i-1)]['zeit'];
					$diff = ($this->zwischenzeit[$i]['zeit'] - $previoustime);
					$echo .= round(($this->zwischenzeit[$i]['zeit'] - $this->starttime),3) . " : " . $this->zwischenzeit[$i]['name'] . " : <b>" . ($diff>.05 ? "<span class='neg'>" : "") . round($diff,3) . ($diff>.05 ? "</span>" : "") . " </b>s since last<br />";
				}
				
				if ($return) return $echo;
				else echo $echo;
			}
			#else echo @lgc('Seitenaufbau in ','page generated in ') . round(($this->timenow() - $this->starttime),2) . "s";
		}

		function timeuntilnow($precision = 2)
		{
			$timeuntilnow = round(($this->timenow() - $this->starttime),$precision);
			return $timeuntilnow;
		}
	}
}

/**
* Gibt die per Array ohne $ angegebenen Variablen in formatierter Form aus: z.B.:
*$test = 123
*@param array $echovars
*@author Kai Kasugai
*/
function echovars ($echovars)
{
	foreach ($echovars as $var) {
		$wert = $GLOBALS[$var];
		echo "<b>\$$var</b> &nbsp; &nbsp; $wert <br>";
	}
}

/**
 * Schickt eine LOG Datei an den FTP Server (Verbindung muss uebergeben werden)
 *
 * @param resource $ftp_conn FTP Connection
 * @param string $remotedir FTP Verzeichnis
 * @param array $shashinset Array mit Einstellungen
 * @param array $logarray zu schreibendes Array
 * @param string $time Timestamp
 * @param bool $ftp Wenn keine FTP Verbindung besteht (FALSE), dann werden Dateien lokal geschrieben
 * @return bool
 */
function write_shashin_log($ftp_conn,$remotedir,$shashinset,$logarray,$time,$ftp = TRUE)
{
	$tempfile = $shashinset['logfile_tempdir'] . $shashinset['logfile_name'];
	
	$writearray = array('time' => $time, 'log' => $logarray);
	
	//Logfile vorbereiten
	$file_contents = serialize($writearray);
	//Lokal schreiben
	$tempfile_res = fopen($tempfile,'w');
	fwrite($tempfile_res,$file_contents);
	fclose($tempfile_res);
	//Hochladen
	if ($ftp){
		if (ftp_put($ftp_conn,$remotedir . "/" . $shashinset['logfile_name'],$tempfile,FTP_BINARY)){
			unlink($tempfile);
			return true;
		} else {
			unlink($tempfile);
			return false;
		}
	} else {
		if (rename($tempfile,$remotedir . "/" . $shashinset['logfile_name'])) return true;
		else return false;
	}
}


function write_shashin_lock($ftp_conn,$remotedir,$shashinset,$ftp = TRUE){
	$contents = "Wenn diese Datei geloescht wird, faengt der Server wieder an zu laden.\nWenn der Server damit fertig ist, erstellt er diese Datei wieder.";
	//lokal schreiben
	$tempfile = $shashinset['logfile_tempdir'] . $shashinset['lockfile_name'];
	$tempfile_res = fopen($tempfile,'w');
	fwrite($tempfile_res,$contents);
	fclose($tempfile_res);
	//Hochladen
	//FTP
	if ($ftp){
		if (ftp_put($ftp_conn,$remotedir . "/" . $shashinset['lockfile_name'],$tempfile,FTP_BINARY)){
			//temp datei loeschen und TRUE ausgeben bei erfolg
			unlink($tempfile);
			return true;
		} else {
			//sonst temp datei loeschen und FALSE ausgeben
			unlink($tempfile);
			return false;
		}
	} else {
	//NICHT FTP:
		if (rename($tempfile,$remotedir . "/" . $shashinset['lockfile_name'])) return true;
		else return false;
	}
}

/**
 * Durchsucht das uebergenene Verzeichnis nach der Lockfile und gibt TRUE zueruck, wenn sie existiert
 *
 * @param resource $conn_id FTP Verbindung
 * @param string $remotedir Verzeichnis auf dem Server wo gesucht werden soll
 * @param array $shashinset Array mit Einstellungen, hier fuer den Namen der Lockfile notwendig
 * @param bool $ftp Wenn TRUE, wird eine Verbindung zum FTP Server hergestellt
 * @return bool TRUE wenn lockfile vorhanden
 */
function shashin_check_lockfile($conn_id,$remotedir,$shashinset,$ftp = TRUE){
	if ($ftp){
		$dircontents = ftp_nlist($conn_id,$remotedir);
		if (!is_array($dircontents)){
			trigger_error('no directory contents received', E_USER_ERROR);
			return false;
		}
		//Verzeichnis durchgehen
		foreach($dircontents AS $file){
			preg_match('#[^/]*$#',$file,$filename);
			$filename = $filename[0];
			if ($filename == $shashinset['lockfile_name']) return true;
		}
	} else {
		//lokal schauen (wenn kein FTP server)
		if (file_exists($remotedir . '/' . $shashinset['lockfile_name'])) return TRUE;
	}
	return false;
}

function delete_shashin_log($ftp_conn,$remotedir,$shashinset,$ftp = TRUE){
	$logfile = $remotedir . "/" . $shashinset['logfile_name'];
	if (
		($ftp && ftp_delete($ftp_conn,$logfile)) OR
		($ftp === false && unlink($logfile))
	) return TRUE;
	else return FALSE;	
}

/**
 * creates a formatted readable output of the debug backtrace path
 */
function renderDebugBacktrace(){
	$backtrace = debug_backtrace();
	if (is_array($backtrace)){
		$return = "<div style='border:3px solid red;margin:1em;padding:.5em;'>Error Backtrace:";
		array_shift($backtrace);
		foreach($backtrace AS $level){
			$return .= "<div style='border:1px solid black; margin:1em; padding:.5em;'><b>" . $level['file'] . "</b> in line " . $level['line'] . ", calling function <i>" . $level['function'] . "</i></div>";
		}
		$return .= "</div>";
	} else {
		return false;
	}
	
	return $return;
}
?>