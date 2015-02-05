// ----------------------------------------------------------------------------
// -------------------------- Autobahn and WAMP part --------------------------
// ----------------------------------------------------------------------------

var sess = null;
var wsuri = "ws://" + window.location.hostname + ":9000";
var thermoRefVoltage, thermoRangeMax, thermoRangeMin;
var pidSetTemp = 0;

function resetPid(){
	if (!confirm("Are you sure you want to reset the PID process?")){
		return false;
	}
	
	console.log("sending reset signal");
	sess.call("rpc:resetPid").always(ab.log);
}

function switchPid(status){
	console.log("sending pid signal " + status);
	sess.call("rpc:togglePid", status).always(ab.log);
}

function controlSocket(status){
	console.log("sending powersocket signal " + status);
	sess.call("rpc:manualPowerSwitch", status).always(ab.log);
}

function autoTuneDataReceived(topicUri, aTuneData) {
	console.log(aTuneData);
	alert("Autotune Data received: \nP: " + aTuneData.p + "\nI: " + aTuneData.i + "\nD: " + aTuneData.d);
}

function nearFarChangeReceived(topicUri, changeMessage) {
	console.log("Changing between near and far parameter set: " + changeMessage);
	//alert("Changing between near and far parameter set: " + changeMessage);
	//TODO: on screen message
}

function pidSkipped(topicUri, skipCount) {
	console.log("PID output skipped. Happened " + skipCount + " times in a row.");
	if (skipCount == 10){
		alert("PID skipped 10 times in a row");
	}
}

function pidSent(topicUri, pidLength) {
	console.log("PID sent to WEMO: " + pidLength + "ms");
}

function pidSentRelais(topicUri, pidData) {
	console.log("PID sent via relais: " + pidData.pidLength + "ms");

	pidData.Zeitpunkt = parseDate(pidData.Zeitpunkt);
	
	pidGraphData.push(pidData);
	//draw new graphs
	graphMain.select("path.pidLine").datum(pidGraphData).attr("d", linePid);

}

//function receives WS new data event and draws the single datum
function newRawDataReceived(topicUri, singleDatum) {
	singleDatum.Zeitpunkt = parseDate(singleDatum.Zeitpunkt);
	singleDatum.Temperatur = +calculateTemperature(singleDatum.Temperatur);
	
	//add new data to array
	graphData.push({'Zeitpunkt':singleDatum.Zeitpunkt, 'Temperatur':singleDatum.Temperatur, 'Temperatur2': singleDatum.Temperatur2});
	
	//TODO: can we do an extend over two columns to avoid these two lines?
	//rescale
	var yRange = d3.extent(graphData, function(d) { return d.Temperatur; });
	var y2Range = d3.extent(graphData, function(d) { return d.Temperatur2; });
	x2.domain(d3.extent(graphData, function(d) { return d.Zeitpunkt; }));
	//choose which min is lower, y or y2, and choose which max is higher, y or y2
	y2.domain([Math.min(yRange[0],y2Range[0]) - temperatureMargin,Math.max(yRange[1],y2Range[1]) + temperatureMargin]);
	y.domain(y2.domain()); //set main window y scale to brush window scale
	
	//TODO: check if we can use updateGraphDrawing() here
	
	//so vielleicht?
	if (runningBrush){
		var brushLeft = d3.time.minute.offset(singleDatum.Zeitpunkt, -10);
		var brushRight = d3.time.minute.offset(singleDatum.Zeitpunkt, 0);
		brush.extent([brushLeft,brushRight]);
		graphBrush.select(".brush").call(brush);
		x.domain(brush.extent()); //change window of main graph
	}
	
	//draw new graphs
	graphMain.select("path.line").datum(graphData).attr("d", line);
	graphMain.select("path.temp2line").datum(graphData).attr("d", lineTemp2);
	graphMain.select("path.pidLine").datum(pidGraphData).attr("d", linePid);
	graphMain.select(".x.axis").call(xAxis);
	graphMain.select(".y.axis").call(yAxis);
	graphBrush.select("path.lineContext").datum(graphData).attr("d", line2);
	graphBrush.select(".x.axis").call(xAxis2);
	
	//move settemp lines vertically (always need to move because scale might have changed)
	graphMain.select(".rightOnTemp").attr("transform","translate(0," + y(pidSetTemp) + ")");
	graphMain.select("#blurTop").attr("transform","translate(0," + y(pidSetTemp + pidSetTempBlur) + ")");
	graphMain.select("#blurBottom").attr("transform","translate(0," + y(pidSetTemp - pidSetTempBlur) + ")");
	
	currentTemperatureText.text(singleDatum.Temperatur + "°C");
	console.log("received " + singleDatum.Temperatur + "C");
	return;
}

