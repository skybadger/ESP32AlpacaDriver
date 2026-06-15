/**************************************************************************************************
  Description: Read-only Alpaca Switch device for auxiliary thermal/lux sensors.
**************************************************************************************************/
#include "AuxSensorSwitch.h"
#include "RuntimeSettings.h"

namespace
{
const char kFirmwareVersion[] = "1.0";
}

AuxSensorSwitch::AuxSensorSwitch() : AlpacaSwitch(Channel::kMaxChannels)
{
  strlcpy(_device_description, "ESP32-C3 auxiliary thermal and sky brightness sensors", sizeof(_device_description));
  strlcpy(_driver_info, "ASCOM Alpaca read-only Switch driver for MLX90614 and TSL2591", sizeof(_driver_info));
  strlcpy(_device_and_driver_version, kFirmwareVersion, sizeof(_device_and_driver_version));

  _addAction("status");
}

void AuxSensorSwitch::Begin(TwoWire *wire)
{
  _wire = wire;

  InitSwitchInitBySetup(kObjectTemperatureC, false);
  InitSwitchCanWrite(kObjectTemperatureC, false);
  InitSwitchName(kObjectTemperatureC, "MLX90614 Object C");
  InitSwitchDescription(kObjectTemperatureC, "MLX90614 object infrared temperature in degrees C");
  InitSwitchValue(kObjectTemperatureC, 0.0);
  InitSwitchMinValue(kObjectTemperatureC, -70.0);
  InitSwitchMaxValue(kObjectTemperatureC, 380.0);
  InitSwitchStep(kObjectTemperatureC, 0.01);
  InitSwitchCanAsync(kObjectTemperatureC, SwitchAsyncType_t::kNoAsyncType);

  InitSwitchInitBySetup(kAmbientTemperatureC, false);
  InitSwitchCanWrite(kAmbientTemperatureC, false);
  InitSwitchName(kAmbientTemperatureC, "MLX90614 Ambient C");
  InitSwitchDescription(kAmbientTemperatureC, "MLX90614 ambient package temperature in degrees C");
  InitSwitchValue(kAmbientTemperatureC, 0.0);
  InitSwitchMinValue(kAmbientTemperatureC, -40.0);
  InitSwitchMaxValue(kAmbientTemperatureC, 125.0);
  InitSwitchStep(kAmbientTemperatureC, 0.01);
  InitSwitchCanAsync(kAmbientTemperatureC, SwitchAsyncType_t::kNoAsyncType);

  InitSwitchInitBySetup(kSkyBrightnessLux, false);
  InitSwitchCanWrite(kSkyBrightnessLux, false);
  InitSwitchName(kSkyBrightnessLux, "TSL2591 Lux");
  InitSwitchDescription(kSkyBrightnessLux, "TSL2591 auto-ranged sky brightness in lux");
  InitSwitchValue(kSkyBrightnessLux, 0.0);
  InitSwitchMinValue(kSkyBrightnessLux, 0.0);
  InitSwitchMaxValue(kSkyBrightnessLux, 88000.0);
  InitSwitchStep(kSkyBrightnessLux, 0.01);
  InitSwitchCanAsync(kSkyBrightnessLux, SwitchAsyncType_t::kNoAsyncType);

  const bool temp_ok = _temperature_sensor.Begin(_wire);
  const bool lux_ok = _lux_sensor.Begin(_wire);

  SLOG_INFO_PRINTF("AuxSensorSwitch MLX90614=%s TSL2591=%s\n", temp_ok ? "ok" : "missing", lux_ok ? "ok" : "missing");
  _refresh(true);

  AlpacaSwitch::Begin();
}

void AuxSensorSwitch::Loop()
{
  _refresh(false);
}

void AuxSensorSwitch::_refresh(bool force)
{
  const uint32_t now_ms = millis();
  if (!force && (now_ms - _last_update_ms) < _refresh_interval_ms)
  {
    return;
  }
  _last_update_ms = now_ms;

  if (_temperature_sensor.Read())
  {
    _setChannel(kObjectTemperatureC, _temperature_sensor.ObjectTemperatureC());
    _setChannel(kAmbientTemperatureC, _temperature_sensor.AmbientTemperatureC());
  }

  if (_lux_sensor.Read())
  {
    _setChannel(kSkyBrightnessLux, _lux_sensor.Lux());
  }
}

void AuxSensorSwitch::_setChannel(uint32_t id, double value)
{
  if (!SetSwitchValue(id, value))
  {
    SLOG_WARNING_PRINTF("AuxSensorSwitch value out of switch range id=%u value=%f\n", id, value);
  }
}

const bool AuxSensorSwitch::_writeSwitchValue(uint32_t id, double value, SwitchAsyncType_t async_type)
{
  (void)id;
  (void)value;
  (void)async_type;
  return false;
}

