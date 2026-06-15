/**************************************************************************************************
  Description: ASCOM Alpaca Camera device for a Melexis MLX90640 thermal array.
**************************************************************************************************/
#include "ThermalCamera.h"
#include "RuntimeSettings.h"

#include <math.h>

namespace
{
const char kDriverVersion[] = "1.0";
const char kIsoUnknown[] = "0001-01-01T00:00:00";
}

ThermalCamera::ThermalCamera()
{
  strlcpy(_device_type, "camera", sizeof(_device_type));
  strlcpy(_device_description, "ESP32-C3 MLX90640 infrared thermal camera", sizeof(_device_description));
  strlcpy(_driver_info, "ASCOM Alpaca Camera driver for Melexis MLX90640 on ESP32-C3", sizeof(_driver_info));
  strlcpy(_device_and_driver_version, kDriverVersion, sizeof(_device_and_driver_version));
  _device_interface_version = 3;

  _addAction("status");
  _addAction("temperaturemap");
}

void ThermalCamera::Begin(uint8_t sda_pin, uint8_t scl_pin)
{
  _sda_pin = sda_pin;
  _scl_pin = scl_pin;

  Wire.begin(_sda_pin, _scl_pin);
  Wire.setClock(_i2c_clock_hz);

  _sensor_ok = _mlx.begin(MLX90640_I2CADDR_DEFAULT, &Wire);
  if (_sensor_ok)
  {
    _mlx.setMode(MLX90640_CHESS);
    _mlx.setResolution(MLX90640_ADC_18BIT);
    _mlx.setRefreshRate(MLX90640_2_HZ);
    _refreshFrame(true);
  }
  else
  {
    SLOG_ERROR_PRINTF("MLX90640 not detected on SDA=%u SCL=%u\n", _sda_pin, _scl_pin);
  }

  AlpacaDevice::Begin();
}

void ThermalCamera::Loop()
{
  if (_camera_state_exposing && (millis() - _exposure_start_ms) >= static_cast<uint32_t>(_exposure_duration * 1000.0))
  {
    _refreshFrame(true);
    _last_exposure_duration = _exposure_duration;
    _camera_state_exposing = false;
    _image_ready = _sensor_ok;
  }

  _refreshFrame(false);
}

