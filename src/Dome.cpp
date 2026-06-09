/**************************************************************************************************
  Description: ASCOM Alpaca Dome device using a shared I2C motor controller.
**************************************************************************************************/
#include "Dome.h"

#include <HTTPClient.h>
#include <WiFiClient.h>
#include <math.h>

namespace
{
const char kDriverVersion[] = "1.0";
}

Dome::Dome()
{
  strlcpy(_device_type, "dome", sizeof(_device_type));
  strlcpy(_device_description, "ESP32 ASCOM Alpaca dome controller", sizeof(_device_description));
  strlcpy(_driver_info, "ASCOM Alpaca Dome driver using shared ESP32 motor control", sizeof(_driver_info));
  strlcpy(_device_and_driver_version, kDriverVersion, sizeof(_device_and_driver_version));
  _device_interface_version = 2;

  _addAction("status");
}

void Dome::Begin(uint8_t sda_pin, uint8_t scl_pin)
{
  _sda_pin = sda_pin;
  _scl_pin = scl_pin;

  Wire.begin(_sda_pin, _scl_pin);
  Wire.setClock(_i2c_clock_hz);

  _motor.configure(_motor_address, &Wire);
  _motor_present = _motor.begin();
  if (!_motor_present)
  {
    SLOG_WARNING_PRINTF("Dome I2C motor not detected on address=0x%02X SDA=%u SCL=%u\n", _motor_address, _sda_pin, _scl_pin);
  }

  AlpacaDevice::Begin();
}

void Dome::Loop()
{
  _updateRemoteEncoder();
  _updateSlew();
  _updateRemoteShutter();
}

void Dome::RegisterCallbacks()
{
  AlpacaDevice::RegisterCallbacks();

  createCallBack(LHF(AlpacaPutAction), HTTP_PUT, "action");
  createCallBack(LHF(_getAltitude), HTTP_GET, "altitude");
  createCallBack(LHF(_getAtHome), HTTP_GET, "athome");
  createCallBack(LHF(_getAtPark), HTTP_GET, "atpark");
  createCallBack(LHF(_getAzimuth), HTTP_GET, "azimuth");
  createCallBack(LHF(_getCanFindHome), HTTP_GET, "canfindhome");
  createCallBack(LHF(_getCanPark), HTTP_GET, "canpark");
  createCallBack(LHF(_getCanSetAltitude), HTTP_GET, "cansetaltitude");
  createCallBack(LHF(_getCanSetAzimuth), HTTP_GET, "cansetazimuth");
  createCallBack(LHF(_getCanSetPark), HTTP_GET, "cansetpark");
  createCallBack(LHF(_getCanSetShutter), HTTP_GET, "cansetshutter");
  createCallBack(LHF(_getCanSlave), HTTP_GET, "canslave");
  createCallBack(LHF(_getCanSyncAzimuth), HTTP_GET, "cansyncazimuth");
  createCallBack(LHF(_getShutterStatus), HTTP_GET, "shutterstatus");
  createCallBack(LHF(_getSlaved), HTTP_GET, "slaved");
  createCallBack(LHF(_putSlaved), HTTP_PUT, "slaved");
  createCallBack(LHF(_getSlewing), HTTP_GET, "slewing");

  createCallBack(LHF(_putAbortSlew), HTTP_PUT, "abortslew");
  createCallBack(LHF(_putCloseShutter), HTTP_PUT, "closeshutter");
  createCallBack(LHF(_putFindHome), HTTP_PUT, "findhome");
  createCallBack(LHF(_putOpenShutter), HTTP_PUT, "openshutter");
  createCallBack(LHF(_putPark), HTTP_PUT, "park");
  createCallBack(LHF(_putSetPark), HTTP_PUT, "setpark");
  createCallBack(LHF(_putSlewToAltitude), HTTP_PUT, "slewtoaltitude");
  createCallBack(LHF(_putSlewToAzimuth), HTTP_PUT, "slewtoazimuth");
  createCallBack(LHF(_putSyncToAzimuth), HTTP_PUT, "synctoazimuth");
}

uint32_t Dome::_checkedClient(AsyncWebServerRequest *request, Spelling_t spelling)
{
  _service_counter++;
  uint32_t client_idx = 0;
  return checkClientDataAndConnection(request, client_idx, spelling);
}

