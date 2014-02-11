###############################################################################
##
## Copyright (C) 2012-2013 Tavendo GmbH
##
## Licensed under the Apache License, Version 2.0 (the "License");
## you may not use this file except in compliance with the License.
## You may obtain a copy of the License at
##
##		http://www.apache.org/licenses/LICENSE-2.0
##
## Unless required by applicable law or agreed to in writing, software
## distributed under the License is distributed on an "AS IS" BASIS,
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
## See the License for the specific language governing permissions and
## limitations under the License.
##
###############################################################################


import sys, time
from struct import *
from subprocess import Popen
import os
import httplib, urllib

if sys.platform == 'win32':
	## on windows, we need to use the following reactor for serial support
	## http://twistedmatrix.com/trac/ticket/3802
	##
	from twisted.internet import win32eventreactor
	win32eventreactor.install()

from twisted.internet import reactor
print "Using Twisted reactor", reactor.__class__
print

from twisted.python import usage, log
from twisted.protocols.basic import LineReceiver
from twisted.internet.serialport import SerialPort
from twisted.web.server import Site
from twisted.web.static import File

from autobahn.twisted.websocket import listenWS
from autobahn.wamp import WampServerFactory, WampServerProtocol, exportRpc

import sqlite3
sq3con = None
p = None
pSkipped = 0			#how often was php not ready (in a row)
sqliteErrorCount = 0	#how did a database error occur

#Thermosetup
set_min = 0
set_max = 0
referenceVoltage = 0
pid_p = 0
pid_i = 0
pid_d = 0
pid_near_kp = 0
pid_near_ki = 0
pid_near_kd = 0
pid_nearfardelta = 0
pid_nearfartimewindow = 0
pid_settemp = 0
wemoIp = ""
aTuneStep = 0
aTuneNoise = 0
aTuneStartValue = 0
aTuneLookBack = 0

#Pushover
pushoverMessageCount = 0; #limit the number of message calls while running
pushoverMessageMax = 20;

class Serial2WsOptions(usage.Options):
	
	optFlags = [
		['debugserial', 'd', 'Turn on Serial data logging.'],
		['debugwamp', 't', 'Turn on WAMP traffic logging.'],
		['debugws', 'r', 'Turn on WebSocket traffic logging.']
	]

	optParameters = [
		['baudrate', 'b', 115200, 'Serial baudrate'],
		['port', 'p', '/dev/ttyATH0', 'Serial port to use (e.g. 3 for a COM port on Windows, /dev/ttyATH0 for Arduino Yun, /dev/ttyACM0 for Serial-over-USB on RaspberryPi'],
		['webport', 'w', 8080, 'Web port to use for embedded Web server'],
		['wsurl', 's', "ws://localhost:9000", 'WebSocket port to use for embedded WebSocket server']
	]

## Sent values to Arduino (used for delayed serial.write)
def sentAutotuneValuesToArduino(context, binaryPid):
	context.transport.write("Q" + binaryPid)

