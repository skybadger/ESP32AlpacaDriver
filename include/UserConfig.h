#pragma once
// AscomDriver Common Properties:
// - Description: A description of the device, such as manufacturer and model number. Any ASCII characters may be used.
// - DriverInfo: Descriptive and version information about this ASCOM driver
// - DriverVersion: A string containing only the major and minor version of the driver
// - InterfaceVersion: The interface version number that this device supports
// - Name: The short name of the driver, for display purposes
// DeviceType - as defined ASCOM

// =======================================================================================================
// CoverCalibrator - Comon Properties
#define ALPACA_COVER_CALIBRATOR_DESCRIPTION "Alpaca CoverCalibrator Template"        // init value; managed by config
#define ALPACA_COVER_CALIBRATOR_DRIVER_INFO "ESP32 CoverCalibrator driver by BigPet" // init value; managed by config
#define ALPACA_COVER_CALIBRATOR_INTERFACE_VERSION 2                                 // don't change
#define ALPACA_COVER_CALIBRATOR_NAME "not used"                                      // init with <deviceType>-<deviceNumber>; managed by config
#define ALPACA_COVER_CALIBRATOR_DEVICE_TYPE "covercalibrator"                        // don't change

// CoverCalibrator - Specific Properties
#define ALPACA_COVER_CALIBRATOR_MAX_BRIGHTNESS 1023 // init; managed by setup

// =======================================================================================================
// Switch - Comon Properties
#define ALPACA_SWITCH_DESCRIPTION "Alpaca Switch Template"        // init value; managed by config
#define ALPACA_SWITCH_DRIVER_INFO "ESP32 Switch driver by BigPet" // init value; managed by config
#define ALPACA_SWITCH_INTERFACE_VERSION 3                         // don't change
#define ALPACA_SWITCH_NAME "not used"                             // init with <deviceType>-<deviceNumber>; managed by config
#define ALPACA_SWITCH_DEVICE_TYPE "switch"                        // don't change

// Switch - Specific Properties
// empty

// =======================================================================================================
// ObservingConditions - Common Properties
#define ALPACA_OBSERVING_CONDITIONS_DESCRIPTION "Alpaca ObservingConditions Template"        // init value; managed by config
#define ALPACA_OBSERVING_CONDITIONS_DRIVER_INFO "ESP32 ObservingConditions driver by BigPet" // init value; managed by config
#define ALPACA_OBSERVING_CONDITIONS_INTERFACE_VERSION 2                                      // don't change
#define ALPACA_OBSERVING_CONDITIONS_NAME "not used"                                          // init with <deviceType>-<deviceNumber>; managed by config
#define ALPACA_OBSERVING_CONDITIONS_DEVICE_TYPE "observingconditions"                        // don't change


// =======================================================================================================
// Focuser - Comon Properties
#define ALPACA_FOCUSER_DESCRIPTION "Skybadger Focuser"        // init value; managed by config
#define ALPACA_FOCUSER_DRIVER_INFO "ESP32 Focuser driver " // init value; managed by config
#define ALPACA_FOCUSER_INTERFACE_VERSION 4                          // don't change
#define ALPACA_FOCUSER_NAME "not used"                              // init with <deviceType>-<deviceNumber>; managed by config
#define ALPACA_FOCUSER_DEVICE_TYPE "focuser"                        // don't change


// add your WIFI credentials and uncommend
// #define DEFAULT_SSID "my_ssid"
// #define DEFAULT_PWD "my_pwd"
#define DEFAULT_SSID "BadgerBT"
#define DEFAULT_PWD "ThisIsBadgerBTTrial"
#define SYSLOG_HOST "192.168.0.48" // your SysLog-Host
#define HOSTNAME "ESP32AlpFoc1"

#define WIFI_CONNECT_TIMEOUT_MS 20000
#define WIFI_AP_FALLBACK_SSID HOSTNAME "-setup"
#define WIFI_AP_FALLBACK_PWD "alpaca-setup"

#define DEFAULT_NTP_SERVER_1 "pool.ntp.org"
#define DEFAULT_NTP_SERVER_2 "time.nist.gov"
#define DEFAULT_NTP_SERVER_3 "time.google.com"
#define DEFAULT_TIMEZONE_NAME "Europe/London"
#define DEFAULT_TIMEZONE_POSIX "GMT0BST,M3.5.0/1,M10.5.0"

#ifndef DEFAULT_SSID 
#include "Credentials.h"
#endif