void ThermalCamera::RegisterCallbacks()
{
  AlpacaDevice::RegisterCallbacks();

  createCallBack(LHF(AlpacaPutAction), HTTP_PUT, "action");

  createCallBack(LHF(_getCanAbortExposure), HTTP_GET, "canabortexposure");
  createCallBack(LHF(_getCanAsymmetricBin), HTTP_GET, "canasymmetricbin");
  createCallBack(LHF(_getCanFastReadout), HTTP_GET, "canfastreadout");
  createCallBack(LHF(_getCanGetCoolerPower), HTTP_GET, "cangetcoolerpower");
  createCallBack(LHF(_getCanPulseGuide), HTTP_GET, "canpulseguide");
  createCallBack(LHF(_getCanSetCcdTemperature), HTTP_GET, "cansetccdtemperature");
  createCallBack(LHF(_getCanStopExposure), HTTP_GET, "canstopexposure");
  createCallBack(LHF(_getCameraState), HTTP_GET, "camerastate");
  createCallBack(LHF(_getCameraXSize), HTTP_GET, "cameraxsize");
  createCallBack(LHF(_getCameraYSize), HTTP_GET, "cameraysize");
  createCallBack(LHF(_getCcdTemperature), HTTP_GET, "ccdtemperature");
  createCallBack(LHF(_getCoolerOn), HTTP_GET, "cooleron");
  createCallBack(LHF(_putCoolerOn), HTTP_PUT, "cooleron");
  createCallBack(LHF(_getCoolerPower), HTTP_GET, "coolerpower");
  createCallBack(LHF(_getElectronsPerADU), HTTP_GET, "electronsperadu");
  createCallBack(LHF(_getExposureMax), HTTP_GET, "exposuremax");
  createCallBack(LHF(_getExposureMin), HTTP_GET, "exposuremin");
  createCallBack(LHF(_getExposureResolution), HTTP_GET, "exposureresolution");
  createCallBack(LHF(_getFastReadout), HTTP_GET, "fastreadout");
  createCallBack(LHF(_putFastReadout), HTTP_PUT, "fastreadout");
  createCallBack(LHF(_getFullWellCapacity), HTTP_GET, "fullwellcapacity");
  createCallBack(LHF(_getGain), HTTP_GET, "gain");
  createCallBack(LHF(_putGain), HTTP_PUT, "gain");
  createCallBack(LHF(_getGainMax), HTTP_GET, "gainmax");
  createCallBack(LHF(_getGainMin), HTTP_GET, "gainmin");
  createCallBack(LHF(_getGains), HTTP_GET, "gains");
  createCallBack(LHF(_getHasShutter), HTTP_GET, "hasshutter");
  createCallBack(LHF(_getHeatSinkTemperature), HTTP_GET, "heatsinktemperature");
  createCallBack(LHF(_getImageArray), HTTP_GET, "imagearray");
  createCallBack(LHF(_getImageArrayVariant), HTTP_GET, "imagearrayvariant");
  createCallBack(LHF(_getImageReady), HTTP_GET, "imageready");
  createCallBack(LHF(_getIsPulseGuiding), HTTP_GET, "ispulseguiding");
  createCallBack(LHF(_getLastExposureDuration), HTTP_GET, "lastexposureduration");
  createCallBack(LHF(_getLastExposureStartTime), HTTP_GET, "lastexposurestarttime");
  createCallBack(LHF(_getMaxADU), HTTP_GET, "maxadu");
  createCallBack(LHF(_getMaxBinX), HTTP_GET, "maxbinx");
  createCallBack(LHF(_getMaxBinY), HTTP_GET, "maxbiny");
  createCallBack(LHF(_getBinX), HTTP_GET, "binx");
  createCallBack(LHF(_putBinX), HTTP_PUT, "binx");
  createCallBack(LHF(_getBinY), HTTP_GET, "biny");
  createCallBack(LHF(_putBinY), HTTP_PUT, "biny");
  createCallBack(LHF(_getNumX), HTTP_GET, "numx");
  createCallBack(LHF(_putNumX), HTTP_PUT, "numx");
  createCallBack(LHF(_getNumY), HTTP_GET, "numy");
  createCallBack(LHF(_putNumY), HTTP_PUT, "numy");
  createCallBack(LHF(_getStartX), HTTP_GET, "startx");
  createCallBack(LHF(_putStartX), HTTP_PUT, "startx");
  createCallBack(LHF(_getStartY), HTTP_GET, "starty");
  createCallBack(LHF(_putStartY), HTTP_PUT, "starty");
  createCallBack(LHF(_getOffset), HTTP_GET, "offset");
  createCallBack(LHF(_putOffset), HTTP_PUT, "offset");
  createCallBack(LHF(_getOffsetMax), HTTP_GET, "offsetmax");
  createCallBack(LHF(_getOffsetMin), HTTP_GET, "offsetmin");
  createCallBack(LHF(_getOffsets), HTTP_GET, "offsets");
  createCallBack(LHF(_getPercentCompleted), HTTP_GET, "percentcompleted");
  createCallBack(LHF(_getPixelSizeX), HTTP_GET, "pixelsizex");
  createCallBack(LHF(_getPixelSizeY), HTTP_GET, "pixelsizey");
  createCallBack(LHF(_getReadoutMode), HTTP_GET, "readoutmode");
  createCallBack(LHF(_putReadoutMode), HTTP_PUT, "readoutmode");
  createCallBack(LHF(_getReadoutModes), HTTP_GET, "readoutmodes");
  createCallBack(LHF(_getSensorName), HTTP_GET, "sensorname");
  createCallBack(LHF(_getSensorType), HTTP_GET, "sensortype");
  createCallBack(LHF(_getSetCcdTemperature), HTTP_GET, "setccdtemperature");
  createCallBack(LHF(_putSetCcdTemperature), HTTP_PUT, "setccdtemperature");
  createCallBack(LHF(_getSubExposureDuration), HTTP_GET, "subexposureduration");
  createCallBack(LHF(_putSubExposureDuration), HTTP_PUT, "subexposureduration");

  createCallBack(LHF(_putAbortExposure), HTTP_PUT, "abortexposure");
  createCallBack(LHF(_putPulseGuide), HTTP_PUT, "pulseguide");
  createCallBack(LHF(_putStartExposure), HTTP_PUT, "startexposure");
  createCallBack(LHF(_putStopExposure), HTTP_PUT, "stopexposure");
}