void Dome::_respondNoValue(AsyncWebServerRequest *request)
{
  const uint32_t client_idx = _checkedClient(request);
  _alpaca_server->Respond(request, _clients[client_idx], _rsp_status);
}

void Dome::_respondBool(AsyncWebServerRequest *request, bool value)
{
  const uint32_t client_idx = _checkedClient(request);
  _alpaca_server->Respond(request, _clients[client_idx], _rsp_status, value);
}

void Dome::_respondInt(AsyncWebServerRequest *request, int32_t value)
{
  const uint32_t client_idx = _checkedClient(request);
  _alpaca_server->Respond(request, _clients[client_idx], _rsp_status, value);
}

void Dome::_respondDouble(AsyncWebServerRequest *request, double value)
{
  const uint32_t client_idx = _checkedClient(request);
  _alpaca_server->Respond(request, _clients[client_idx], _rsp_status, value);
}

void Dome::_respondInvalidValue(AsyncWebServerRequest *request, const char *parameter)
{
  const uint32_t client_idx = _checkedClient(request);
  _rsp_status.error_code = AlpacaErrorCode_t::InvalidValue;
  snprintf(_rsp_status.error_msg, sizeof(_rsp_status.error_msg), "%s - Parameter '%s' not found or invalid", request->url().c_str(), parameter);
  _alpaca_server->Respond(request, _clients[client_idx], _rsp_status);
}

void Dome::_respondNotImplemented(AsyncWebServerRequest *request, const char *command)
{
  const uint32_t client_idx = _checkedClient(request);
  _rsp_status.error_code = AlpacaErrorCode_t::NotImplemented;
  snprintf(_rsp_status.error_msg, sizeof(_rsp_status.error_msg), "%s - Command '%s' not implemented", request->url().c_str(), command);
  _alpaca_server->Respond(request, _clients[client_idx], _rsp_status);
}

float Dome::_normaliseAzimuth(float azimuth) const
{
  while (azimuth < 0.0f)
  {
    azimuth += 360.0f;
  }
  while (azimuth >= 360.0f)
  {
    azimuth -= 360.0f;
  }
  return azimuth;
}

float Dome::_shortestDelta(float target, float current) const
{
  return fmodf((target - current) + 540.0f, 360.0f) - 180.0f;
}

bool Dome::_isAtAzimuth(float azimuth) const
{
  return fabsf(_shortestDelta(_normaliseAzimuth(azimuth - _sync_offset), _current_azimuth)) <= _acceptable_azimuth_error;
}

String Dome::_remoteUrl(const String &host, uint16_t port, const char *path) const
{
  if (host.isEmpty())
  {
    return "";
  }

  String url = host;
  if (!url.startsWith("http://") && !url.startsWith("https://"))
  {
    url = "http://" + url;
  }

  const int scheme_end = url.indexOf("://");
  const int host_start = scheme_end >= 0 ? scheme_end + 3 : 0;
  const int path_start = url.indexOf('/', host_start);
  const bool has_path = path_start >= 0;
  const String base = has_path ? url.substring(0, path_start) : url;
  String suffix = has_path ? url.substring(path_start) : "";
  const bool has_explicit_port = base.indexOf(':', host_start) >= 0;

  url = base;
  if (!has_explicit_port && port > 0 && port != 80)
  {
    url += ":";
    url += port;
  }

  if (suffix.endsWith("/"))
  {
    suffix.remove(suffix.length() - 1);
  }
  url += suffix;

  if (path != nullptr && path[0] != '\0')
  {
    if (url.endsWith("/") && path[0] == '/')
    {
      url.remove(url.length() - 1);
    }
    else if (!url.endsWith("/") && path[0] != '/')
    {
      url += "/";
    }
    url += path;
  }

  return url;
}

bool Dome::_httpGetJson(const String &host, uint16_t port, const char *path, JsonDocument &doc, int32_t &http_code)
{
  http_code = 0;
  const String url = _remoteUrl(host, port, path);
  if (url.isEmpty())
  {
    return false;
  }

  WiFiClient client;
  HTTPClient http;
  http.setTimeout(_rest_timeout_ms);
  http.setReuse(true);

  if (!http.begin(client, url))
  {
    http.end();
    return false;
  }

  http_code = http.GET();
  if (http_code == HTTP_CODE_OK || http_code == HTTP_CODE_MOVED_PERMANENTLY)
  {
    const String response = http.getString();
    const DeserializationError error = deserializeJson(doc, response);
    http.end();
    return !error;
  }

  http.end();
  return false;
}

