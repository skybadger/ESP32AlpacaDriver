/**************************************************************************************************
  Description: ASCOM Alpaca Dome device using a shared I2C motor controller.
**************************************************************************************************/
#pragma once

#include <AlpacaDevice.h>
#include <Wire.h>

#include "I2CMotor.h"

class Dome : public AlpacaDevice
{
private:
  enum DomeState : uint8_t
  {
    DOME_IDLE = 0,
    DOME_SLEWING = 1,
    DOME_ABORT = 2
  };

  enum ShutterState : int32_t
  {
    SHUTTER_OPEN = 0,
    SHUTTER_CLOSED = 1,
    SHUTTER_OPENING = 2,
    SHUTTER_CLOSING = 3,
    SHUTTER_ERROR = 4
  };

  static constexpr uint8_t kDefaultSdaPin = 21;
  static constexpr uint8_t kDefaultSclPin = 22;
  static constexpr uint32_t kDefaultI2cClockHz = 100000;

  I2CMotor _motor;
  bool _motor_present = false;

  uint8_t _sda_pin = kDefaultSdaPin;
  uint8_t _scl_pin = kDefaultSclPin;
  uint32_t _i2c_clock_hz = kDefaultI2cClockHz;
  uint8_t _motor_address = I2CMotor::kDefaultAddress;

  float _current_azimuth = 0.0f;
  float _target_azimuth = 0.0f;
  float _sync_offset = 0.0f;
  float _acceptable_azimuth_error = 1.0f;
  float _slow_azimuth_range = 10.0f;
  float _simulated_slew_step_deg = 1.5f;
  uint32_t _encoder_poll_interval_ms = 500;
  uint32_t _shutter_poll_interval_ms = 1000;
  uint16_t _rest_timeout_ms = 250;
  uint32_t _last_encoder_poll_ms = 0;
  uint32_t _last_shutter_poll_ms = 0;

  int32_t _home_position = 180;
  int32_t _park_position = 356;
  uint8_t _slow_slew_speed = I2CMotor::kSpeedSlowSlew;
  uint8_t _fast_slew_speed = I2CMotor::kSpeedFastSlew;

  bool _can_find_home = true;
  bool _can_park = true;
  bool _can_set_azimuth = true;
  bool _can_set_park = true;
  bool _can_set_shutter = true;
  bool _can_slave = false;
  bool _can_sync_azimuth = true;
  bool _slaved = false;

  DomeState _dome_state = DOME_IDLE;
  ShutterState _shutter_state = SHUTTER_CLOSED;

  String _encoder_host;
  uint16_t _encoder_port = 80;
  String _shutter_host;
  uint16_t _shutter_port = 80;
  bool _encoder_read_ok = false;
  bool _shutter_read_ok = false;
  int32_t _last_encoder_http_code = 0;
  int32_t _last_shutter_http_code = 0;

  String _mqtt_host;
  uint16_t _mqtt_port = 1883;
  String _mqtt_user;
  String _mqtt_pwd;
  String _mqtt_health_topic = "observatory/heartbeat";
  String _mqtt_function_topic = "dome/0/status";

  uint32_t _checkedClient(AsyncWebServerRequest *request, Spelling_t spelling = Spelling_t::kIgnoreCase);
  void _respondNoValue(AsyncWebServerRequest *request);
  void _respondBool(AsyncWebServerRequest *request, bool value);
  void _respondInt(AsyncWebServerRequest *request, int32_t value);
  void _respondDouble(AsyncWebServerRequest *request, double value);
  void _respondInvalidValue(AsyncWebServerRequest *request, const char *parameter);
  void _respondNotImplemented(AsyncWebServerRequest *request, const char *command);

  float _normaliseAzimuth(float azimuth) const;
  float _shortestDelta(float target, float current) const;
  bool _isAtAzimuth(float azimuth) const;
  void _stopMotor();
  void _startSlew(float azimuth);
  void _updateSlew();
  void _updateRemoteEncoder(bool force = false);
  void _updateRemoteShutter(bool force = false);
  String _remoteUrl(const String &host, uint16_t port, const char *path) const;
  bool _httpGetJson(const String &host, uint16_t port, const char *path, JsonDocument &doc, int32_t &http_code);
  bool _httpPutForm(const String &host, uint16_t port, const char *path, const char *form, int32_t &http_code);
  bool _sendShutterCommand(const char *command);

  void _getAltitude(AsyncWebServerRequest *request);
  void _getAtHome(AsyncWebServerRequest *request);
  void _getAtPark(AsyncWebServerRequest *request);
  void _getAzimuth(AsyncWebServerRequest *request);
  void _getCanFindHome(AsyncWebServerRequest *request);
  void _getCanPark(AsyncWebServerRequest *request);
  void _getCanSetAltitude(AsyncWebServerRequest *request);
  void _getCanSetAzimuth(AsyncWebServerRequest *request);
  void _getCanSetPark(AsyncWebServerRequest *request);
  void _getCanSetShutter(AsyncWebServerRequest *request);
  void _getCanSlave(AsyncWebServerRequest *request);
  void _getCanSyncAzimuth(AsyncWebServerRequest *request);
  void _getShutterStatus(AsyncWebServerRequest *request);
  void _getSlaved(AsyncWebServerRequest *request);
  void _putSlaved(AsyncWebServerRequest *request);
  void _getSlewing(AsyncWebServerRequest *request);

  void _putAbortSlew(AsyncWebServerRequest *request);
  void _putCloseShutter(AsyncWebServerRequest *request);
  void _putFindHome(AsyncWebServerRequest *request);
  void _putOpenShutter(AsyncWebServerRequest *request);
  void _putPark(AsyncWebServerRequest *request);
  void _putSetPark(AsyncWebServerRequest *request);
  void _putSlewToAltitude(AsyncWebServerRequest *request);
  void _putSlewToAzimuth(AsyncWebServerRequest *request);
  void _putSyncToAzimuth(AsyncWebServerRequest *request);
  void AlpacaPutAction(AsyncWebServerRequest *request) override;

  const bool _getDeviceStateList(size_t buf_len, char *buf) override;

public:
  Dome();
  void Begin(uint8_t sda_pin = kDefaultSdaPin, uint8_t scl_pin = kDefaultSclPin);
  void Loop();
  void RegisterCallbacks() override;

  void AlpacaReadJson(JsonObject &root) override;
  void AlpacaWriteJson(JsonObject &root) override;
  bool GetMqttHeartbeatJson(char *buffer, size_t buffer_size) const;
  const char *GetMqttHost() const { return _mqtt_host.c_str(); }
  uint16_t GetMqttPort() const { return _mqtt_port; }
  const char *GetMqttUser() const { return _mqtt_user.c_str(); }
  const char *GetMqttPassword() const { return _mqtt_pwd.c_str(); }
  const char *GetMqttHealthTopic() const { return _mqtt_health_topic.c_str(); }
  const char *GetMqttFunctionTopic() const { return _mqtt_function_topic.c_str(); }
};