void ThermalCamera::_refreshFrame(bool force)
{
  if (!_sensor_ok || (!force && millis() - _last_frame_ms < _frame_interval_ms))
  {
    return;
  }

  if (_mlx.getFrame(_temperatures) == 0)
  {
    _last_frame_ms = millis();
    _image_ready = true;
    _updateStats();
  }
  else
  {
    _image_ready = false;
  }
}

void ThermalCamera::_updateStats()
{
  float min_temp = _temperatures[0];
  float max_temp = _temperatures[0];
  double sum = 0.0;

  for (uint16_t i = 0; i < kPixelCount; i++)
  {
    const float t = _temperatures[i];
    min_temp = min(min_temp, t);
    max_temp = max(max_temp, t);
    sum += t;
  }

  _min_temp_c = min_temp;
  _max_temp_c = max_temp;
  _mean_temp_c = static_cast<float>(sum / kPixelCount);
}

uint32_t ThermalCamera::_checkedClient(AsyncWebServerRequest *request, Spelling_t spelling)
{
  _service_counter++;
  uint32_t client_idx = 0;
  return checkClientDataAndConnection(request, client_idx, spelling);
}

void ThermalCamera::_respondSimple(AsyncWebServerRequest *request, const char *value, JsonValue_t value_type)
{
  const uint32_t client_idx = _checkedClient(request);
  _alpaca_server->Respond(request, _clients[client_idx], _rsp_status, value, value_type);
}

void ThermalCamera::_respondNotImplemented(AsyncWebServerRequest *request, const char *command)
{
  const uint32_t client_idx = _checkedClient(request);
  _rsp_status.error_code = AlpacaErrorCode_t::NotImplemented;
  snprintf(_rsp_status.error_msg, sizeof(_rsp_status.error_msg), "%s - Command '%s' not implemented", request->url().c_str(), command);
  _alpaca_server->Respond(request, _clients[client_idx], _rsp_status);
}