bool Dome::_httpPutForm(const String &host, uint16_t port, const char *path, const char *form, int32_t &http_code)
{
  http_code = 0;
  const String url = _remoteUrl(host, port, path);
  if (url.isEmpty())
  {
    return false;
  }

  WiFiClient client;
  HTTPClient http;
  http.setTimeout(_rest_timeout_ms);
  http.setReuse(true);

  if (!http.begin(client, url))
  {
    http.end();
    return false;
  }

  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  http_code = http.PUT(form);
  http.end();
  return http_code == HTTP_CODE_OK || http_code == HTTP_CODE_MOVED_PERMANENTLY;
}

void Dome::_updateRemoteEncoder(bool force)
{
  if (_encoder_host.isEmpty())
  {
    return;
  }

  const uint32_t now_ms = millis();
  if (!force && now_ms - _last_encoder_poll_ms < _encoder_poll_interval_ms)
  {
    return;
  }
  _last_encoder_poll_ms = now_ms;

  JsonDocument doc;
  int32_t http_code = 0;
  const bool ok = _httpGetJson(_encoder_host, _encoder_port, "/bearing", doc, http_code);
  _last_encoder_http_code = http_code;
  if (ok && doc["bearing"].is<float>())
  {
    const float bearing = doc["bearing"].as<float>();
    _current_azimuth = _normaliseAzimuth(bearing);
    _encoder_read_ok = true;
    return;
  }

  _encoder_read_ok = false;
}

void Dome::_updateRemoteShutter(bool force)
{
  if (_shutter_host.isEmpty())
  {
    return;
  }

  const uint32_t now_ms = millis();
  if (!force && now_ms - _last_shutter_poll_ms < _shutter_poll_interval_ms)
  {
    return;
  }
  _last_shutter_poll_ms = now_ms;

  JsonDocument doc;
  int32_t http_code = 0;
  const bool ok = _httpGetJson(_shutter_host, _shutter_port, "/status", doc, http_code);
  _last_shutter_http_code = http_code;
  if (ok && doc["status"].is<int>())
  {
    const int32_t status = doc["status"].as<int32_t>();
    if (status >= SHUTTER_OPEN && status <= SHUTTER_ERROR)
    {
      _shutter_state = static_cast<ShutterState>(status);
      _shutter_read_ok = true;
      return;
    }
  }

  _shutter_read_ok = false;
}

bool Dome::_sendShutterCommand(const char *command)
{
  if (_shutter_host.isEmpty())
  {
    return false;
  }

  char form[32] = {0};
  snprintf(form, sizeof(form), "shutter=%s", command);
  int32_t http_code = 0;
  const bool ok = _httpPutForm(_shutter_host, _shutter_port, "/shutter", form, http_code);
  _last_shutter_http_code = http_code;
  _shutter_read_ok = ok;
  return ok;
}

void Dome::_stopMotor()
{
  _motor.disableMotor();
  _dome_state = DOME_IDLE;
}

void Dome::_startSlew(float azimuth)
{
  _target_azimuth = _normaliseAzimuth(azimuth - _sync_offset);
  if (_isAtAzimuth(azimuth))
  {
    _current_azimuth = _target_azimuth;
    _stopMotor();
    return;
  }
  _dome_state = DOME_SLEWING;
}

void Dome::_updateSlew()
{
  if (_dome_state != DOME_SLEWING)
  {
    return;
  }

  const float delta = _shortestDelta(_target_azimuth, _current_azimuth);
  const float abs_delta = fabsf(delta);
  if (abs_delta <= _acceptable_azimuth_error)
  {
    _current_azimuth = _target_azimuth;
    _stopMotor();
    return;
  }

  const uint8_t speed = abs_delta <= _slow_azimuth_range ? _slow_slew_speed : _fast_slew_speed;
  const uint8_t direction = delta >= 0.0f ? Motor::DIRECTION_CW : Motor::DIRECTION_CCW;
  _motor_present = _motor.setSpeedDirection(speed, direction);

  if (_encoder_host.isEmpty())
  {
    const float step = min(abs_delta, _simulated_slew_step_deg);
    _current_azimuth = _normaliseAzimuth(_current_azimuth + (delta >= 0.0f ? step : -step));
  }
}

