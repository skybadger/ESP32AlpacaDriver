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
#define _DEFAULT_NUM_SWITCH_DEVICES 8                             // intial number of swtixches in the switch device  
#define ALPACA_SWITCH_DESCRIPTION "Alpaca Switch Template"        // init value; managed by config
#define ALPACA_SWITCH_DRIVER_INFO "ESP32 Switch driver by BigPet" // init value; managed by config
#define ALPACA_SWITCH_INTERFACE_VERSION 3                         // don't change
#define ALPACA_SWITCH_NAME "Skybadger Switch 3"                   // init with <deviceType>-<deviceNumber>; managed by config
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
#define ALPACA_FOCUSER_DESCRIPTION "Skybadger Focuser"              // init value; managed by config
#define ALPACA_FOCUSER_DRIVER_INFO "ESP32 Focuser driver "          // init value; managed by config
#define ALPACA_FOCUSER_INTERFACE_VERSION 4                          // don't change
#define ALPACA_FOCUSER_NAME "Big Blue collimator"                   // init with <deviceType>-<deviceNumber>; managed by config
#define ALPACA_FOCUSER_DEVICE_TYPE "focuser"                        // don't change


// add your WIFI credentials and uncommend
// #define DEFAULT_SSID "my_ssid"
// #define DEFAULT_PWD "my_pwd"
#define DEFAULT_SSID "BadgerBT"
#define DEFAULT_PWD "ThisIsBadgerBTTrial"
#define SYSLOG_HOST "192.168.0.48" // your SysLog-Host
#define HOSTNAME "ESP32AlpFoc1"

#ifndef DEFAULT_SSID 
#include "Credentials.h"
#endif

//time local settings 
#define TZ              0       // (utc+) TZ in hours
#define DST_MN          60      // use 60mn for summer time in some countries
#define TZ_MN           ((TZ)*60)
#define TZ_SEC          ((TZ)*3600)
#define DST_SEC         ((DST_MN)*60)

static const char timeServer1[] = "pool.ntp.org";
static const char timeServer2[] = "time.nist.gov";
static const char timeServer3[] = "time.google.com";

// MQTT defaults. These are copied into each Alpaca device's setup JSON and can be
// changed from the setup UI, then saved to LittleFS settings.
#define MQTT_DEFAULT_ENABLED true
#define MQTT_DEFAULT_HOST "192.168.0.48"
#define MQTT_DEFAULT_PORT 1883
#define MQTT_DEFAULT_USER ""
#define MQTT_DEFAULT_PASSWORD ""
#define MQTT_DEFAULT_HEARTBEAT_TOPIC "skybadger/heartbeat"
#define MQTT_DEFAULT_STATUS_TOPIC "skybadger/status"
#define MQTT_DEFAULT_FUNCTION_TOPIC "skybadger/driver/function"
