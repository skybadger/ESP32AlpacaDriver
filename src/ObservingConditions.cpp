/**************************************************************************************************
  Filename:       ObservingConditions.cpp
  Revised:        $Date: 2024-02-02$
  Revision:       $Revision: 01 $
  Description:    ASCOM Observing Conditions Device Template

  Copyright 2024-2025 peter_n@gmx.de. All rights reserved.
**************************************************************************************************/
#include "ObservingConditions.h"
#include "RuntimeSettings.h"

#include <math.h>

ObservingConditions::ObservingConditions() : AlpacaObservingConditions()
{
}

void ObservingConditions::Begin(uint8_t sda_pin, uint8_t scl_pin)
{
    _sda_pin = sda_pin;
    _scl_pin = scl_pin;

    Wire.begin(_sda_pin, _scl_pin);
    Wire.setClock(_i2c_clock_hz);

    _htu21d_present = _htu21d.begin(&Wire);
    _bme280_present = _bme280.begin(_bme280_address, &Wire);
    _bmp280_present = _bmp280.begin(_bmp280_address);
    _hmc5883_present = _hmc5883.begin();

    SLOG_INFO_PRINTF("ObservingConditions sensors HTU21D=%s BME280=%s BMP280=%s HMC5883=%s\n",
                     _htu21d_present ? "ok" : "missing",
                     _bme280_present ? "ok" : "missing",
                     _bmp280_present ? "ok" : "missing",
                     _hmc5883_present ? "ok" : "missing");

    // adapt the sensor description
    SetSensorDescriptionByIdx(kOcCloudCoverSensorIdx, "Cloud cover is not implemented");
    SetSensorDescriptionByIdx(kOcDewPointSensorIdx, "Calculated dew point from temperature and humidity");
    SetSensorDescriptionByIdx(kOcHumiditySensorIdx, "Relative humidity from BME280 or HTU21D");
    SetSensorDescriptionByIdx(kOcPressureSensorIdx, "Barometric pressure from BME280 or BMP280");
    SetSensorDescriptionByIdx(kOcRainRateSensorIdx, "Rain rate is not implemented");
    SetSensorDescriptionByIdx(kOcSkyBrightnessSensorIdx, "Sky brightness is not implemented");
    SetSensorDescriptionByIdx(kOcSkyQualitySensorIdx, "Sky quality is not implemented");
    SetSensorDescriptionByIdx(kOcSkyTemperatureSensorIdx, "Sky temperature is not implemented");
    SetSensorDescriptionByIdx(kOcStarFwhmSensorIdx, "Star FWHM is not implemented");
    SetSensorDescriptionByIdx(kOcTemperatureSensorIdx, "Ambient temperature from BME280, BMP280, or HTU21D");
    SetSensorDescriptionByIdx(kOcWindDirectionSensorIdx, "Magnetic heading from HMC5883 compass");
    SetSensorDescriptionByIdx(kOcWindGustSensorIdx, "Wind gust is not implemented");
    SetSensorDescriptionByIdx(kOcWindSpeedSensorIdx, "Wind speed is not implemented");

    // adapt if not implemented
    SetSensorImplementedByIdx(kOcCloudCoverSensorIdx, false);
    SetSensorImplementedByIdx(kOcDewPointSensorIdx, _bme280_present || _htu21d_present);
    SetSensorImplementedByIdx(kOcHumiditySensorIdx, _bme280_present || _htu21d_present);
    SetSensorImplementedByIdx(kOcPressureSensorIdx, _bme280_present || _bmp280_present);
    SetSensorImplementedByIdx(kOcRainRateSensorIdx, false);
    SetSensorImplementedByIdx(kOcSkyBrightnessSensorIdx, false);
    SetSensorImplementedByIdx(kOcSkyQualitySensorIdx, false);
    SetSensorImplementedByIdx(kOcSkyTemperatureSensorIdx, false);
    SetSensorImplementedByIdx(kOcStarFwhmSensorIdx, false);
    SetSensorImplementedByIdx(kOcTemperatureSensorIdx, _bme280_present || _bmp280_present || _htu21d_present);
    SetSensorImplementedByIdx(kOcWindDirectionSensorIdx, _hmc5883_present);
    SetSensorImplementedByIdx(kOcWindGustSensorIdx, false);
    SetSensorImplementedByIdx(kOcWindSpeedSensorIdx, false);

    // init sensor and sensor data
    uint32_t system_time = millis();

    SetAveragePeriod(0.0);
    _update_time_ms = system_time;
    _refresh();

    AlpacaObservingConditions::Begin();
}