## MCU protocol
##
class McuProtocol(LineReceiver):

	## need a reference to our WS-MCU gateway factory to dispatch PubSub events
	##
	def __init__(self, wsMcuFactory):
		self.wsMcuFactory = wsMcuFactory


	## this method is exported as RPC and can be called by connected clients
	##
	@exportRpc("control-led")
	def controlLed(self, status):
		if status:
			payload = '1'
		else:
			payload = '0'
		if self.wsMcuFactory.debugSerial:
			print "Serial TX:", payload
		self.transport.write("O" + payload)
		
	@exportRpc("setTuning")
	def sendTuningCommand(self, doStart):
		if doStart:
			payload = '1'
		else:
			payload = '0'
		if self.wsMcuFactory.debugSerial:
			print "Setting tuning to:", payload
		self.transport.write("A" + payload)
		return True
		
	@exportRpc("resetPid")
	def sendResetPidSignal(self):
		if self.wsMcuFactory.debugSerial:
			print "Sending reset signal to arduino"
		self.transport.write("R")
		
	@exportRpc("togglePid")
	def sendTogglePidSignal(self):
		if self.wsMcuFactory.debugSerial:
			print "Sending toggle PID signal to arduino"
		self.transport.write("T")
		
	@exportRpc("getEntireDB")
	def readTemperatureDB(self, numberTotal):
		if self.wsMcuFactory.debugSerial:
			print "reading DB"
		sq3cur = sq3con.cursor()
		sq3cur.execute("SELECT COUNT(*) FROM `temperatures`")
		tempCount = sq3cur.fetchone()
		getEveryN = round(float(tempCount[0]) / float(numberTotal), 0)
		if getEveryN < 1:
			getEveryN = 1
		getEveryN = (getEveryN,)
		print "getting " + str(getEveryN)
		sq3cur.execute("SELECT temperature,datetime(timestamp,'unixepoch','localtime') AS timestamp,temp_ID FROM temperatures WHERE temperature != 0 AND temperature != 1023 AND temp_ID %? = 0 AND temp_ID > 60 ORDER BY temp_ID ASC", getEveryN)
		dbContents = sq3cur.fetchall()
		dbContentsTsv = "Ident\tZeitpunkt\tTemperatur\n"
		for row in dbContents:
			dbContentsTsv += str("{0}\t{1}\t{2}\n".format(row[2], row[1], row[0]))
		return dbContentsTsv
	
	@exportRpc("getSettings")
	def readTemperatureSettings(self):
		if self.wsMcuFactory.debugSerial:
			print "sending runtime thermo settings" + str(referenceVoltage)
		thermosetupTsv = "RangeMin\tRangeMax\tRefVolt\n"
		thermosetupTsv += str("{0}\t{1}\t{2}\n".format(set_min, set_max, referenceVoltage))
		return thermosetupTsv

	@exportRpc("getPidSettings")
	def readPidSettings(self):
		if self.wsMcuFactory.debugSerial:
			print "sending runtime PID settings"
		pidsetupTsv = "wemoIp\tpid_settemp\tpid_kp\tpid_ki\tpid_kd\tpid_near_kp\tpid_near_ki\tpid_near_kd\tpid_nearfardelta\tpid_nearfartimewindow\taTuneStep\taTuneNoise\taTuneStartValue\taTuneLookBack\n"
		pidsetupTsv += str("{0}\t{1}\t{2}\t{3}\t{4}\t{5}\t{6}\t{7}\t{8}\t{9}\t{10}\t{11}\t{12}\t{13}\n".format(wemoIp, pid_settemp, pid_p, pid_i, pid_d, pid_near_kp, pid_near_ki, pid_near_kd, pid_nearfardelta, pid_nearfartimewindow, aTuneStep, aTuneNoise, aTuneStartValue, aTuneLookBack))
		return pidsetupTsv

	@exportRpc("deleteAllData")
	def deleteTemperatureData(self):
		sq3cur = sq3con.cursor()
		sq3cur.execute("DELETE FROM temperatures")
		sq3cur.execute("DELETE FROM SQLITE_SEQUENCE WHERE name = 'temperatures'")
		if self.wsMcuFactory.debugSerial:
			print "All data was deleted"
		return True


	##Updates the settings in the database and resets the database
	@exportRpc("newThermoSettings")
	def writeThermoSettings(self, newRefVolt, newTempEnd, newTempStart):
		global referenceVoltage, set_min, set_max
		newTempEnd = float(newTempEnd)
		newTempStart = float(newTempStart)
		newRefVolt = float(newRefVolt)
		if (newTempEnd > 1372 or newTempEnd < -100):
			raise Exception("Temperature range end over limit of 1372 or below -100")
		if (newTempStart > 1372 or newTempStart < -100):
			raise Exception("Temperature range start over limit of 1372 or below -100")
		if (newRefVolt > 5):
			raise Exception("Reference voltage must not be above 5V")
		#check if the settings have changed
		if (newTempEnd == set_max and newTempStart == set_min and newRefVolt == referenceVoltage):
			raise Exception("Settings where not changed (values where as before)")
		thermoSettings = (newTempStart, newTempEnd, newRefVolt)
		sq3cur = sq3con.cursor()
		sq3cur.execute("UPDATE thermosetup SET set_min = ?, set_max = ?, referenceVoltage = ?", thermoSettings)
		#Safe settings for runtime
		referenceVoltage = newRefVolt
		set_min = newTempStart
		set_max = newTempEnd
		#also send these values to the arduino, since the PID will only be possible with the right temperature
		if self.wsMcuFactory.debugSerial:
			print "Sending PID and THERMO settings to Arduino: " + str(newTempEnd)
		binaryPid = pack('<ffffffffffffffff', pid_settemp, pid_p, pid_i, pid_d, pid_near_kp, pid_near_ki, pid_near_kd, pid_nearfardelta, pid_nearfartimewindow, set_min, set_max, referenceVoltage, aTuneStep, aTuneNoise, aTuneStartValue, aTuneLookBack)
		self.transport.write("P" + binaryPid)
		return True

	@exportRpc("newPIDSettings")
	def writePIDSettings(self, newWemoIP, newPidSettemp, newPID_kp, newPID_ki, newPID_kd, newPID_near_kp, newPID_near_ki, newPID_near_kd, newPID_nearfardelta, newPID_nearfartimewindow, newATuneStep, newATuneNoise, newATuneStartValue, newATuneLookBack):
		global pid_p, pid_i, pid_d, pid_near_kp, pid_near_ki, pid_near_kd, pid_nearfardelta, pid_nearfartimewindow, pid_settemp, wemoIp, aTuneStep, aTuneNoise, aTuneStartValue, aTuneLookBack
		#Parse floats
		newWemoIP = newWemoIP
		newPidSettemp = float(newPidSettemp)
		newPID_kp = float(newPID_kp)
		newPID_ki = float(newPID_ki)
		newPID_kd = float(newPID_kd)
		newPID_near_kp = float(newPID_near_kp)
		newPID_near_ki = float(newPID_near_ki)
		newPID_near_kd = float(newPID_near_kd)
		newPID_nearfardelta = float(newPID_nearfardelta)
		newPID_nearfartimewindow = float(newPID_nearfartimewindow)
		newATuneStep = float(newATuneStep)
		newATuneNoise = float(newATuneNoise)
		newATuneStartValue = float(newATuneStartValue)
		newATuneLookBack = float(newATuneLookBack)
		#Validate
		if (newPidSettemp > 80 or newPidSettemp < 0):
			raise Exception("Set Temperature must be between 0 C and 80 C")
		if (newPID_kp > 1000000 or newPID_kp < 0):
			raise Exception("PID - P must be between 0 and 1000000")
		if (newPID_ki > 1000000 or newPID_ki < 0):
			raise Exception("PID - I must be between 0 and 1000000")
		if (newPID_kd > 1000000 or newPID_kd < 0):
			raise Exception("PID - D must be between 0 and 1000000")
		thermoSettings = (newWemoIP, newPidSettemp, newPID_kp, newPID_ki, newPID_kd, newPID_near_kp, newPID_near_ki, newPID_near_kd, newPID_nearfardelta, newPID_nearfartimewindow, newATuneStep, newATuneNoise, newATuneStartValue, newATuneLookBack)
		sq3cur = sq3con.cursor()
		sq3cur.execute("UPDATE thermosetup SET wemo_ip = ?, wemo_temp_max = ?, pid_kp = ?, pid_ki = ?, pid_kd = ?, pid_near_kp = ?, pid_near_ki = ?, pid_near_kd = ?, pid_nearfardelta = ?, pid_nearfartimewindow = ?, aTuneStep = ?, aTuneNoise = ?, aTuneStartValue = ?, aTuneLookBack = ?", thermoSettings)
		if self.wsMcuFactory.debugSerial:
			print "Sending PID and THERMO settings to Arduino: " + str(newPidSettemp)
		binaryPid = pack('<ffffffffffff', newPidSettemp, newPID_kp, newPID_ki, newPID_kd, newPID_near_kp, newPID_near_ki, newPID_near_kd, newPID_nearfardelta, newPID_nearfartimewindow, set_min, set_max, referenceVoltage)
		self.transport.write("P" + binaryPid)
		binaryPid = pack('<ffff', newATuneStep, newATuneNoise, newATuneStartValue, newATuneLookBack)
		#The rest needs to be sent later, because the default Arduino buffersize is 64
		#Wait 300ms before sending
		reactor.callLater(.3, sentAutotuneValuesToArduino, self, binaryPid)
		#kill old  PHP process, if IP has changed
		if (wemoIp != newWemoIP):
			if (p):
				if (p.poll() == None):
					p.kill()
		#safe PID settings for runtime
		pid_p = newPID_kp
		pid_i = newPID_ki
		pid_d = newPID_kd
		pid_near_kp = newPID_near_kp
		pid_near_ki = newPID_near_ki
		pid_near_kd = newPID_near_kd
		pid_nearfardelta = newPID_nearfardelta
		pid_nearfartimewindow = newPID_nearfartimewindow
		pid_settemp = newPidSettemp
		wemoIp = newWemoIP
		aTuneStep = newATuneStep
		aTuneNoise = newATuneNoise
		aTuneStartValue = newATuneStartValue
		aTuneLookBack = newATuneLookBack
		return True

	def connectionMade(self):
		log.msg('Serial port connected.')


	def lineReceived(self, line):
	#/opt/usr/bin/php-cli -c /opt/etc/php.ini /mnt/sda1/wemo/wemoTimed.php 192.168.4.149 200
		global p, pSkipped, sqliteErrorCount
		if self.wsMcuFactory.debugSerial:
			print "Serial RX:", line
		if (line.startswith("P")):
			pidLength = int(float(line[1:]))
			print "PID detected: " + str(pidLength / 1000) + " seconds"
			if (p and (p.poll() == None)):
				#TODO: manage autoexit. run a php-shutdown script. then maybe count the number of failures and do a total exit after 5 or so?
				print "ALERT. PROCESS STILL RUNNING. SKIPPING THIS PROCESS RUN. SKIPPED " + str(pSkipped)
				pSkipped += 1
				self.wsMcuFactory.dispatch("http://raumgeist.dyndns.org/thermo#PIDOutputSkipped", pSkipped)
				if (pSkipped == 10):
					if (pushoverMessageCount < pushoverMessageMax):
						conn = httplib.HTTPSConnection("api.pushover.net:443")
						conn.request("POST", "/1/messages.json",
							urllib.urlencode({
								"token": "aPHxby54Su8bXMLMCYY4ESapXBnLmS",
								"user": "uBdVQW8BKVXnF9vVdhUSpnVcVKWsSv",
								"priority": "1",
								"sound": "falling",
								"message": "Socket was not responsive for 10 times in a row",}), { "Content-type": "application/x-www-form-urlencoded" })
						pushoverMessageCount++
					elif (pushoverMessageCount == pushoverMessageMax):
						conn = httplib.HTTPSConnection("api.pushover.net:443")
						conn.request("POST", "/1/messages.json",
							urllib.urlencode({
								"token": "aPHxby54Su8bXMLMCYY4ESapXBnLmS",
								"user": "uBdVQW8BKVXnF9vVdhUSpnVcVKWsSv",
								"priority": "1",
								"sound": "falling",
								"message": "Socket unresponsive. Too many warnings sent. This will be the last warning until server reset.",}), { "Content-type": "application/x-www-form-urlencoded" })
						pushoverMessageCount++
			else:
				p = Popen(['/opt/usr/bin/php-cli','-c','/opt/etc/php.ini','/mnt/sda1/wemo/wemoTimed.php',str(wemoIp),str(pidLength)])
				pSkipped = 0
				self.wsMcuFactory.dispatch("http://raumgeist.dyndns.org/thermo#PIDOutputSent", pidLength)
		elif (line.startswith("A")):
			try:
				autoTuneString = str(line[1:])
				autoTuneData = [float(x) for x in autoTuneString.split()]
				autoTuneJson = {'p': autoTuneData[0], 'i': autoTuneData[1], 'd': autoTuneData[2]}
				self.wsMcuFactory.dispatch("http://raumgeist.dyndns.org/thermo#autoTuneReady", autoTuneJson)
			except ValueError:
				log.err('Unable to parse value %s' % line)
		else:
			try:
				## parse data received from MCU
				##
				data = [int(x) for x in line.split()]

				## construct PubSub event from raw data
				##
				evt = {'Zeitpunkt': time.strftime("%Y-%m-%d %H:%M:%S"), 'Temperatur': data[1]}

				## publish event to all clients subscribed to topic
				##
				self.wsMcuFactory.dispatch("http://raumgeist.dyndns.org/thermo#rawValue", evt)
			
				rawTemp = (data[1],)
				sq3cur = sq3con.cursor()
				try:
					sq3cur.execute("INSERT INTO temperatures (temperature) VALUES (?)", rawTemp)
					sq3con.commit()
				except sqlite3.OperationalError, msg:
					sqliteErrorCount += 1
					if self.wsMcuFactory.debugSerial:
						print "sqlite error: ", msg
			except ValueError:
				log.err('Unable to parse value %s' % line)

