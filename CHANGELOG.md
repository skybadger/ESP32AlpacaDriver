# Changelog ESP32AlpacaDevicesDemo2

## 0.0.0 2025-09-06 created / based on ESP32AlpacaDevicesDemo2

## 1.0.0 2025-09-08 
1. main.cpp VERSION 1.0.0


## 1.1.0 2025-09-10
1. Test DeviceState with NINA 3.2 Beta 8
    ASCOM Device State: For drivers that implement the new ASCOM 7 Device State the application will now use the state when possible instead of polling individual fields
2. Usage of ESP32AlpacaDevice2 4.3.0
    




### Investigation: Usage of devicestate during polling by NINA. Test with NINA 3.2 Beta 8

See also (https://ascom-standards.org/newdocs/interfaces.html)

Status 2025-09-08
1. SWITCH: <br>
    - => NOK devicestate not called (tbi)

2. OBSERVING_CONDITIONS: <br>
    - GET connected 
    - GET averageperiod (its not part of devicestate)
    - GET devicestate
    - => OK

3. FOCUSER
    - GET devicestate
    - GET tempcompavailable
    - GET connected

    - Remark: OK, but polling of tempcompavailable which is not part of devicestate

4. Cover Calibrator
    - GET connected
    - GET devicestate
    - GET calibratorstate -> WHY
    - GET calibratorstate -> WHY
    - Remark: OK, but redundant call of calibratorstate (tbi)
    






