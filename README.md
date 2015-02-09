sousVide
========

Use Arduino YUN with attached thermometer and WEMO switch to control a hotplate or use directly connected relais to control heating equipment.

A PID algorithm runs on the Arduino and controls the heating intervals to achieve a +- 0.1°C accuracy.

On the linux side of the YUN, a python script supplies a websocket interface to render a temperature graph on a javascript enabled website. The graph shows a history of the temperatures and the heating intervals and the current temperature. You can also change the settings of the PID parameters on the website.

The python script also writes to a sqlite3 database to store the graph data, as well as the settings.

The Arduino program is able to read temperatures from a connected Greisinger GMH 3210 or a DS18B20. It also displays values on a LCD1602 display module.

Beware: This is a very hardware specific project and is posted here in order to give some inspiration, rather than to enable others to install the scripts on their arduino. If you wish to do so, write me a message and I can give you further information on the hardware used.