void Dome::_getAltitude(AsyncWebServerRequest *request) { _respondNotImplemented(request, "altitude"); }
void Dome::_getAtHome(AsyncWebServerRequest *request) { _respondBool(request, _isAtAzimuth(static_cast<float>(_home_position))); }
void Dome::_getAtPark(AsyncWebServerRequest *request) { _respondBool(request, _isAtAzimuth(static_cast<float>(_park_position))); }
void Dome::_getAzimuth(AsyncWebServerRequest *request) { _respondDouble(request, _normaliseAzimuth(_current_azimuth + _sync_offset)); }
void Dome::_getCanFindHome(AsyncWebServerRequest *request) { _respondBool(request, _can_find_home); }
void Dome::_getCanPark(AsyncWebServerRequest *request) { _respondBool(request, _can_park); }
void Dome::_getCanSetAltitude(AsyncWebServerRequest *request) { _respondBool(request, false); }
void Dome::_getCanSetAzimuth(AsyncWebServerRequest *request) { _respondBool(request, _can_set_azimuth); }
void Dome::_getCanSetPark(AsyncWebServerRequest *request) { _respondBool(request, _can_set_park); }
void Dome::_getCanSetShutter(AsyncWebServerRequest *request) { _respondBool(request, _can_set_shutter); }
void Dome::_getCanSlave(AsyncWebServerRequest *request) { _respondBool(request, _can_slave); }
void Dome::_getCanSyncAzimuth(AsyncWebServerRequest *request) { _respondBool(request, _can_sync_azimuth); }
void Dome::_getShutterStatus(AsyncWebServerRequest *request) { _respondInt(request, static_cast<int32_t>(_shutter_state)); }
void Dome::_getSlaved(AsyncWebServerRequest *request) { _respondBool(request, _slaved); }
void Dome::_getSlewing(AsyncWebServerRequest *request) { _respondBool(request, _dome_state == DOME_SLEWING); }

void Dome::_putSlaved(AsyncWebServerRequest *request)
{
  const uint32_t client_idx = _checkedClient(request, Spelling_t::kStrict);
  bool value = false;
  if (!_alpaca_server->GetParam(request, "Slaved", value, Spelling_t::kStrict))
  {
    _rsp_status.error_code = AlpacaErrorCode_t::InvalidValue;
    snprintf(_rsp_status.error_msg, sizeof(_rsp_status.error_msg), "%s - Parameter 'Slaved' not found", request->url().c_str());
  }
  else if (!_can_slave && value)
  {
    _rsp_status.error_code = AlpacaErrorCode_t::NotImplemented;
    snprintf(_rsp_status.error_msg, sizeof(_rsp_status.error_msg), "%s - Slaving is not implemented", request->url().c_str());
  }
  else
  {
    _slaved = value;
  }
  _alpaca_server->Respond(request, _clients[client_idx], _rsp_status);
}

void Dome::_putAbortSlew(AsyncWebServerRequest *request)
{
  _dome_state = DOME_ABORT;
  _stopMotor();
  if (!_shutter_host.isEmpty())
  {
    _sendShutterCommand("abort");
    _updateRemoteShutter(true);
  }
  _respondNoValue(request);
}

void Dome::_putCloseShutter(AsyncWebServerRequest *request)
{
  if (!_can_set_shutter)
  {
    _respondNotImplemented(request, "closeshutter");
    return;
  }
  if (_shutter_host.isEmpty())
  {
    _shutter_state = SHUTTER_CLOSED;
  }
  else if (_sendShutterCommand("close"))
  {
    _shutter_state = SHUTTER_CLOSING;
  }
  _respondNoValue(request);
}

void Dome::_putFindHome(AsyncWebServerRequest *request)
{
  if (!_can_find_home)
  {
    _respondNotImplemented(request, "findhome");
    return;
  }
  _startSlew(static_cast<float>(_home_position));
  _respondNoValue(request);
}