void ObservingConditions::Loop()
{
    if ((_update_time_ms + _refresh_time_ms) <= millis())
    {
        _refresh();
    }
    //AlpacaObservingConditions::Loop();
}

const bool ObservingConditions::_putAveragePeriodRequest(double average_period)
{
    // perform your specific test
    // https://ascom-standards.org/Help/Developer/html/P_ASCOM_DeviceInterface_IObservingConditions_AveragePeriod.htm
    if (average_period == 0.0) //
    {
        SetAveragePeriod(average_period);
        return true;
    }
    else
    {
        return false;
    }
}

void ObservingConditions::_refresh()
{
    _update_time_ms = millis();
    _last_read_ok = false;

    double temperature_c = NAN;
    double humidity_percent = NAN;
    double pressure_hpa = NAN;
    double magnetic_heading_deg = NAN;

    if (_bme280_present)
    {
        const double temperature = _bme280.readTemperature();
        const double humidity = _bme280.readHumidity();
        const double pressure = _bme280.readPressure() / 100.0;

        if (isfinite(temperature))
        {
            temperature_c = temperature;
        }
        if (isfinite(humidity))
        {
            humidity_percent = humidity;
        }
        if (isfinite(pressure))
        {
            pressure_hpa = pressure;
        }
    }

    if (_bmp280_present)
    {
        const double temperature = _bmp280.readTemperature();
        const double pressure = _bmp280.readPressure() / 100.0;

        if (!isfinite(temperature_c) && isfinite(temperature))
        {
            temperature_c = temperature;
        }
        if (!isfinite(pressure_hpa) && isfinite(pressure))
        {
            pressure_hpa = pressure;
        }
    }

    if (_htu21d_present)
    {
        const double temperature = _htu21d.readTemperature();
        const double humidity = _htu21d.readHumidity();

        if (!isfinite(temperature_c) && isfinite(temperature))
        {
            temperature_c = temperature;
        }
        if (!isfinite(humidity_percent) && isfinite(humidity))
        {
            humidity_percent = humidity;
        }
    }

    if (_hmc5883_present)
    {
        sensors_event_t event;
        _hmc5883.getEvent(&event);
        if (isfinite(event.magnetic.x) && isfinite(event.magnetic.y))
        {
            magnetic_heading_deg = _headingDeg(event.magnetic.x, event.magnetic.y);
        }
    }

    if (isfinite(temperature_c))
    {
        SetSensorValueByIdx(kOcTemperatureSensorIdx, temperature_c, _update_time_ms);
        _last_read_ok = true;
    }

    if (isfinite(humidity_percent))
    {
        SetSensorValueByIdx(kOcHumiditySensorIdx, humidity_percent, _update_time_ms);
        _last_read_ok = true;
    }

    if (isfinite(temperature_c) && isfinite(humidity_percent))
    {
        SetSensorValueByIdx(kOcDewPointSensorIdx, _dewPointC(temperature_c, humidity_percent), _update_time_ms);
    }

    if (isfinite(pressure_hpa))
    {
        SetSensorValueByIdx(kOcPressureSensorIdx, pressure_hpa, _update_time_ms);
        _last_read_ok = true;
    }

    if (isfinite(magnetic_heading_deg))
    {
        SetSensorValueByIdx(kOcWindDirectionSensorIdx, magnetic_heading_deg, _update_time_ms);
        _last_read_ok = true;
    }
}

double ObservingConditions::_dewPointC(double temperature_c, double humidity_percent) const
{
    if (humidity_percent <= 0.0)
    {
        return NAN;
    }

    const double a = 17.62;
    const double b = 243.12;
    const double gamma = log(humidity_percent / 100.0) + ((a * temperature_c) / (b + temperature_c));
    return (b * gamma) / (a - gamma);
}

double ObservingConditions::_headingDeg(double x, double y) const
{
    double heading_deg = atan2(y, x) * 180.0 / PI;
    heading_deg += _magnetic_declination_deg;
    while (heading_deg < 0.0)
    {
        heading_deg += 360.0;
    }
    while (heading_deg >= 360.0)
    {
        heading_deg -= 360.0;
    }
    return heading_deg;
}

