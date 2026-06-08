/**************************************************************************************************
  Description: Internal interfaces for auxiliary I2C sensors.
**************************************************************************************************/
#include "SensorInterfaces.h"

#include <math.h>

bool MLX90614TemperatureSensor::Begin(TwoWire *wire, uint8_t address)
{
  _present = _mlx.begin(address, wire);
  _last_read_ok = false;
  if (_present)
  {
    Read();
  }
  return _present;
}

bool MLX90614TemperatureSensor::Read()
{
  if (!_present)
  {
    _last_read_ok = false;
    return false;
  }

  const double object_temp_c = _mlx.readObjectTempC();
  const double ambient_temp_c = _mlx.readAmbientTempC();
  _last_read_ok = isfinite(object_temp_c) && isfinite(ambient_temp_c);

  if (_last_read_ok)
  {
    _object_temp_c = object_temp_c;
    _ambient_temp_c = ambient_temp_c;
    _last_read_ms = millis();
  }

  return _last_read_ok;
}

const TSL2591LuxSensor::RangeSetting TSL2591LuxSensor::kRangeSettings[] = {
    {TSL2591_GAIN_LOW, TSL2591_INTEGRATIONTIME_100MS, "1x 100ms"},
    {TSL2591_GAIN_MED, TSL2591_INTEGRATIONTIME_100MS, "25x 100ms"},
    {TSL2591_GAIN_HIGH, TSL2591_INTEGRATIONTIME_100MS, "428x 100ms"},
    {TSL2591_GAIN_HIGH, TSL2591_INTEGRATIONTIME_300MS, "428x 300ms"},
    {TSL2591_GAIN_MAX, TSL2591_INTEGRATIONTIME_300MS, "9876x 300ms"},
    {TSL2591_GAIN_MAX, TSL2591_INTEGRATIONTIME_600MS, "9876x 600ms"}};

const uint8_t TSL2591LuxSensor::kRangeSettingCount = sizeof(TSL2591LuxSensor::kRangeSettings) / sizeof(TSL2591LuxSensor::kRangeSettings[0]);

bool TSL2591LuxSensor::Begin(TwoWire *wire)
{
  _present = _tsl.begin(wire);
  _last_read_ok = false;
  if (_present)
  {
    _applyRange();
    Read();
  }
  return _present;
}

bool TSL2591LuxSensor::Read()
{
  if (!_present)
  {
    _last_read_ok = false;
    return false;
  }

  uint16_t full_raw = 0;
  uint16_t ir_raw = 0;
  double lux = 0.0;

  _last_read_ok = _readRaw(full_raw, ir_raw, lux);
  if (!_last_read_ok)
  {
    return false;
  }

  if (_autoRange(full_raw))
  {
    _last_read_ok = _readRaw(full_raw, ir_raw, lux);
    if (!_last_read_ok)
    {
      return false;
    }
  }

  _full_raw = full_raw;
  _ir_raw = ir_raw;
  _lux = lux < 0.0 ? 0.0 : lux;
  _last_read_ms = millis();
  return true;
}

const char *TSL2591LuxSensor::RangeName() const
{
  return kRangeSettings[_range_index].name;
}

void TSL2591LuxSensor::_applyRange()
{
  _tsl.setGain(kRangeSettings[_range_index].gain);
  _tsl.setTiming(kRangeSettings[_range_index].timing);
}

bool TSL2591LuxSensor::_readRaw(uint16_t &full_raw, uint16_t &ir_raw, double &lux)
{
  const uint32_t lum = _tsl.getFullLuminosity();
  ir_raw = static_cast<uint16_t>(lum >> 16);
  full_raw = static_cast<uint16_t>(lum & 0xFFFF);
  lux = _tsl.calculateLux(full_raw, ir_raw);
  return isfinite(lux);
}

bool TSL2591LuxSensor::_autoRange(uint16_t full_raw)
{
  uint8_t new_index = _range_index;

  if (full_raw >= kRawSaturated || full_raw > kRawTargetHigh)
  {
    while (new_index > 0 && full_raw > kRawTargetHigh)
    {
      new_index--;
      break;
    }
  }
  else if (full_raw < kRawTargetLow)
  {
    while (new_index + 1 < kRangeSettingCount && full_raw < kRawTargetLow)
    {
      new_index++;
      break;
    }
  }

  if (new_index == _range_index)
  {
    return false;
  }

  _range_index = new_index;
  _applyRange();
  return true;
}