void ThermalCamera::_getCanAbortExposure(AsyncWebServerRequest *request) { _respondSimple(request, "true"); }
void ThermalCamera::_getCanAsymmetricBin(AsyncWebServerRequest *request) { _respondSimple(request, "false"); }
void ThermalCamera::_getCanFastReadout(AsyncWebServerRequest *request) { _respondSimple(request, "false"); }
void ThermalCamera::_getCanGetCoolerPower(AsyncWebServerRequest *request) { _respondSimple(request, "false"); }
void ThermalCamera::_getCanPulseGuide(AsyncWebServerRequest *request) { _respondSimple(request, "false"); }
void ThermalCamera::_getCanSetCcdTemperature(AsyncWebServerRequest *request) { _respondSimple(request, "false"); }
void ThermalCamera::_getCanStopExposure(AsyncWebServerRequest *request) { _respondSimple(request, "true"); }
void ThermalCamera::_getCameraState(AsyncWebServerRequest *request) { _respondSimple(request, _camera_state_exposing ? "2" : "0"); }
void ThermalCamera::_getCameraXSize(AsyncWebServerRequest *request) { _respondSimple(request, "32"); }
void ThermalCamera::_getCameraYSize(AsyncWebServerRequest *request) { _respondSimple(request, "24"); }
void ThermalCamera::_getCcdTemperature(AsyncWebServerRequest *request) { char value[32]; snprintf(value, sizeof(value), "%f", _mean_temp_c); _respondSimple(request, value); }
void ThermalCamera::_getCoolerOn(AsyncWebServerRequest *request) { _respondSimple(request, "false"); }
void ThermalCamera::_putCoolerOn(AsyncWebServerRequest *request) { _respondNotImplemented(request, "cooleron"); }
void ThermalCamera::_getCoolerPower(AsyncWebServerRequest *request) { _respondNotImplemented(request, "coolerpower"); }
void ThermalCamera::_getElectronsPerADU(AsyncWebServerRequest *request) { _respondSimple(request, "1.0"); }
void ThermalCamera::_getExposureMax(AsyncWebServerRequest *request) { _respondSimple(request, "5.0"); }
void ThermalCamera::_getExposureMin(AsyncWebServerRequest *request) { _respondSimple(request, "0.5"); }
void ThermalCamera::_getExposureResolution(AsyncWebServerRequest *request) { _respondSimple(request, "0.5"); }
void ThermalCamera::_getFastReadout(AsyncWebServerRequest *request) { _respondSimple(request, "false"); }
void ThermalCamera::_putFastReadout(AsyncWebServerRequest *request) { _respondNotImplemented(request, "fastreadout"); }
void ThermalCamera::_getFullWellCapacity(AsyncWebServerRequest *request) { _respondSimple(request, "65535.0"); }
void ThermalCamera::_getGain(AsyncWebServerRequest *request) { _respondSimple(request, "0"); }
void ThermalCamera::_putGain(AsyncWebServerRequest *request) { _respondNotImplemented(request, "gain"); }
void ThermalCamera::_getGainMax(AsyncWebServerRequest *request) { _respondSimple(request, "0"); }
void ThermalCamera::_getGainMin(AsyncWebServerRequest *request) { _respondSimple(request, "0"); }
void ThermalCamera::_getGains(AsyncWebServerRequest *request) { _respondSimple(request, "[\"Fixed\"]"); }
void ThermalCamera::_getHasShutter(AsyncWebServerRequest *request) { _respondSimple(request, "false"); }
void ThermalCamera::_getHeatSinkTemperature(AsyncWebServerRequest *request) { _respondNotImplemented(request, "heatsinktemperature"); }
void ThermalCamera::_getImageArrayVariant(AsyncWebServerRequest *request) { _getImageArray(request); }
void ThermalCamera::_getImageReady(AsyncWebServerRequest *request) { _respondSimple(request, _image_ready ? "true" : "false"); }
void ThermalCamera::_getIsPulseGuiding(AsyncWebServerRequest *request) { _respondSimple(request, "false"); }
void ThermalCamera::_getLastExposureDuration(AsyncWebServerRequest *request) { char value[32]; snprintf(value, sizeof(value), "%f", _last_exposure_duration); _respondSimple(request, value); }
void ThermalCamera::_getLastExposureStartTime(AsyncWebServerRequest *request) { _respondSimple(request, kIsoUnknown, JsonValue_t::kAsJsonStringValue); }
void ThermalCamera::_getMaxADU(AsyncWebServerRequest *request) { _respondSimple(request, "65535"); }
void ThermalCamera::_getMaxBinX(AsyncWebServerRequest *request) { _respondSimple(request, "1"); }
void ThermalCamera::_getMaxBinY(AsyncWebServerRequest *request) { _respondSimple(request, "1"); }
void ThermalCamera::_getBinX(AsyncWebServerRequest *request) { _respondSimple(request, "1"); }
void ThermalCamera::_putBinX(AsyncWebServerRequest *request) { _respondSimple(request, nullptr, JsonValue_t::kNoValue); }
void ThermalCamera::_getBinY(AsyncWebServerRequest *request) { _respondSimple(request, "1"); }
void ThermalCamera::_putBinY(AsyncWebServerRequest *request) { _respondSimple(request, nullptr, JsonValue_t::kNoValue); }
void ThermalCamera::_getNumX(AsyncWebServerRequest *request) { _respondSimple(request, "32"); }
void ThermalCamera::_putNumX(AsyncWebServerRequest *request) { _respondSimple(request, nullptr, JsonValue_t::kNoValue); }
void ThermalCamera::_getNumY(AsyncWebServerRequest *request) { _respondSimple(request, "24"); }
void ThermalCamera::_putNumY(AsyncWebServerRequest *request) { _respondSimple(request, nullptr, JsonValue_t::kNoValue); }
void ThermalCamera::_getStartX(AsyncWebServerRequest *request) { _respondSimple(request, "0"); }
void ThermalCamera::_putStartX(AsyncWebServerRequest *request) { _respondSimple(request, nullptr, JsonValue_t::kNoValue); }
void ThermalCamera::_getStartY(AsyncWebServerRequest *request) { _respondSimple(request, "0"); }
void ThermalCamera::_putStartY(AsyncWebServerRequest *request) { _respondSimple(request, nullptr, JsonValue_t::kNoValue); }
void ThermalCamera::_getOffset(AsyncWebServerRequest *request) { _respondSimple(request, "0"); }
void ThermalCamera::_putOffset(AsyncWebServerRequest *request) { _respondNotImplemented(request, "offset"); }
void ThermalCamera::_getOffsetMax(AsyncWebServerRequest *request) { _respondSimple(request, "0"); }
void ThermalCamera::_getOffsetMin(AsyncWebServerRequest *request) { _respondSimple(request, "0"); }
void ThermalCamera::_getOffsets(AsyncWebServerRequest *request) { _respondSimple(request, "[\"Fixed\"]"); }
void ThermalCamera::_getPercentCompleted(AsyncWebServerRequest *request) { _respondSimple(request, _camera_state_exposing ? "50" : "100"); }
void ThermalCamera::_getPixelSizeX(AsyncWebServerRequest *request) { _respondSimple(request, "1.0"); }
void ThermalCamera::_getPixelSizeY(AsyncWebServerRequest *request) { _respondSimple(request, "1.0"); }
void ThermalCamera::_getReadoutMode(AsyncWebServerRequest *request) { _respondSimple(request, "0"); }
void ThermalCamera::_putReadoutMode(AsyncWebServerRequest *request) { _respondSimple(request, nullptr, JsonValue_t::kNoValue); }
void ThermalCamera::_getReadoutModes(AsyncWebServerRequest *request) { _respondSimple(request, "[\"MLX90640 2Hz thermal\"]"); }
void ThermalCamera::_getSensorName(AsyncWebServerRequest *request) { _respondSimple(request, "Melexis MLX90640", JsonValue_t::kAsJsonStringValue); }
void ThermalCamera::_getSensorType(AsyncWebServerRequest *request) { _respondSimple(request, "0"); }
void ThermalCamera::_getSetCcdTemperature(AsyncWebServerRequest *request) { _respondNotImplemented(request, "setccdtemperature"); }
void ThermalCamera::_putSetCcdTemperature(AsyncWebServerRequest *request) { _respondNotImplemented(request, "setccdtemperature"); }
void ThermalCamera::_getSubExposureDuration(AsyncWebServerRequest *request) { _respondSimple(request, "0.5"); }
void ThermalCamera::_putSubExposureDuration(AsyncWebServerRequest *request) { _respondSimple(request, nullptr, JsonValue_t::kNoValue); }
void ThermalCamera::_putAbortExposure(AsyncWebServerRequest *request) { _camera_state_exposing = false; _image_ready = false; _respondSimple(request, nullptr, JsonValue_t::kNoValue); }
void ThermalCamera::_putPulseGuide(AsyncWebServerRequest *request) { _respondNotImplemented(request, "pulseguide"); }
void ThermalCamera::_putStopExposure(AsyncWebServerRequest *request) { _camera_state_exposing = false; _refreshFrame(true); _respondSimple(request, nullptr, JsonValue_t::kNoValue); }