void Dome::_putOpenShutter(AsyncWebServerRequest *request)
{
  if (!_can_set_shutter)
  {
    _respondNotImplemented(request, "openshutter");
    return;
  }
  if (_shutter_host.isEmpty())
  {
    _shutter_state = SHUTTER_OPEN;
  }
  else if (_sendShutterCommand("open"))
  {
    _shutter_state = SHUTTER_OPENING;
  }
  _respondNoValue(request);
}

void Dome::_putPark(AsyncWebServerRequest *request)
{
  if (!_can_park)
  {
    _respondNotImplemented(request, "park");
    return;
  }
  _startSlew(static_cast<float>(_park_position));
  _respondNoValue(request);
}

void Dome::_putSetPark(AsyncWebServerRequest *request)
{
  if (!_can_set_park)
  {
    _respondNotImplemented(request, "setpark");
    return;
  }
  _park_position = static_cast<int32_t>(_normaliseAzimuth(_current_azimuth + _sync_offset));
  _respondNoValue(request);
}

void Dome::_putSlewToAltitude(AsyncWebServerRequest *request)
{
  _respondNotImplemented(request, "slewtoaltitude");
}

void Dome::_putSlewToAzimuth(AsyncWebServerRequest *request)
{
  const uint32_t client_idx = _checkedClient(request, Spelling_t::kStrict);
  double azimuth = 0.0;
  if (!_alpaca_server->GetParam(request, "Azimuth", azimuth, Spelling_t::kStrict) || azimuth < 0.0 || azimuth >= 360.0)
  {
    _rsp_status.error_code = AlpacaErrorCode_t::InvalidValue;
    snprintf(_rsp_status.error_msg, sizeof(_rsp_status.error_msg), "%s - Parameter 'Azimuth' not found or invalid", request->url().c_str());
  }
  else if (!_can_set_azimuth)
  {
    _rsp_status.error_code = AlpacaErrorCode_t::NotImplemented;
    snprintf(_rsp_status.error_msg, sizeof(_rsp_status.error_msg), "%s - SlewToAzimuth is not implemented", request->url().c_str());
  }
  else
  {
    _startSlew(static_cast<float>(azimuth));
  }
  _alpaca_server->Respond(request, _clients[client_idx], _rsp_status);
}

void Dome::_putSyncToAzimuth(AsyncWebServerRequest *request)
{
  const uint32_t client_idx = _checkedClient(request, Spelling_t::kStrict);
  double azimuth = 0.0;
  if (!_alpaca_server->GetParam(request, "Azimuth", azimuth, Spelling_t::kStrict) || azimuth < 0.0 || azimuth >= 360.0)
  {
    _rsp_status.error_code = AlpacaErrorCode_t::InvalidValue;
    snprintf(_rsp_status.error_msg, sizeof(_rsp_status.error_msg), "%s - Parameter 'Azimuth' not found or invalid", request->url().c_str());
  }
  else if (!_can_sync_azimuth)
  {
    _rsp_status.error_code = AlpacaErrorCode_t::NotImplemented;
    snprintf(_rsp_status.error_msg, sizeof(_rsp_status.error_msg), "%s - SyncToAzimuth is not implemented", request->url().c_str());
  }
  else
  {
    _sync_offset = _normaliseAzimuth(static_cast<float>(azimuth) - _current_azimuth);
  }
  _alpaca_server->Respond(request, _clients[client_idx], _rsp_status);
}

void Dome::AlpacaPutAction(AsyncWebServerRequest *request)
{
  const uint32_t client_idx = _checkedClient(request, Spelling_t::kStrict);
  char action[64] = {0};

  if (!_alpaca_server->GetParam(request, "Action", action, sizeof(action), Spelling_t::kStrict))
  {
    _rsp_status.error_code = AlpacaErrorCode_t::InvalidValue;
    snprintf(_rsp_status.error_msg, sizeof(_rsp_status.error_msg), "%s - Parameter 'Action' not found", request->url().c_str());
  }
  else if (strcasecmp(action, "status") == 0)
  {
    char payload[384] = {0};
    GetMqttHeartbeatJson(payload, sizeof(payload));
    _alpaca_server->Respond(request, _clients[client_idx], _rsp_status, payload, JsonValue_t::kAsPlainStringValue);
    return;
  }
  else
  {
    _rsp_status.error_code = AlpacaErrorCode_t::ActionNotImplementedException;
    snprintf(_rsp_status.error_msg, sizeof(_rsp_status.error_msg), "%s - Action '%s' not implemented", request->url().c_str(), action);
  }

  _alpaca_server->Respond(request, _clients[client_idx], _rsp_status);
}

