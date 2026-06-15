/**************************************************************************************************
  Description:    ASCOM Observing Conditions Device Template
  Copyright 2024-2025 peter_n@gmx.de. All rights reserved.
**************************************************************************************************/
#pragma once
#include <Adafruit_BME280.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_HMC5883_U.h>
#include <Adafruit_HTU21DF.h>
#include <Wire.h>

#include "AlpacaObservingConditions.h"

class ObservingConditions : public AlpacaObservingConditions
{
public:
  ObservingConditions();
  void Begin(uint8_t sda_pin = kDefaultSdaPin, uint8_t scl_pin = kDefaultSclPin);
  void Loop();
  
private:
  static constexpr uint8_t kDefaultSdaPin = 21;
  static constexpr uint8_t kDefaultSclPin = 22;
  static constexpr uint32_t kDefaultI2cClockHz = 100000;
  static constexpr uint8_t kDefaultHtu21dAddress = 0x40;
  static constexpr uint8_t kDefaultBme280Address = 0x76;
  static constexpr uint8_t kDefaultBmp280Address = 0x77;
  static constexpr int32_t kDefaultHmc5883SensorId = 12345;

  Adafruit_HTU21DF _htu21d;
  Adafruit_BME280 _bme280;
  Adafruit_BMP280 _bmp280;
  Adafruit_HMC5883_Unified _hmc5883 = Adafruit_HMC5883_Unified(kDefaultHmc5883SensorId);

  uint8_t _sda_pin = kDefaultSdaPin;
  uint8_t _scl_pin = kDefaultSclPin;
  uint32_t _i2c_clock_hz = kDefaultI2cClockHz;
  uint8_t _htu21d_address = kDefaultHtu21dAddress;
  uint8_t _bme280_address = kDefaultBme280Address;
  uint8_t _bmp280_address = kDefaultBmp280Address;
  double _magnetic_declination_deg = 0.0;

  bool _htu21d_present = false;
  bool _bme280_present = false;
  bool _bmp280_present = false;
  bool _hmc5883_present = false;
  bool _last_read_ok = false;

  uint32_t _update_time_ms = 0;
  uint32_t _refresh_time_ms = 1000;

  // virtual methods
  void _putRefreshRequest() { _update_time_ms = 0; };
  const bool _putAveragePeriodRequest(double average_period);

  // optional Alpaca service: to be implemented if needed
  const bool _putAction(const char *const action, const char *const parameters, char *string_response, size_t string_response_size) { return false; }
  const bool _putCommandBlind(const char *const command, const char *const raw, bool &bool_response) { return false; };
  const bool _putCommandBool(const char *const command, const char *const raw, bool &bool_response) { return false; };
  const bool _putCommandString(const char *const command_str, const char *const raw, char *string_response, size_t string_response_size) { return false; };

  void _refresh();
  double _dewPointC(double temperature_c, double humidity_percent) const;
  double _headingDeg(double x, double y) const;

  void AlpacaReadJson(JsonObject &root);
  void AlpacaWriteJson(JsonObject &root);
};