void ThermalCamera::_putStartExposure(AsyncWebServerRequest *request)
{
  const uint32_t client_idx = _checkedClient(request, Spelling_t::kStrict);
  double duration = _exposure_duration;
  bool light = true;

  if (!_alpaca_server->GetParam(request, "Duration", duration, Spelling_t::kStrict))
  {
    _rsp_status.error_code = AlpacaErrorCode_t::InvalidValue;
    snprintf(_rsp_status.error_msg, sizeof(_rsp_status.error_msg), "%s - Parameter 'Duration' not found", request->url().c_str());
  }
  else
  {
    _alpaca_server->GetParam(request, "Light", light, Spelling_t::kStrict);
    _exposure_duration = constrain(duration, 0.5, 5.0);
    _light_frame = light;
    _camera_state_exposing = true;
    _image_ready = false;
    _exposure_start_ms = millis();
  }

  _alpaca_server->Respond(request, _clients[client_idx], _rsp_status);
}

void ThermalCamera::_getImageArray(AsyncWebServerRequest *request)
{
  const uint32_t client_idx = _checkedClient(request);
  if (!_image_ready)
  {
    _refreshFrame(true);
  }

  AsyncResponseStream *response = request->beginResponseStream(kAlpacaJsonType);
  response->printf("{ \"Value\": [");
  for (uint16_t y = 0; y < kSensorHeight; y++)
  {
    if (y > 0)
      response->print(",");
    response->print("[");
    for (uint16_t x = 0; x < kSensorWidth; x++)
    {
      if (x > 0)
        response->print(",");
      const int32_t centi_c = static_cast<int32_t>(lroundf(_temperatures[y * kSensorWidth + x] * 100.0f));
      response->print(centi_c);
    }
    response->print("]");
  }
  response->printf("], \"ClientTransactionID\": %u, \"ServerTransactionID\": 0, \"ErrorNumber\": %d, \"ErrorMessage\": \"%s\"}",
                   _clients[client_idx].client_transaction_id,
                   static_cast<int32_t>(_rsp_status.error_code),
                   _rsp_status.error_msg);
  request->send(response);
}

