/**************************************************************************************************
  Description: Internal interfaces for auxiliary I2C sensors.
**************************************************************************************************/
#pragma once

#include <Adafruit_MLX90614.h>
#include <Adafruit_TSL2591.h>
#include <Arduino.h>
#include <Wire.h>

class MLX90614TemperatureSensor
{
public:
  bool Begin(TwoWire *wire = &Wire, uint8_t address = 0x5A);
  bool Read();

  bool IsPresent() const { return _present; }
  bool LastReadOk() const { return _last_read_ok; }
  double ObjectTemperatureC() const { return _object_temp_c; }
  double AmbientTemperatureC() const { return _ambient_temp_c; }
  uint32_t LastReadMs() const { return _last_read_ms; }

private:
  Adafruit_MLX90614 _mlx = Adafruit_MLX90614();
  bool _present = false;
  bool _last_read_ok = false;
  double _object_temp_c = 0.0;
  double _ambient_temp_c = 0.0;
  uint32_t _last_read_ms = 0;
};

class TSL2591LuxSensor
{
public:
  bool Begin(TwoWire *wire = &Wire);
  bool Read();

  bool IsPresent() const { return _present; }
  bool LastReadOk() const { return _last_read_ok; }
  double Lux() const { return _lux; }
  uint16_t FullRaw() const { return _full_raw; }
  uint16_t IrRaw() const { return _ir_raw; }
  uint8_t RangeIndex() const { return _range_index; }
  const char *RangeName() const;
  uint32_t LastReadMs() const { return _last_read_ms; }

private:
  struct RangeSetting
  {
    tsl2591Gain_t gain;
    tsl2591IntegrationTime_t timing;
    const char *name;
  };

  static constexpr uint16_t kRawTargetLow = 18000;
  static constexpr uint16_t kRawTargetHigh = 43000;
  static constexpr uint16_t kRawSaturated = 65000;

  static const RangeSetting kRangeSettings[];
  static const uint8_t kRangeSettingCount;

  Adafruit_TSL2591 _tsl = Adafruit_TSL2591(2591);
  bool _present = false;
  bool _last_read_ok = false;
  double _lux = 0.0;
  uint16_t _full_raw = 0;
  uint16_t _ir_raw = 0;
  uint8_t _range_index = 1;
  uint32_t _last_read_ms = 0;

  void _applyRange();
  bool _readRaw(uint16_t &full_raw, uint16_t &ir_raw, double &lux);
  bool _autoRange(uint16_t full_raw);
};