function updateGraphDrawing(){
	//rescale
	var yRange = d3.extent(graphData, function(d) { return d.Temperatur; });
	var y2Range = d3.extent(graphData, function(d) { return d.Temperatur2; });
	x2.domain(d3.extent(graphData, function(d) { return d.Zeitpunkt; }));
	y2.domain([Math.min(yRange[0],y2Range[0]) - temperatureMargin,Math.max(yRange[1],y2Range[1]) + temperatureMargin]);
	y.domain(y2.domain()); //set main window y scale to brush window scale
	
	//draw new graphs
	graphMain.select("path.line").datum(graphData).attr("d", line);
	graphMain.select("path.temp2line").datum(graphData).attr("d", lineTemp2);
	graphMain.select("path.pidLine").datum(pidGraphData).attr("d", linePid);
	graphMain.select(".x.axis").call(xAxis);
	graphMain.select(".y.axis").call(yAxis);
	graphBrush.select("path.lineContext").datum(graphData).attr("d", line2);
	graphBrush.select(".x.axis").call(xAxis2);
}

function controlLed(status) {
	sess.call("rpc:control-led", status).always(ab.log);
}

function controlAlarm(status) {
	sess.call("rpc:control-alarm", status).always(ab.log);
}

function setAutoTune(status) {
	sess.call("rpc:setTuning", status).always(ab.log);
}

//this function askes for the settings and is called once in the beginning
//it also asks for all graph data and triggers the drawing of the graph
function askForSettings(updateGraph){
	//should all data be retrieved and the graph be drawn with this data
	if (updateGraph == null){
		updateGraph = true;
	}
	sess.call("rpc:getSettings").then(function (result) {
		//receive and parse settings
		var settings = d3.tsv.parse(result);
		thermoRefVoltage = parseFloat(settings[0].RefVolt);
		thermoRangeMax = parseFloat(settings[0].RangeMax);
		thermoRangeMin = parseFloat(settings[0].RangeMin);
		
		document.getElementById('refVolt').value = thermoRefVoltage;
		document.getElementById('tempRangeEnd').value = thermoRangeMax;
		document.getElementById('tempRangeStart').value = thermoRangeMin;
		
		if (updateGraph){
			//ask for all values
			sess.call("rpc:getEntireDB", 50).then(function (resultValues) {
				data = d3.tsv.parse(resultValues);
				data.forEach(function(d) {
					d.Zeitpunkt = parseDate(d.Zeitpunkt);
					d.Temperatur = +(calculateTemperature(d.Temperatur));
					d.Temperatur2 = d.Temperatur2;
				});
				
				//store data in globally available array
				graphData = data;
				console.log('Graph data stored');
				
				//only really draw the graph if we have never drawn the graph
				if (graphData.length > 0 && !graphDrawn) {
					console.log('Redrawing graph');
					drawGraph();
				}
			});
		}
	});
}

//this function askes for the settings and is called once in the beginning
function askForPidSettings(){
	sess.call("rpc:getPidSettings").then(function (result) {
		//receive and parse settings
		var settings = d3.tsv.parse(result);
		wemoIp = settings[0].wemoIp;
		pid_settemp = parseFloat(settings[0].pid_settemp);
		pid_kp = parseFloat(settings[0].pid_kp);
		pid_ki = parseFloat(settings[0].pid_ki);
		pid_kd = parseFloat(settings[0].pid_kd);
		pid_near_kp = parseFloat(settings[0].pid_near_kp);
		pid_near_ki = parseFloat(settings[0].pid_near_ki);
		pid_near_kd = parseFloat(settings[0].pid_near_kd);
		pid_nearfardelta = parseFloat(settings[0].pid_nearfardelta);
		pid_nearfartimewindow = parseFloat(settings[0].pid_nearfartimewindow);
		aTuneStep = parseFloat(settings[0].aTuneStep);
		aTuneNoise = parseFloat(settings[0].aTuneNoise);
		aTuneStartValue = parseFloat(settings[0].aTuneStartValue);
		aTuneLookBack = parseFloat(settings[0].aTuneLookBack);
		
		document.getElementById('wemoIp').value = wemoIp;
		document.getElementById('pid_settemp').value = pid_settemp;
		document.getElementById('pid_kp').value = pid_kp;
		document.getElementById('pid_ki').value = pid_ki;
		document.getElementById('pid_kd').value = pid_kd;
		document.getElementById('pid_near_kp').value = pid_near_kp;
		document.getElementById('pid_near_ki').value = pid_near_ki;
		document.getElementById('pid_near_kd').value = pid_near_kd;
		document.getElementById('pid_nearfardelta').value = pid_nearfardelta;
		document.getElementById('pid_nearfartimewindow').value = pid_nearfartimewindow;
		document.getElementById('aTuneStep').value = aTuneStep;
		document.getElementById('aTuneNoise').value = aTuneNoise;
		document.getElementById('aTuneStartValue').value = aTuneStartValue;
		document.getElementById('aTuneLookBack').value = aTuneLookBack;
		pidSetTemp = pid_settemp;
		
	});
}