void ThermalCamera::AlpacaPutAction(AsyncWebServerRequest *request)
{
  const uint32_t client_idx = _checkedClient(request, Spelling_t::kStrict);
  char action[64] = {0};
  char parameters[128] = {0};

  if (!_alpaca_server->GetParam(request, "Action", action, sizeof(action), Spelling_t::kStrict))
  {
    _rsp_status.error_code = AlpacaErrorCode_t::InvalidValue;
    snprintf(_rsp_status.error_msg, sizeof(_rsp_status.error_msg), "%s - Parameter 'Action' not found", request->url().c_str());
    _alpaca_server->Respond(request, _clients[client_idx], _rsp_status);
    return;
  }
  _alpaca_server->GetParam(request, "Parameters", parameters, sizeof(parameters), Spelling_t::kStrict);

  if (strcasecmp(action, "status") == 0)
  {
    char payload[256];
    snprintf(payload, sizeof(payload),
             "{\"SensorOk\":%s,\"Width\":%u,\"Height\":%u,\"MinC\":%.2f,\"MeanC\":%.2f,\"MaxC\":%.2f,\"FrameAgeMs\":%u}",
             _sensor_ok ? "true" : "false", kSensorWidth, kSensorHeight, _min_temp_c, _mean_temp_c, _max_temp_c, millis() - _last_frame_ms);
    _alpaca_server->Respond(request, _clients[client_idx], _rsp_status, payload, JsonValue_t::kAsPlainStringValue);
    return;
  }

  if (strcasecmp(action, "temperaturemap") == 0)
  {
    AsyncResponseStream *response = request->beginResponseStream(kAlpacaJsonType);
    response->printf("{ \"Value\": {\"Width\":%u,\"Height\":%u,\"Unit\":\"C\",\"Pixels\":[", kSensorWidth, kSensorHeight);
    for (uint16_t i = 0; i < kPixelCount; i++)
    {
      if (i > 0)
        response->print(",");
      response->printf("%.2f", _temperatures[i]);
    }
    response->printf("]}, \"ClientTransactionID\": %u, \"ServerTransactionID\": 0, \"ErrorNumber\": 0, \"ErrorMessage\": \"\"}",
                     _clients[client_idx].client_transaction_id);
    request->send(response);
    return;
  }

  _rsp_status.error_code = AlpacaErrorCode_t::ActionNotImplementedException;
  snprintf(_rsp_status.error_msg, sizeof(_rsp_status.error_msg), "%s - Action '%s' not implemented", request->url().c_str(), action);
  _alpaca_server->Respond(request, _clients[client_idx], _rsp_status);
}