## Kais shutdown procedure
def sqlite3Close():
	global p
	if (p):
		#TODO: wait for process to finish. then close the process after a while if not finished and run the php stop script
		if (p.poll() == None):
			p.kill()
			#Sending shutoff signal
			p = Popen(['/opt/usr/bin/php-cli','-c','/opt/etc/php.ini','/mnt/sda1/wemo/wemoOff.php',str(wemoIp)])
			print "PHP wemo OFF called"
		print "PHP process was closed at shutdown time"
	sq3con.close()
	print "Database closed successfully"
	
## Loads the thermometer settings from database for the runtime
def loadThermoSettings():
	global set_min, set_max, referenceVoltage
	global pid_p, pid_i, pid_d, pid_settemp, wemoIp
	global pid_near_kp, pid_near_ki, pid_near_kd, pid_nearfardelta, pid_nearfartimewindow
	global aTuneStep, aTuneNoise, aTuneStartValue, aTuneLookBack
	sq3cur = sq3con.cursor()
	sq3cur.execute("SELECT set_min,set_max,referenceVoltage,pid_kp,pid_ki,pid_kd,pid_near_kp,pid_near_ki,pid_near_kd,pid_nearfardelta,pid_nearfartimewindow,wemo_temp_max,wemo_ip,aTuneStep,aTuneNoise,aTuneStartValue,aTuneLookBack FROM `thermosetup`")
	thermosetup = sq3cur.fetchone()
	set_min = float(thermosetup[0])
	set_max = float(thermosetup[1])
	referenceVoltage = float(thermosetup[2])
	pid_p = float(thermosetup[3])
	pid_i = float(thermosetup[4])
	pid_d = float(thermosetup[5])
	pid_near_kp = float(thermosetup[6])
	pid_near_ki = float(thermosetup[7])
	pid_near_kd = float(thermosetup[8])
	pid_nearfardelta = float(thermosetup[9])
	pid_nearfartimewindow = float(thermosetup[10])
	pid_settemp = float(thermosetup[11])
	wemoIp = thermosetup[12]
	aTuneStep = thermosetup[13]
	aTuneNoise = thermosetup[14]
	aTuneStartValue = thermosetup[15]
	aTuneLookBack = thermosetup[16]
	
	print "Settings read from database for runtime: eg. REFVOLT=" + str(referenceVoltage) + " and " + str(wemoIp) + " and " + str(aTuneStartValue)

