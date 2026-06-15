/**************************************************************************************************
  Description: Read-only Alpaca Switch device for auxiliary thermal/lux sensors.
**************************************************************************************************/
#pragma once

#include "SensorInterfaces.h"

#include <AlpacaSwitch.h>

class AuxSensorSwitch : public AlpacaSwitch
{
public:
  enum Channel : uint32_t
  {
    kObjectTemperatureC = 0,
    kAmbientTemperatureC = 1,
    kSkyBrightnessLux = 2,
    kMaxChannels = 3
  };

  AuxSensorSwitch();
  void Begin(TwoWire *wire = &Wire);
  void Loop();

  void AlpacaReadJson(JsonObject &root);
  void AlpacaWriteJson(JsonObject &root);
  bool GetMqttHeartbeatJson(char *buffer, size_t buffer_size) const;
  const char *GetMqttHost() const { return _mqtt_host.c_str(); }
  uint16_t GetMqttPort() const { return _mqtt_port; }
  const char *GetMqttUser() const { return _mqtt_user.c_str(); }
  const char *GetMqttPassword() const { return _mqtt_pwd.c_str(); }
  const char *GetMqttHealthTopic() const { return _mqtt_health_topic.c_str(); }
  const char *GetMqttFunctionTopic() const { return _mqtt_function_topic.c_str(); }

private:
  MLX90614TemperatureSensor _temperature_sensor;
  TSL2591LuxSensor _lux_sensor;

  TwoWire *_wire = &Wire;
  uint32_t _last_update_ms = 0;
  uint32_t _refresh_interval_ms = 1000;

  String _mqtt_host;
  uint16_t _mqtt_port = 1883;
  String _mqtt_user;
  String _mqtt_pwd;
  String _mqtt_health_topic = "observatory/heartbeat";
  String _mqtt_function_topic = "switch/0/status";

  void _refresh(bool force = false);
  void _setChannel(uint32_t id, double value);

  const bool _writeSwitchValue(uint32_t id, double value, SwitchAsyncType_t async_type);

  const bool _putAction(const char *const action, const char *const parameters, char *string_response, size_t string_response_size);
  const bool _putCommandBlind(const char *const command, const char *const raw, bool &bool_response) { return false; }
  const bool _putCommandBool(const char *const command, const char *const raw, bool &bool_response) { return false; }
  const bool _putCommandString(const char *const command_str, const char *const raw, char *string_response, size_t string_response_size) { return false; }
};
