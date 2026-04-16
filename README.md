# ESP32AlpacaDeviceDemo2

Example and test program for the [ESP32AlpacaDevice2](https://github.com/npeter/ESP32AlpacaDevices2) library.
<br>
The project provides an easy way to get started and a first introduction to the library.
At the same time, the project replaces the examples of the library.
<br>
The test was conducted on a
[WEMOS D1 mini3](https://docs.platformio.org/en/stable/boards/espressif32/wemos_d1_mini32.html).

The example should be translatable with few adjustments and able to run on suitable ESP32-based hardware.
<br>

1. **main.c:** Adjust WIFI credentials and remove comment marks
2. **platformio.ini:** Adjust board and monitor_port
<br>
<br>
Before uploading the application, the ESP32's filesystem should be initialized and the data for the webserver provided:
<br>
<br>
1. PROJECT TASKS/"your env"/Platform/**Build Filesystem image**
2. PROJECT TASKS/"your env"/Platform/**Upload Filesystem image**
<br>

As a test client, [Conform Universal](https://github.com/ASCOMInitiative/ConformU) from the [ASCOM Initiative](https://ascom-standards.org/Initiative/Index.htm) can be used.<br>
Tested with NINA 3.2 Beta 8