const bool AuxSensorSwitch::_putAction(const char *const action, const char *const parameters, char *string_response, size_t string_response_size)
{
  (void)parameters;
  if (strcasecmp(action, "status") != 0)
  {
    return false;
  }

  snprintf(string_response,
           string_response_size,
           "{\"MLX90614Present\":%s,\"MLX90614ReadOk\":%s,\"TSL2591Present\":%s,\"TSL2591ReadOk\":%s,\"TSL2591Range\":\"%s\",\"TSL2591FullRaw\":%u,\"TSL2591IrRaw\":%u}",
           _temperature_sensor.IsPresent() ? "true" : "false",
           _temperature_sensor.LastReadOk() ? "true" : "false",
           _lux_sensor.IsPresent() ? "true" : "false",
           _lux_sensor.LastReadOk() ? "true" : "false",
           _lux_sensor.RangeName(),
           _lux_sensor.FullRaw(),
           _lux_sensor.IrRaw());
  return true;
}

void AuxSensorSwitch::AlpacaReadJson(JsonObject &root)
{
  AlpacaSwitch::AlpacaReadJson(root);
  if (JsonObject obj_config = root["AuxSensorConfiguration"])
  {
    RuntimeSettings::ReadJson(obj_config);
    _refresh_interval_ms = obj_config["RefreshIntervalMs"] | _refresh_interval_ms;
    _mqtt_host = obj_config["MQTTHost"] | _mqtt_host;
    _mqtt_port = obj_config["MQTTPort"] | _mqtt_port;
    _mqtt_user = obj_config["MQTTUser"] | _mqtt_user;
    _mqtt_pwd = obj_config["MQTTPwd"] | _mqtt_pwd;
    _mqtt_health_topic = obj_config["MQTTHealthTopic"] | _mqtt_health_topic;
    _mqtt_function_topic = obj_config["MQTTFunctionTopic"] | _mqtt_function_topic;
  }
}

void AuxSensorSwitch::AlpacaWriteJson(JsonObject &root)
{
  AlpacaSwitch::AlpacaWriteJson(root);
  JsonObject obj_config = root["AuxSensorConfiguration"].to<JsonObject>();
  RuntimeSettings::WriteJson(obj_config);
  obj_config["RefreshIntervalMs"] = _refresh_interval_ms;
  obj_config["MQTTHost"] = _mqtt_host;
  obj_config["MQTTPort"] = _mqtt_port;
  obj_config["MQTTUser"] = _mqtt_user;
  obj_config["MQTTPwd"] = _mqtt_pwd;
  obj_config["MQTTHealthTopic"] = _mqtt_health_topic;
  obj_config["MQTTFunctionTopic"] = _mqtt_function_topic;

  JsonObject obj_states = root["#AuxSensorStates"].to<JsonObject>();
  obj_states["MLX90614Present"] = _temperature_sensor.IsPresent();
  obj_states["MLX90614ReadOk"] = _temperature_sensor.LastReadOk();
  obj_states["TSL2591Present"] = _lux_sensor.IsPresent();
  obj_states["TSL2591ReadOk"] = _lux_sensor.LastReadOk();
  obj_states["TSL2591Range"] = _lux_sensor.RangeName();
  obj_states["TSL2591FullRaw"] = _lux_sensor.FullRaw();
  obj_states["TSL2591IrRaw"] = _lux_sensor.IrRaw();
}

bool AuxSensorSwitch::GetMqttHeartbeatJson(char *buffer, size_t buffer_size) const
{
  const int written = snprintf(buffer,
                               buffer_size,
                               "{\"device\":\"switch\",\"type\":\"AuxSensors\",\"mlx90614Present\":%s,\"mlx90614ReadOk\":%s,\"objectC\":%.2f,\"ambientC\":%.2f,\"tsl2591Present\":%s,\"tsl2591ReadOk\":%s,\"lux\":%.2f,\"tsl2591Range\":\"%s\",\"tsl2591FullRaw\":%u,\"tsl2591IrRaw\":%u}",
                               _temperature_sensor.IsPresent() ? "true" : "false",
                               _temperature_sensor.LastReadOk() ? "true" : "false",
                               _temperature_sensor.ObjectTemperatureC(),
                               _temperature_sensor.AmbientTemperatureC(),
                               _lux_sensor.IsPresent() ? "true" : "false",
                               _lux_sensor.LastReadOk() ? "true" : "false",
                               _lux_sensor.Lux(),
                               _lux_sensor.RangeName(),
                               _lux_sensor.FullRaw(),
                               _lux_sensor.IrRaw());
  return written > 0 && static_cast<size_t>(written) < buffer_size;
}
