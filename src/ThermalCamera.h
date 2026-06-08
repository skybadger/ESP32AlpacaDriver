/**************************************************************************************************
  Description: ASCOM Alpaca Camera device for a Melexis MLX90640 thermal array.
**************************************************************************************************/
#pragma once

#include <AlpacaDevice.h>
#include <Adafruit_MLX90640.h>
#include <Wire.h>

class ThermalCamera : public AlpacaDevice
{
private:
  static constexpr uint16_t kSensorWidth = 32;
  static constexpr uint16_t kSensorHeight = 24;
  static constexpr uint16_t kPixelCount = kSensorWidth * kSensorHeight;
  static constexpr uint8_t kDefaultSdaPin = 8;
  static constexpr uint8_t kDefaultSclPin = 9;
  static constexpr uint32_t kDefaultI2cClockHz = 400000;

  Adafruit_MLX90640 _mlx;
  float _temperatures[kPixelCount] = {0.0f};

  uint8_t _sda_pin = kDefaultSdaPin;
  uint8_t _scl_pin = kDefaultSclPin;
  uint32_t _i2c_clock_hz = kDefaultI2cClockHz;
  uint32_t _last_frame_ms = 0;
  uint32_t _frame_interval_ms = 500;
  uint32_t _exposure_start_ms = 0;

  bool _sensor_ok = false;
  bool _image_ready = false;
  bool _camera_state_exposing = false;

  double _exposure_duration = 0.5;
  double _last_exposure_duration = 0.5;
  bool _light_frame = true;

  float _min_temp_c = 0.0f;
  float _max_temp_c = 0.0f;
  float _mean_temp_c = 0.0f;

  void _refreshFrame(bool force = false);
  void _updateStats();
  void _respondSimple(AsyncWebServerRequest *request, const char *value, JsonValue_t value_type = JsonValue_t::kAsPlainStringValue);
  void _respondNotImplemented(AsyncWebServerRequest *request, const char *command);
  uint32_t _checkedClient(AsyncWebServerRequest *request, Spelling_t spelling = Spelling_t::kIgnoreCase);

  void _getCanAbortExposure(AsyncWebServerRequest *request);
  void _getCanAsymmetricBin(AsyncWebServerRequest *request);
  void _getCanFastReadout(AsyncWebServerRequest *request);
  void _getCanGetCoolerPower(AsyncWebServerRequest *request);
  void _getCanPulseGuide(AsyncWebServerRequest *request);
  void _getCanSetCcdTemperature(AsyncWebServerRequest *request);
  void _getCanStopExposure(AsyncWebServerRequest *request);
  void _getCameraState(AsyncWebServerRequest *request);
  void _getCameraXSize(AsyncWebServerRequest *request);
  void _getCameraYSize(AsyncWebServerRequest *request);
  void _getCcdTemperature(AsyncWebServerRequest *request);
  void _getCoolerOn(AsyncWebServerRequest *request);
  void _putCoolerOn(AsyncWebServerRequest *request);
  void _getCoolerPower(AsyncWebServerRequest *request);
  void _getElectronsPerADU(AsyncWebServerRequest *request);
  void _getExposureMax(AsyncWebServerRequest *request);
  void _getExposureMin(AsyncWebServerRequest *request);
  void _getExposureResolution(AsyncWebServerRequest *request);
  void _getFastReadout(AsyncWebServerRequest *request);
  void _putFastReadout(AsyncWebServerRequest *request);
  void _getFullWellCapacity(AsyncWebServerRequest *request);
  void _getGain(AsyncWebServerRequest *request);
  void _putGain(AsyncWebServerRequest *request);
  void _getGainMax(AsyncWebServerRequest *request);
  void _getGainMin(AsyncWebServerRequest *request);
  void _getGains(AsyncWebServerRequest *request);
  void _getHasShutter(AsyncWebServerRequest *request);
  void _getHeatSinkTemperature(AsyncWebServerRequest *request);
  void _getImageArray(AsyncWebServerRequest *request);
  void _getImageArrayVariant(AsyncWebServerRequest *request);
  void _getImageReady(AsyncWebServerRequest *request);
  void _getIsPulseGuiding(AsyncWebServerRequest *request);
  void _getLastExposureDuration(AsyncWebServerRequest *request);
  void _getLastExposureStartTime(AsyncWebServerRequest *request);
  void _getMaxADU(AsyncWebServerRequest *request);
  void _getMaxBinX(AsyncWebServerRequest *request);
  void _getMaxBinY(AsyncWebServerRequest *request);
  void _getBinX(AsyncWebServerRequest *request);
  void _putBinX(AsyncWebServerRequest *request);
  void _getBinY(AsyncWebServerRequest *request);
  void _putBinY(AsyncWebServerRequest *request);
  void _getNumX(AsyncWebServerRequest *request);
  void _putNumX(AsyncWebServerRequest *request);
  void _getNumY(AsyncWebServerRequest *request);
  void _putNumY(AsyncWebServerRequest *request);
  void _getStartX(AsyncWebServerRequest *request);
  void _putStartX(AsyncWebServerRequest *request);
  void _getStartY(AsyncWebServerRequest *request);
  void _putStartY(AsyncWebServerRequest *request);
  void _getOffset(AsyncWebServerRequest *request);
  void _putOffset(AsyncWebServerRequest *request);
  void _getOffsetMax(AsyncWebServerRequest *request);
  void _getOffsetMin(AsyncWebServerRequest *request);
  void _getOffsets(AsyncWebServerRequest *request);
  void _getPercentCompleted(AsyncWebServerRequest *request);
  void _getPixelSizeX(AsyncWebServerRequest *request);
  void _getPixelSizeY(AsyncWebServerRequest *request);
  void _getReadoutMode(AsyncWebServerRequest *request);
  void _putReadoutMode(AsyncWebServerRequest *request);
  void _getReadoutModes(AsyncWebServerRequest *request);
  void _getSensorName(AsyncWebServerRequest *request);
  void _getSensorType(AsyncWebServerRequest *request);
  void _getSetCcdTemperature(AsyncWebServerRequest *request);
  void _putSetCcdTemperature(AsyncWebServerRequest *request);
  void _getSubExposureDuration(AsyncWebServerRequest *request);
  void _putSubExposureDuration(AsyncWebServerRequest *request);

  void _putAbortExposure(AsyncWebServerRequest *request);
  void _putPulseGuide(AsyncWebServerRequest *request);
  void _putStartExposure(AsyncWebServerRequest *request);
  void _putStopExposure(AsyncWebServerRequest *request);
  void AlpacaPutAction(AsyncWebServerRequest *request);

  const bool _getDeviceStateList(size_t buf_len, char *buf);

public:
  ThermalCamera();
  void Begin(uint8_t sda_pin = kDefaultSdaPin, uint8_t scl_pin = kDefaultSclPin);
  void Loop();
  void RegisterCallbacks();

  void AlpacaReadJson(JsonObject &root);
  void AlpacaWriteJson(JsonObject &root);
  bool GetMqttHeartbeatJson(char *buffer, size_t buffer_size) const;
};