const bool Dome::_getDeviceStateList(size_t buf_len, char *buf)
{
  snprintf(buf,
           buf_len,
           "[{\"Name\":\"Azimuth\",\"Value\":%.2f},{\"Name\":\"AtHome\",\"Value\":%s},{\"Name\":\"AtPark\",\"Value\":%s},{\"Name\":\"Slewing\",\"Value\":%s},{\"Name\":\"ShutterStatus\",\"Value\":%d}]",
           _normaliseAzimuth(_current_azimuth + _sync_offset),
           _isAtAzimuth(static_cast<float>(_home_position)) ? "true" : "false",
           _isAtAzimuth(static_cast<float>(_park_position)) ? "true" : "false",
           _dome_state == DOME_SLEWING ? "true" : "false",
           static_cast<int>(_shutter_state));
  return true;
}

void Dome::AlpacaReadJson(JsonObject &root)
{
  AlpacaDevice::AlpacaReadJson(root);
  bool motor_config_changed = false;
  if (JsonObject obj_config = root["DomeConfiguration"])
  {
    const uint8_t sda_pin = obj_config["SDA"] | _sda_pin;
    const uint8_t scl_pin = obj_config["SCL"] | _scl_pin;
    const uint32_t i2c_clock_hz = obj_config["I2CClockHz"] | _i2c_clock_hz;
    const uint8_t motor_address = obj_config["MotorAddress"] | _motor_address;
    motor_config_changed = sda_pin != _sda_pin || scl_pin != _scl_pin || i2c_clock_hz != _i2c_clock_hz || motor_address != _motor_address;
    _sda_pin = sda_pin;
    _scl_pin = scl_pin;
    _i2c_clock_hz = i2c_clock_hz;
    _motor_address = motor_address;
    _encoder_host = obj_config["EncoderHost"] | _encoder_host;
    _encoder_port = obj_config["EncoderPort"] | _encoder_port;
    _shutter_host = obj_config["ShutterHost"] | _shutter_host;
    _shutter_port = obj_config["ShutterPort"] | _shutter_port;
    _encoder_poll_interval_ms = obj_config["EncoderPollIntervalMs"] | _encoder_poll_interval_ms;
    _shutter_poll_interval_ms = obj_config["ShutterPollIntervalMs"] | _shutter_poll_interval_ms;
    _rest_timeout_ms = obj_config["RestTimeoutMs"] | _rest_timeout_ms;
    _current_azimuth = obj_config["CurrentAzimuth"] | _current_azimuth;
    _home_position = obj_config["HomePosition"] | _home_position;
    _park_position = obj_config["ParkPosition"] | _park_position;
    _sync_offset = obj_config["AzimuthSyncOffset"] | _sync_offset;
    _acceptable_azimuth_error = obj_config["AcceptableAzimuthError"] | _acceptable_azimuth_error;
    _slow_azimuth_range = obj_config["SlowAzimuthRange"] | _slow_azimuth_range;
    _slow_slew_speed = obj_config["SlowSlewSpeed"] | _slow_slew_speed;
    _fast_slew_speed = obj_config["FastSlewSpeed"] | _fast_slew_speed;
    _simulated_slew_step_deg = obj_config["SimulatedSlewStepDeg"] | _simulated_slew_step_deg;
    _can_set_shutter = obj_config["CanSetShutter"] | _can_set_shutter;
    _mqtt_host = obj_config["MQTTHost"] | _mqtt_host;
    _mqtt_port = obj_config["MQTTPort"] | _mqtt_port;
    _mqtt_user = obj_config["MQTTUser"] | _mqtt_user;
    _mqtt_pwd = obj_config["MQTTPwd"] | _mqtt_pwd;
    _mqtt_health_topic = obj_config["MQTTHealthTopic"] | _mqtt_health_topic;
    _mqtt_function_topic = obj_config["MQTTFunctionTopic"] | _mqtt_function_topic;
  }

  _current_azimuth = _normaliseAzimuth(_current_azimuth);
  _target_azimuth = _normaliseAzimuth(_target_azimuth);

  if (motor_config_changed)
  {
    Wire.begin(_sda_pin, _scl_pin);
    Wire.setClock(_i2c_clock_hz);
    _motor.configure(_motor_address, &Wire);
    _motor_present = _motor.begin();
  }

  _updateRemoteEncoder(true);
  _updateRemoteShutter(true);
}