bool const ThermalCamera::_getDeviceStateList(size_t buf_len, char *buf)
{
  const size_t written = snprintf(buf, buf_len,
                                  "{\"Name\":\"CameraState\",\"Value\":%d},{\"Name\":\"ImageReady\",\"Value\":%s},{\"Name\":\"CCDTemperature\",\"Value\":%.2f}",
                                  _camera_state_exposing ? 2 : 0,
                                  _image_ready ? "true" : "false",
                                  _mean_temp_c);
  return written > 0 && written < buf_len;
}

void ThermalCamera::AlpacaReadJson(JsonObject &root)
{
  AlpacaDevice::AlpacaReadJson(root);
  if (JsonObject obj_config = root["ThermalCameraConfiguration"])
  {
    RuntimeSettings::ReadJson(obj_config);
    _sda_pin = obj_config["SDA"] | _sda_pin;
    _scl_pin = obj_config["SCL"] | _scl_pin;
    _i2c_clock_hz = obj_config["I2CClockHz"] | _i2c_clock_hz;
    _mqtt_host = obj_config["MQTTHost"] | _mqtt_host;
    _mqtt_port = obj_config["MQTTPort"] | _mqtt_port;
    _mqtt_user = obj_config["MQTTUser"] | _mqtt_user;
    _mqtt_pwd = obj_config["MQTTPwd"] | _mqtt_pwd;
    _mqtt_health_topic = obj_config["MQTTHealthTopic"] | _mqtt_health_topic;
    _mqtt_function_topic = obj_config["MQTTFunctionTopic"] | _mqtt_function_topic;
  }
}

void ThermalCamera::AlpacaWriteJson(JsonObject &root)
{
  AlpacaDevice::AlpacaWriteJson(root);
  JsonObject obj_config = root["ThermalCameraConfiguration"].to<JsonObject>();
  RuntimeSettings::WriteJson(obj_config);
  obj_config["SDA"] = _sda_pin;
  obj_config["SCL"] = _scl_pin;
  obj_config["I2CClockHz"] = _i2c_clock_hz;
  obj_config["MQTTHost"] = _mqtt_host;
  obj_config["MQTTPort"] = _mqtt_port;
  obj_config["MQTTUser"] = _mqtt_user;
  obj_config["MQTTPwd"] = _mqtt_pwd;
  obj_config["MQTTHealthTopic"] = _mqtt_health_topic;
  obj_config["MQTTFunctionTopic"] = _mqtt_function_topic;

  JsonObject obj_states = root["#States"].to<JsonObject>();
  obj_states["SensorOk"] = _sensor_ok;
  obj_states["Width"] = kSensorWidth;
  obj_states["Height"] = kSensorHeight;
  obj_states["MinC"] = _min_temp_c;
  obj_states["MeanC"] = _mean_temp_c;
  obj_states["MaxC"] = _max_temp_c;
}

bool ThermalCamera::GetMqttHeartbeatJson(char *buffer, size_t buffer_size) const
{
  const int written = snprintf(buffer,
                               buffer_size,
                               "{\"device\":\"camera\",\"type\":\"MLX90640\",\"sensorOk\":%s,\"imageReady\":%s,\"cameraState\":%d,\"width\":%u,\"height\":%u,\"minC\":%.2f,\"meanC\":%.2f,\"maxC\":%.2f,\"frameAgeMs\":%u}",
                               _sensor_ok ? "true" : "false",
                               _image_ready ? "true" : "false",
                               _camera_state_exposing ? 2 : 0,
                               kSensorWidth,
                               kSensorHeight,
                               _min_temp_c,
                               _mean_temp_c,
                               _max_temp_c,
                               millis() - _last_frame_ms);
  return written > 0 && static_cast<size_t>(written) < buffer_size;
}