void ObservingConditions::AlpacaReadJson(JsonObject &root)
{
    DBG_JSON_PRINTFJ(SLOG_INFO, root, "BEGIN (root=<%s>) ...\n", _ser_json_);
    AlpacaObservingConditions::AlpacaReadJson(root);

    if (JsonObject obj_config = root["ObservingConditionsConfiguration"])
    {
        RuntimeSettings::ReadJson(obj_config);
        const uint8_t sda_pin = obj_config["SDA"] | _sda_pin;
        const uint8_t scl_pin = obj_config["SCL"] | _scl_pin;
        const uint32_t i2c_clock_hz = obj_config["I2CClockHz"] | _i2c_clock_hz;
        const uint8_t htu21d_address = obj_config["HTU21DAddress"] | _htu21d_address;
        const uint8_t bme280_address = obj_config["BME280Address"] | _bme280_address;
        const uint8_t bmp280_address = obj_config["BMP280Address"] | _bmp280_address;
        _magnetic_declination_deg = obj_config["MagneticDeclinationDeg"] | _magnetic_declination_deg;
        const bool bus_changed = sda_pin != _sda_pin || scl_pin != _scl_pin || i2c_clock_hz != _i2c_clock_hz ||
                                 htu21d_address != _htu21d_address || bme280_address != _bme280_address ||
                                 bmp280_address != _bmp280_address;

        _sda_pin = sda_pin;
        _scl_pin = scl_pin;
        _i2c_clock_hz = i2c_clock_hz;
        _htu21d_address = htu21d_address;
        _bme280_address = bme280_address;
        _bmp280_address = bmp280_address;
        _refresh_time_ms = obj_config["RefreshIntervalMs"] | _refresh_time_ms;

        if (bus_changed)
        {
            Wire.begin(_sda_pin, _scl_pin);
            Wire.setClock(_i2c_clock_hz);
            _htu21d_present = _htu21d.begin(&Wire);
            _bme280_present = _bme280.begin(_bme280_address, &Wire);
            _bmp280_present = _bmp280.begin(_bmp280_address);
            _hmc5883_present = _hmc5883.begin();

            SetSensorImplementedByIdx(kOcDewPointSensorIdx, _bme280_present || _htu21d_present);
            SetSensorImplementedByIdx(kOcHumiditySensorIdx, _bme280_present || _htu21d_present);
            SetSensorImplementedByIdx(kOcPressureSensorIdx, _bme280_present || _bmp280_present);
            SetSensorImplementedByIdx(kOcTemperatureSensorIdx, _bme280_present || _bmp280_present || _htu21d_present);
            SetSensorImplementedByIdx(kOcWindDirectionSensorIdx, _hmc5883_present);
        }
    }
    else
    {
        SLOG_PRINTF(SLOG_WARNING, "... END    ... no ObservingConditionsConfiguration\n");
    }
}

// to be adapted
void ObservingConditions::AlpacaWriteJson(JsonObject &root)
{
    SLOG_PRINTF(SLOG_INFO, "BEGIN ...\n");
    AlpacaObservingConditions::AlpacaWriteJson(root);

    JsonObject obj_config = root["ObservingConditionsConfiguration"].to<JsonObject>();
    RuntimeSettings::WriteJson(obj_config);
    obj_config["SDA"] = _sda_pin;
    obj_config["SCL"] = _scl_pin;
    obj_config["I2CClockHz"] = _i2c_clock_hz;
    obj_config["HTU21DAddress"] = _htu21d_address;
    obj_config["BME280Address"] = _bme280_address;
    obj_config["BMP280Address"] = _bmp280_address;
    obj_config["MagneticDeclinationDeg"] = _magnetic_declination_deg;
    obj_config["RefreshIntervalMs"] = _refresh_time_ms;

    // #add # for read only
    JsonObject obj_states = root["#States"].to<JsonObject>();
    obj_states["HTU21DPresent"] = _htu21d_present;
    obj_states["BME280Present"] = _bme280_present;
    obj_states["BMP280Present"] = _bmp280_present;
    obj_states["HMC5883Present"] = _hmc5883_present;
    obj_states["LastReadOk"] = _last_read_ok;

    for (int i = kOcCloudCoverSensorIdx; i < kOcMaxSensorIdx; i++)
    {
        if (GetSensorImplementedByIdx((OCSensorIdx_t)i))
            obj_states[GetSensorNameByIdx((OCSensorIdx_t)i)] = GetSensorValueByIdx((OCSensorIdx_t)i);
    }

    DBG_JSON_PRINTFJ(SLOG_INFO, obj_states, "... END \n", _ser_json_);
}