function sendPIDSettings(){
	var theForm = document.getElementById('pidSettings');
	if (!theForm.checkValidity()){
		event.preventDefault();
		alert("Error in PID values. Resetting values");
		//receive (old) settings
		askForPidSettings(false);
		return false;
	}

	//read values from form
	sendWemoIp = document.getElementById('wemoIp').value;
	sendPid_settemp = document.getElementById('pid_settemp').value;
	sendPid_kp = document.getElementById('pid_kp').value;
	sendPid_ki = document.getElementById('pid_ki').value;
	sendPid_kd = document.getElementById('pid_kd').value;
	sendPid_near_kp = document.getElementById('pid_near_kp').value;
	sendPid_near_ki = document.getElementById('pid_near_ki').value;
	sendPid_near_kd = document.getElementById('pid_near_kd').value;
	sendPid_nearfardelta = document.getElementById('pid_nearfardelta').value;
	sendPid_nearfartimewindow = document.getElementById('pid_nearfartimewindow').value;
	sendATuneStep = document.getElementById('aTuneStep').value;
	sendATuneNoise = document.getElementById('aTuneNoise').value;
	sendATuneStartValue = document.getElementById('aTuneStartValue').value;
	sendATuneLookBack = document.getElementById('aTuneLookBack').value;

	//send these values
	sess.call("rpc:newPIDSettings", sendWemoIp, sendPid_settemp, sendPid_kp, sendPid_ki, sendPid_kd, sendPid_near_kp, sendPid_near_ki, sendPid_near_kd, sendPid_nearfardelta, sendPid_nearfartimewindow, sendATuneStep, sendATuneNoise, sendATuneStartValue, sendATuneLookBack).then(
		function (result) {
			//safe the values
			pidSetTemp = parseFloat(sendPid_settemp);
			//update settemp lines
			graphMain
				.select(".rightOnTemp")
				.transition()
				.duration(1000)
				.attr("transform","translate(0," + y(pidSetTemp) + ")");
			graphMain
				.select("#blurTop")
				.transition()
				.duration(1000)
				.attr("transform","translate(0," + y(pidSetTemp + pidSetTempBlur) + ")");
			graphMain
				.select("#blurBottom")
				.transition()
				.duration(1000)
				.attr("transform","translate(0," + y(pidSetTemp - pidSetTempBlur) + ")");
			console.log('Successfully sent settings');
		}, function (error) {
			alert("Server did not accept PID values. Resetting values");
			console.log('Error setting PID settings');
			console.log(error);
			//receive (old) settings
			askForPidSettings(false);
		}
	);

}

function resetGraph(){
	if (!confirm("Are you sure you want to reset the graph?")){
		return false;
	}
	
	sess.call("rpc:deleteAllData").then(
		function (result) {
			askForSettings(true); //asks for data and redraws the graph
			runningBrush = true; //otherwise we might not be displaying anything
			console.log('Successfully deleted all data');
		}, function (error) {
			alert("Server answered with error code.\nMessage: " + error.desc);
			console.log('Error resetting graph');
			console.log(error);
		}
	);

}

function sendThermoSettings(){
	var theForm = document.getElementById('tempSettings');
	if (!theForm.checkValidity()){
		event.preventDefault();
		alert("Error in values. Resetting values");
		//receive (old) settings
		askForSettings(false);
		return false;
	}
	
	//read values from form
	sendRefVolt = document.getElementById('refVolt').value;
	sendTempRangeEnd = document.getElementById('tempRangeEnd').value;
	sendTempRangeStart = document.getElementById('tempRangeStart').value;
	
	//send these values
	sess.call("rpc:newThermoSettings", sendRefVolt, sendTempRangeEnd, sendTempRangeStart).then(
		function (result) {
			askForSettings(true); //we need to receive the raw values again. these where not stored.  this asks for data and redraws the graph
			updateGraphDrawing();
			console.log('Successfully sent new settings');
		}, function (error) {
			alert("Server did not accept values. Resetting values.\nMessage: " + error.desc);
			console.log('Error resettings graph or sending settings');
			console.log(error);
			//receive (old) settings
			askForSettings(false);
		}
	);
}

