# ESP32AlpacaDeviceDemo2

Beispiel- und Testprogramm für die [ESP32AlpacaDevice2](https://github.com/npeter/ESP32AlpacaDevices2) Bibliothek.
<br>
Das Projekt ermöglicht auf einfache Weise einen Einstieg und erstes Kennenlernen der Bibliothek.
Gleichzeitig ersetzt das Projekt die Beispiele der Bibliothek.
<br>
Der Test erfolgte auf einen 
[WEMOS D1 mini3](https://docs.platformio.org/en/stable/boards/espressif32/wemos_d1_mini32.html).

Das Beispiel sollte mit wenigen Anpassungen übersetzbar und auf einer geeignete ESP32 basierende Hardware ablaufen können.
<br>

1. **main.c:**  WIFI credentials anpassen und Kommentarzeichen entfernen
2. **platformio.ini:** board und monitor_port anpassen
<br>
<br>
Vor dem Upload der Anwendung sollte das Filesystem des ESP32 initialisiert und die Daten für den Webserver bereitgestellt werden:
<br>
<br>
1. PROJECT TASKS/\"your env\"/Platform/**Build Filesystem image**
2. PROJECT TASKS/"your env"/Platform/**Upload Filesystem image**
<br>
Als Testclient kann [Conform Universal](https://github.com/ASCOMInitiative/ConformU) der [ASCOM Initiative](https://ascom-standards.org/Initiative/Index.htm) verwendet werden.










