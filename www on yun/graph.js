var	hoverLineX, //X-part of crosshair
	hoverLineY, //Y-part of crosshair
	hoverLayer, //group containing circle, and both texts in the center of the crosshair
	hoverTextDate, //date next to crosshair 
	currentTemperatureText, //temperature next to crosshair
	runningBrush = true,
	graphData; //the data

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
	y2 = d3.scale.linear().range([height2, 0]);

var xAxis = d3.svg.axis().scale(x).orient("bottom"),
	xAxis2 = d3.svg.axis().scale(x2).orient("bottom"),
	yAxis = d3.svg.axis().scale(y).tickSubdivide(3).tickSize(10,5,0).orient("left");

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
    .on("mouseover", function() { hoverLayer.style("display", null); hoverLineX.style("display" , null); hoverLineY.style("display" , null);})
    .on("mouseout", function() { hoverLayer.style("display", "none"); hoverLineX.style("display", "none"); hoverLineY.style("display", "none");})
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


//get data once in the beginning
d3.tsv("getData.php", function(error, data) {
	
	data.forEach(function(d) {
		d.Zeitpunkt = parseDate(d.Zeitpunkt);
		d.Temperatur = +(calculateTemperature(d.Temperatur));
	});
	
	//store data in globally available array
	graphData = data;
	
	drawGraph();
	setInterval(tempClick, 2000);
});

function setRange(){
	//configure tick values and domain (range from to)
	var yRange = d3.extent(graphData, function(d) { return d.Temperatur; });
	
	//set range
	x.domain(d3.extent(graphData, function(d) { return d.Zeitpunkt; }));
	y.domain([yRange[0] - temperatureMargin,yRange[1] + temperatureMargin]);
	x2.domain(x.domain());
	y2.domain(y.domain());
}

function drawGraph(){
	setRange();
	
	//Temperatur Pfad
	graphMain.append("path")
		.datum(graphData)
		.attr("class", "line")
		.attr("clip-path", "url(#clip)")
		.attr("d", line);	
	
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
}

function mousemove(){
    var x0 = x.invert(d3.mouse(this)[0]),
		i = bisectDate(graphData, x0, 1),
		d0 = graphData[i - 1],
		d1 = graphData[i],
		d = x0 - d0.Zeitpunkt > d1.Zeitpunkt - x0 ? d1 : d0,
		transX = x(d.Zeitpunkt),
		transY = y(d.Temperatur);
    hoverLayer.attr("transform", "translate(" + transX + "," + transY + ")");
    
    hoverLineX.attr('x1', transX).attr('x2', transX);
    hoverLineY.attr('y1', transY).attr('y2', transY);

    //change Text
    var format = d3.time.format("%H:%M:%S");
    hoverTextDate.text(format(d.Zeitpunkt));
    hoverTextTemperature.text(d.Temperatur + "°C");
}

function brushFunction() {
	if (runningBrush){
	}
	  x.domain(brush.empty() ? x2.domain() : brush.extent());
	  graphMain.select("path").attr("d", line);
	  graphMain.select(".x.axis").call(xAxis);
	  runningBrush = false;
}

function calculateTemperature(rawTemp){
	var temp = rawTemp / 1023.0 * thermoRefVoltage;
	temp = thermoRangeMin + (temp * (thermoRangeMax - thermoRangeMin));
	return temp.toFixed(1);
}

var tempClick = function(){
	d3.tsv("getCurrentDatum.php", function(error, singleDatum) {
		
		singleDatum.forEach(function(d) {
			d.Zeitpunkt = parseDate(d.Zeitpunkt);
			d.Temperatur = +calculateTemperature(d.Temperatur);
		});
		
		//add new data to array
		graphData.push(singleDatum[0]);
		
		//rescale
		var yRange = d3.extent(graphData, function(d) { return d.Temperatur; });
		x2.domain(d3.extent(graphData, function(d) { return d.Zeitpunkt; }));
		y2.domain([yRange[0] - temperatureMargin,yRange[1] + temperatureMargin]);
		y.domain(y2.domain()); //set main window y scale to brush window scale
		
		//so vielleicht?
		if (runningBrush){
			var test3 = d3.time.minute.offset(new Date(), -10);
			var test4 = d3.time.minute.offset(new Date(), 0);
			brush.extent([test3,test4]);
			graphBrush.select(".brush").call(brush);
			x.domain(brush.extent()); //change window of main graph
		}
		
		//draw new graphs
		graphMain.select("path").attr("d", line);
		graphMain.select(".x.axis").call(xAxis);
		graphBrush.select("path").attr("d", line2);
		graphBrush.select(".x.axis").call(xAxis2);
		
		//hier weiter
		currentTemperatureText.text(singleDatum[0].Temperatur + "°C");
	});
	return;
}