window.onload = function () {
	// connect to WAMP server
	ab.connect(wsuri,
	
	   // WAMP session was established
	   function (session) {
	
	      sess = session;
	      console.log("Connected to " + wsuri);
	      retryCount = 0;
	
	      sess.prefix("event", "http://raumgeist.dyndns.org/thermo#");
	      sess.subscribe("event:rawValue", newRawDataReceived);
	      sess.subscribe("event:autoTuneReady", autoTuneDataReceived);
	      sess.subscribe("event:NearFarChange", nearFarChangeReceived);
	      sess.subscribe("event:PIDOutputSkipped", pidSkipped);
	      sess.subscribe("event:PIDOutputSent", pidSent);
	      sess.subscribe("event:PIDOutputSentRelais", pidSentRelais);
	
	      sess.prefix("rpc", "http://raumgeist.dyndns.org/thermoControl#");
	
	      eventCnt = 0;
	
	      //get entire data once in the beginning
	      askForSettings();
	      askForPidSettings();
	      
	      //window.setInterval(updateEventCnt, eventCntUpdateInterval * 1000);
	   },
	
	   // WAMP session is gone
	   function (code, reason) {
	
	      sess = null;
	      console.log(reason);
	   }
	);
}

//----------------------------------------------------------
//-------------------------- D3JS --------------------------
//----------------------------------------------------------

var	hoverLineX, //X-part of crosshair
	hoverLineY, //Y-part of crosshair
	hoverLayer, //group containing circle, and both texts in the center of the crosshair
	hoverLayerPid, //group containing circle and data for PID line
	hoverTextPidLength, //pid length next to circle
	hoverTextDate, //date next to crosshair
	hoverTextTemperature, //temperature next to crosshair
	currentTemperatureText, //temperature next to crosshair
	runningBrush = true,
	graphData = []; //the data
	pidGraphData = []; //the pid runtime data

var graphDrawn = false; //is the graph drawn already

var startWindowTime = 1800, //Window width in seconds
	temperatureMargin = 5; //temperature scope to extend beyond real scope

var parseDate = d3.time.format("%Y-%m-%d %H:%M:%S").parse;
var bisectDate = d3.bisector(function(d) { return d.Zeitpunkt; }).left;

var margin = {top: 10, right: 10, bottom: 100, left: 40},
	margin2 = {top: 430, right: 10, bottom: 20, left: 40},
	width = 960 - margin.left - margin.right,
	height = 500 - margin.top - margin.bottom;
	height2 = 500 - margin2.top - margin2.bottom;

var x = d3.time.scale().range([0, width]),
	x2 = d3.time.scale().range([0, width]),
	y = d3.scale.linear().range([height, 0]),
	y2 = d3.scale.linear().range([height2, 0]),
	yPid = d3.scale.linear().range([height, 0]);
	

var xAxis = d3.svg.axis().scale(x).orient("bottom"),
	xAxis2 = d3.svg.axis().scale(x2).orient("bottom"),
	yAxis = d3.svg.axis().scale(y).ticks(20).tickSubdivide(10).tickSize(10,5,0).orient("left");

var test1 = "2014-01-14 01:00:00";
var test2 = "2014-01-14 22:45:00";
var brush = d3.svg.brush()
	.x(x2)
	.on("brush", brushFunction);

var line = d3.svg.line()
	.interpolate("linear") //linear (-open/-closed) linear (-closed) monotone step-before step-after cardinal (-open/-closed) bundle
	.x(function(d) { return x(d.Zeitpunkt); })
	.y(function(d) { return y(d.Temperatur); });

var line2 = d3.svg.line()
	.x(function(d) { return x2(d.Zeitpunkt); })
	.y(function(d) { return y2(d.Temperatur); });

var lineTemp2 = d3.svg.line()
	.interpolate("linear") //linear (-open/-closed) linear (-closed) monotone step-before step-after cardinal (-open/-closed) bundle
	.x(function(d) { return x(d.Zeitpunkt); })
	.y(function(d) { return y(d.Temperatur2); });