void Dome::AlpacaWriteJson(JsonObject &root)
{
  AlpacaDevice::AlpacaWriteJson(root);
  JsonObject obj_config = root["DomeConfiguration"].to<JsonObject>();
  obj_config["SDA"] = _sda_pin;
  obj_config["SCL"] = _scl_pin;
  obj_config["I2CClockHz"] = _i2c_clock_hz;
  obj_config["MotorAddress"] = _motor_address;
  obj_config["EncoderHost"] = _encoder_host;
  obj_config["EncoderPort"] = _encoder_port;
  obj_config["ShutterHost"] = _shutter_host;
  obj_config["ShutterPort"] = _shutter_port;
  obj_config["EncoderPollIntervalMs"] = _encoder_poll_interval_ms;
  obj_config["ShutterPollIntervalMs"] = _shutter_poll_interval_ms;
  obj_config["RestTimeoutMs"] = _rest_timeout_ms;
  obj_config["CurrentAzimuth"] = _current_azimuth;
  obj_config["HomePosition"] = _home_position;
  obj_config["ParkPosition"] = _park_position;
  obj_config["AzimuthSyncOffset"] = _sync_offset;
  obj_config["AcceptableAzimuthError"] = _acceptable_azimuth_error;
  obj_config["SlowAzimuthRange"] = _slow_azimuth_range;
  obj_config["SlowSlewSpeed"] = _slow_slew_speed;
  obj_config["FastSlewSpeed"] = _fast_slew_speed;
  obj_config["SimulatedSlewStepDeg"] = _simulated_slew_step_deg;
  obj_config["CanSetShutter"] = _can_set_shutter;
  obj_config["MQTTHost"] = _mqtt_host;
  obj_config["MQTTPort"] = _mqtt_port;
  obj_config["MQTTUser"] = _mqtt_user;
  obj_config["MQTTPwd"] = _mqtt_pwd;
  obj_config["MQTTHealthTopic"] = _mqtt_health_topic;
  obj_config["MQTTFunctionTopic"] = _mqtt_function_topic;

  JsonObject obj_states = root["#DomeStates"].to<JsonObject>();
  obj_states["MotorPresent"] = _motor_present;
  obj_states["CurrentAzimuth"] = _current_azimuth;
  obj_states["TargetAzimuth"] = _normaliseAzimuth(_target_azimuth + _sync_offset);
  obj_states["DomeState"] = static_cast<int>(_dome_state);
  obj_states["ShutterState"] = static_cast<int>(_shutter_state);
  obj_states["EncoderReadOk"] = _encoder_read_ok;
  obj_states["ShutterReadOk"] = _shutter_read_ok;
  obj_states["LastEncoderHttpCode"] = _last_encoder_http_code;
  obj_states["LastShutterHttpCode"] = _last_shutter_http_code;
}

bool Dome::GetMqttHeartbeatJson(char *buffer, size_t buffer_size) const
{
  const int written = snprintf(buffer,
                               buffer_size,
                               "{\"device\":\"dome\",\"motorPresent\":%s,\"encoderReadOk\":%s,\"shutterReadOk\":%s,\"azimuth\":%.2f,\"targetAzimuth\":%.2f,\"domeStatus\":%u,\"shutterStatus\":%d,\"atPark\":%s,\"atHome\":%s,\"slewing\":%s}",
                               _motor_present ? "true" : "false",
                               _encoder_read_ok ? "true" : "false",
                               _shutter_read_ok ? "true" : "false",
                               _normaliseAzimuth(_current_azimuth + _sync_offset),
                               _normaliseAzimuth(_target_azimuth + _sync_offset),
                               static_cast<unsigned int>(_dome_state),
                               static_cast<int>(_shutter_state),
                               _isAtAzimuth(static_cast<float>(_park_position)) ? "true" : "false",
                               _isAtAzimuth(static_cast<float>(_home_position)) ? "true" : "false",
                               _dome_state == DOME_SLEWING ? "true" : "false");
  return written > 0 && static_cast<size_t>(written) < buffer_size;
}