## WS-MCU protocol
##
class WsMcuProtocol(WampServerProtocol):

	def onSessionOpen(self):
		## register topic prefix under which we will publish MCU measurements
		##
		self.registerForPubSub("http://raumgeist.dyndns.org/thermo#", True)

		## register methods for RPC
		##
		self.registerForRpc(self.factory.mcuProtocol, "http://raumgeist.dyndns.org/thermoControl#")


## WS-MCU factory
##
class WsMcuFactory(WampServerFactory):

	protocol = WsMcuProtocol

	def __init__(self, url, debugSerial = False, debugWs = False, debugWamp = False):
		WampServerFactory.__init__(self, url, debug = debugWs, debugWamp = debugWamp)
		self.debugSerial = debugSerial
		self.mcuProtocol = McuProtocol(self)


if __name__ == '__main__':

	## parse options
	##
	o = Serial2WsOptions()
	try:
		o.parseOptions()
	except usage.UsageError, errortext:
		print '%s %s' % (sys.argv[0], errortext)
		print 'Try %s --help for usage details' % sys.argv[0]
		sys.exit(1)

	debugWs = bool(o.opts['debugws'])
	debugWamp = bool(o.opts['debugwamp'])
	debugSerial = bool(o.opts['debugserial'])
	baudrate = int(o.opts['baudrate'])
	port = o.opts['port']
	webport = int(o.opts['webport'])
	wsurl = o.opts['wsurl']
	
	## connect to database
	try:
		sq3con = sqlite3.connect('/mnt/sda1/temperatureDb.sqlite3')
	except usage.UsageError, errortext:
		print 'DB Connection error'
		sys.exit(1)
	loadThermoSettings()
	reactor.addSystemEventTrigger('before', 'shutdown', sqlite3Close)

	## start Twisted log system
	##
	log.startLogging(sys.stdout)

	## create Serial2Ws gateway factory
	##
	wsMcuFactory = WsMcuFactory(wsurl, debugSerial = debugSerial, debugWs = debugWs, debugWamp = debugWamp)
	listenWS(wsMcuFactory)

	## create serial port and serial port protocol
	##
	log.msg('About to open serial port %s [%d baud] ..' % (port, baudrate))
	serialPort = SerialPort(wsMcuFactory.mcuProtocol, port, reactor, baudrate = baudrate)

	## create embedded web server for static files
	##
	webdir = File(".")
	web = Site(webdir)
	reactor.listenTCP(webport, web)
	
	## start Twisted reactor ..
	##
	reactor.run()