var linePid = d3.svg.line()
	.interpolate("basis") //basis, linear (-open/-closed) linear (-closed) monotone step-before step-after cardinal (-open/-closed) bundle
	.x(function(d) { return x(d.Zeitpunkt); })
	.y(function(d) { return yPid(d.pidLength); });

//plus minus how much is acceptable? show that in the graph
var pidSetTempBlur = .5;

//--------------------------------------------------------------------------------------------------------
//----------------------------------------SVG CREATION START----------------------------------------------
//--------------------------------------------------------------------------------------------------------
var svg = d3.select("body").append("svg")
	.attr("class", "d3svg")
	.attr("width", width + margin.left + margin.right)
	.attr("height", height + margin.top + margin.bottom);

svg.append("defs").append("clipPath")
	.attr("id", "clip")
.append("rect")
	.attr("width", width)
	.attr("height", height);

var graphMain = svg.append("g")
	.attr("transform", "translate(" + margin.left + "," + margin.top + ")");

var graphBrush = svg.append("g")
	.attr("transform", "translate(" + margin2.left + "," + margin2.top + ")");

//hover X
hoverLineX = graphMain.append("svg:line")
	.attr('class', 'hoverLine')
	.attr('y1', 0)
    .attr('y2', height)
    .style("display","none");

//hover Y
hoverLineY = graphMain.append("svg:line")
	.attr('class', 'hoverLine')
	.attr('x1', 0)
    .attr('x2', width)
    .style("display","none");

hoverLayer = graphMain.append("g")
    .style("display", "none");

hoverLayer.append("circle")
    .attr("class", "hoverCircle")
    .attr("r", 4.5);

hoverLayerPid = graphMain.append("g")
	.style("display", "none");

hoverLayerPid.append("circle")
	.attr("class", "hoverCirclePid")
	.attr("r", 6);

hoverTextPidLength = hoverLayerPid.append("text")
	.attr('y',-15)
	.attr('x',-5)
	.attr('dy','1em')
	.attr('class', 'hoverTextPid')
	.text('xyz');

//hover Text
hoverTextDate = hoverLayer.append('text')
	.attr('y',5)
	.attr('x',5)
	.attr('dy','1em')
	.style('text-anchor','start')
	.text('xyz');
    
hoverTextTemperature = hoverLayer.append('text')
	.attr('y',-15)
	.attr('x',-5)
	.attr('dy','1em')
	.style('text-anchor','end')
	.text('xyz');

graphMain.append("rect")
    .attr("class", "overlay")
    .attr("width", width)
    .attr("height", height)
    .on("mouseover", function() { hoverLayer.style("display", null); hoverLayerPid.style("display",null); hoverLineX.style("display" , null); hoverLineY.style("display" , null);})
    .on("mouseout", function() { hoverLayer.style("display", "none"); hoverLayerPid.style("display","none"); hoverLineX.style("display", "none"); hoverLineY.style("display", "none");})
    .on("mousemove", mousemove);

var currentTemperatureGroup = graphMain.append("g")
	.attr("transform", "translate(" + width/2 + "," + (30) + ")");
	
currentTemperatureGroup.append("rect")
    .attr("width", 100)
    .attr("x",-50)
    .attr("height", "4em")
    .attr("class","currentTempRect")
    .attr("rx",10)
    .attr("ry",10);

currentTemperatureText = currentTemperatureGroup.append("text")
	.attr('x',0)
	.attr('y',".5em")
	.attr('dy','1em')
	.style('text-anchor','middle')
	.attr('class','currentTempText')
	.text('WAIT');

//--------------------------------------------------------------------------------------------------------
//----------------------------------------SVG CREATION END----------------------------------------------
//--------------------------------------------------------------------------------------------------------

function calculateTemperature(rawTemp){
	var temp = rawTemp / 1023.0 * thermoRefVoltage;
	temp = thermoRangeMin + (temp * (thermoRangeMax - thermoRangeMin));
	return temp.toFixed(1);
}

function setRange(){
	//configure tick values and domain (range from to)
	var yRange = d3.extent(graphData, function(d) { return d.Temperatur; });
	var y2Range = d3.extent(graphData, function(d) { return d.Temperatur2; });
	
	//set range
	x.domain(d3.extent(graphData, function(d) { return d.Zeitpunkt; }));
	y2.domain([Math.min(yRange[0],y2Range[0]) - temperatureMargin,Math.max(yRange[1],y2Range[1]) + temperatureMargin]);
	x2.domain(x.domain());
	y2.domain(y.domain());
	yPid.domain([-500,10000]);
}

function drawGraph(){
	setRange();
	
	//Temperatur Pfad
	graphMain.append("path")
		.datum(graphData)
		.attr("class", "line")
		.attr("clip-path", "url(#clip)")
		.attr("d", line);
	
	graphMain.append("path")
		.datum(graphData)
		.attr("class", "temp2line")
		.attr("clip-path", "url(#clip)")
		.attr("d", lineTemp2);
	
	graphMain.append("path")
		.datum(pidGraphData)
		.attr("class", "pidLine")
		.attr("clip-path", "url(#clip)")
		.attr("d", linePid);
	
	//x Achse
	graphMain.append("g")
		.attr("class", "x axis")
		.attr("transform", "translate(0," + height + ")")
		.call(xAxis);
	
	//y Achse
	graphMain.append("g")
		.attr("class", "y axis")
		.call(yAxis)
		.append("text")
			.attr("transform", "rotate(-90)")
			.attr("y", 6)
			.attr("dy", ".71em")
			.style("text-anchor", "end")
			.text("temperature °C");
	
	//the set temp lines
	graphMain.append("line")
		.attr("class","rightOnTemp")
		.attr("transform","translate(0," + y(pidSetTemp) + ")")
		.attr("x1",0)
		.attr("x2",width);

	graphMain.append("line")
		.attr("id","blurTop")
		.attr("class","blurredTemp")
		.attr("x1",0)
		.attr("x2",width)
		.attr("transform","translate(0," + y(pidSetTemp + pidSetTempBlur) + ")");
		
	graphMain.append("line")
		.attr("id","blurBottom")
		.attr("class","blurredTemp")
		.attr("x1",0)
		.attr("x2",width)
		.attr("transform","translate(0," + y(pidSetTemp - pidSetTempBlur) + ")");
	
    
    //context / brush
	graphBrush.append("path")
		.datum(graphData)
		.attr("class","lineContext")
		.attr("d", line2);

	graphBrush.append("g")
	    .attr("class", "x axis")
	    .attr("transform", "translate(0," + height2 + ")")
	    .call(xAxis2);
	
	graphBrush.append("g")
	    .attr("class", "x brush")
	    .call(brush)
	  .selectAll("rect")
	    .attr("y", -6)
	    .attr("height", height2 + 7);
	
	graphDrawn = true;
}

function mousemove(){
	if (graphData.length == 0){
		return;
	}
	
    var x0 = x.invert(d3.mouse(this)[0]),
		i = bisectDate(graphData, x0, 1),
		d0 = graphData[i - 1],
		d1 = graphData[i],
		d = x0 - d0.Zeitpunkt > d1.Zeitpunkt - x0 ? d1 : d0;
	if (d != undefined) {
		var transX = x(d.Zeitpunkt),
			transY = y(d.Temperatur);
			
	    hoverLayer.attr("transform", "translate(" + transX + "," + transY + ")");
	    
	    hoverLineX.attr('x1', transX).attr('x2', transX);
	    hoverLineY.attr('y1', transY).attr('y2', transY);
	
	    //change Text
	    var format = d3.time.format("%H:%M:%S");
	    hoverTextDate.text(format(d.Zeitpunkt));
	    hoverTextTemperature.text(d.Temperatur + "°C / " + d.Temperatur2 + "°C");
	}

    if (pidGraphData.length > 0){
		//the same for the pid data
		var iPid = bisectDate(pidGraphData, x0, 1),
			d0Pid = pidGraphData[iPid - 1];
			d1Pid = pidGraphData[iPid];
			dPid = x0 - d0Pid.Zeitpunkt > d1.Zeitpunkt - x0 ? d1Pid : d0Pid;
		if (dPid != undefined){
			var transXPid = x(dPid.Zeitpunkt),
				transYPid = yPid(dPid.pidLength);
			
			hoverLayerPid.attr("transform", "translate(" + transXPid + "," + transYPid + ")");
			hoverTextPidLength.text(dPid.pidLength + "ms");
		}
    }
    
    //TODO: now create a dot for the second temperature2 (hoverLayer2?)
}

function brushFunction() {
	x.domain(brush.empty() ? x2.domain() : brush.extent());
	graphMain.select("path.line").attr("d", line);
	graphMain.select("path.temp2line").attr("d", lineTemp2);
	graphMain.select("path.pidLine").attr("d", linePid);
	graphMain.select(".x.axis").call(xAxis);
	runningBrush = false; //disable the running brush
}
