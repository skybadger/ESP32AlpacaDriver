# ESP32 Alpaca Driver

ESP32 ASCOM Alpaca firmware for several observatory devices. The project is built with PlatformIO and currently produces separate binaries for:

- an ESP32-C3 MLX90640 thermal camera with auxiliary MLX90614/TSL2591 sensor switch
- a Wemos D1 Mini ESP32 generic switch/demo target
- a Wemos D1 Mini ESP32 observing-conditions controller
- a Wemos D1 Mini ESP32 Alpaca dome controller

All active ESP32 targets use the shared 4 MB LittleFS/OTA partition file:

```text
partitions/esp32_4mb_littlefs_ota.csv
```

## Build Targets

| PlatformIO env | Board | Firmware device(s) | Selection flag |
| --- | --- | --- | --- |
| `esp32_c3_mlx90640` | `esp32-c3-devkitm-1` | Alpaca `camera/0` thermal camera plus `switch/0` auxiliary sensors | `ESP32_C3_MLX90640` |
| `wemos_d1_mini32` | `wemos_d1_mini32` | Legacy broad Wemos demo build, currently Alpaca `switch/0` | default when no device-specific flag is set |
| `esp32_switch` | `wemos_d1_mini32` | Alpaca `switch/0` generic/demo switch | `ESP32_SWITCH` |
| `esp32_observing_conditions` | `wemos_d1_mini32` | Alpaca `observingconditions/0` using HTU21D/BME280/BMP280 I2C sensors | `ESP32_OBSERVING_CONDITIONS` |
| `esp32_dome` | `wemos_d1_mini32` | Alpaca `dome/0` controller | `ESP32_DOME` |

The active device is selected in [src/main.cpp](src/main.cpp) from the build flags supplied by [platformio.ini](platformio.ini). To add another firmware image, add a new `[env:...]` section with a unique build flag, then extend the selection block in `main.cpp`.

## Building

Build the default target:

```bash
pio run
```

Build a specific binary:

```bash
pio run -e esp32_c3_mlx90640
pio run -e wemos_d1_mini32
pio run -e esp32_switch
pio run -e esp32_observing_conditions
pio run -e esp32_dome
```

Build all active targets:

```bash
pio run -e esp32_c3_mlx90640 -e esp32_switch -e esp32_observing_conditions -e esp32_dome
```

Upload a specific firmware:

```bash
pio run -e esp32_dome -t upload
```

Build and upload the LittleFS image before first use, or after changing web/setup assets:

```bash
pio run -e esp32_dome -t buildfs
pio run -e esp32_dome -t uploadfs
```

Change the default build by editing:

```ini
[platformio]
default_envs = esp32_c3_mlx90640
```

## Testing

Compile checks:

```bash
pio run -e esp32_c3_mlx90640 -e esp32_switch -e esp32_observing_conditions -e esp32_dome
```

Thermal camera API test:

```bash
scripts/test_thermal_camera.sh esp32alptherm1.local
```

Dome API test:

```bash
scripts/test_dome_openapi.sh esp32dome.local
```

The test scripts write command logs and JSON responses under `test-results/<timestamp>/`.

The dome script configures the dome via `/setup/v1/dome/0/jsondata`, saves settings, connects an Alpaca client, exercises shutter and slew commands, then disconnects. Remote devices can be supplied by environment:

```bash
DOME_ENCODER_HOST=espencoder.local \
DOME_SHUTTER_HOST=espshutter.local \
scripts/test_dome_openapi.sh esp32dome.local
```

Useful dome test overrides:

```bash
DOME_TEST_SLEW_TARGET_1=90
DOME_TEST_SLEW_TARGET_2=270
DOME_TEST_WAIT_ATTEMPTS=120
DOME_TEST_WAIT_DELAY=0.25
```

ASCOM Conform Universal and ASCOM clients such as NINA can also be used against the advertised Alpaca devices.

## Real vs Simulated Hardware

### Dome

The dome firmware can run with real or simulated remote components from the device JSON settings:

- `EncoderHost` blank: no remote encoder is polled. Dome azimuth is advanced internally during slews using `SimulatedSlewStepDeg`.
- `EncoderHost` set: the dome polls `GET http://<EncoderHost>[:EncoderPort]/bearing` and expects JSON containing `bearing`.
- `ShutterHost` blank: shutter state is cached locally, and open/close commands update that cached value.
- `ShutterHost` set: the dome polls `GET http://<ShutterHost>[:ShutterPort]/status` and expects JSON containing numeric `status`. Open/close/abort use `PUT /shutter` with form values `shutter=open`, `shutter=close`, or `shutter=abort`.
- `MotorAddress`, `SDA`, `SCL`, and `I2CClockHz` select the local I2C DC motor controller.

These values are stored under `DomeConfiguration` by the setup JSON reader/writer.

### Thermal Camera And Auxiliary Sensors

The `esp32_c3_mlx90640` build expects:

- MLX90640 thermal array on I2C for Alpaca camera frames
- optional MLX90614 object/ambient temperature sensor
- optional TSL2591 lux sensor

Sensor presence and read status are reported in setup JSON state and MQTT health. Missing auxiliary sensors do not stop the firmware from running; missing MLX90640 means camera image data will not become ready.

### Observing Conditions

The `esp32_observing_conditions` build reads local I2C weather sensors:

- HTU21D/HTU21DF for humidity and temperature
- BME280 for humidity, temperature, and barometric pressure
- BMP280 for temperature and barometric pressure
- HMC5883L for magnetic compass heading, exposed as ObservingConditions `WindDirection`

The setup JSON section `ObservingConditionsConfiguration` stores `SDA`, `SCL`, `I2CClockHz`, `HTU21DAddress`, `BME280Address`, `BMP280Address`, `MagneticDeclinationDeg`, and `RefreshIntervalMs`. BME280 readings are preferred where available, BMP280 supplies pressure/temperature fallback, and HTU21D supplies humidity/temperature fallback. Dew point is calculated from temperature and humidity. HMC5883 heading includes `MagneticDeclinationDeg` before being normalised to 0-360 degrees.

Note: HTU21D is a humidity/temperature part, not a magnetic sensor. HMC5883L is the magnetic sensor.

### Generic Switch

The `wemos_d1_mini32` target is still mostly a demo switch implementation. Switch 1 temperature and switch 2 door state are simulated from `millis()`, and writable channels only update internal state. Replace `Switch::Loop()` and `_writeSwitchValue()` with GPIO or device-specific code for real hardware.

## Runtime Configuration

Device setup JSON is available at:

```text
GET  http://<host>/setup/v1/<device>/<number>/jsondata
POST http://<host>/setup/v1/<device>/<number>/jsondata
GET  http://<host>/save_settings
```

OTA firmware update support is provided by ElegantOTA through the Alpaca web server:

```text
GET http://<host>/update
```

At boot the firmware tries the configured station WiFi networks using `WiFiMulti`. If none connect within `WIFI_CONNECT_TIMEOUT_MS`, it starts a fallback setup access point using `WIFI_AP_FALLBACK_SSID` and `WIFI_AP_FALLBACK_PWD` from [include/UserConfig.h](include/UserConfig.h). The loop keeps retrying station WiFi and shuts down the fallback AP after a station connection recovers.

NTP time synchronisation runs after device settings load and whenever station WiFi recovers. The following fields are stored with device setup JSON:

- `NTPServer1`
- `NTPServer2`
- `NTPServer3`
- `TimeZoneName`
- `TimeZonePosix`

The default timezone is `Europe/London` with POSIX value `GMT0BST,M3.5.0/1,M10.5.0`.

MQTT settings live with each device configuration:

- `MQTTHost`
- `MQTTPort`
- `MQTTUser`
- `MQTTPwd`
- `MQTTHealthTopic`
- `MQTTFunctionTopic`

MQTT is disabled when `MQTTHost` is blank. The main MQTT callback subscribes to the health topic and publishes each registered device status to its configured function topic.

## Known Static Or Incomplete Areas

These are intentional placeholders or first-pass implementations that need real data, more complete ASCOM behaviour, or hardware-specific work:

- Dome altitude is not implemented: `altitude`, `slewtoaltitude`, and `cansetaltitude` return not implemented or false. A real shutter altitude source/command path is needed.
- Dome slaving is not implemented: `canslave` is false and setting `Slaved=true` returns not implemented.
- Dome shutter commands assume the remote shutter API from the original ESP8266 project. There is no retry queue yet; failed remote commands are reported only through cached state and last HTTP code.
- Dome encoder polling expects `bearing` only. There is no local encoder implementation or magnetometer reset/freeze recovery yet.
- Dome at-home/at-park depends on configured positions and current azimuth; there are no physical home/park sensors.
- Dome motor movement has no stall detection or closed-loop safety beyond encoder movement and target distance.
- Observing conditions currently implements temperature, humidity, pressure, calculated dew point, and HMC5883 magnetic heading exposed as wind direction. Cloud cover, rain rate, sky brightness, sky quality, sky temperature, star FWHM, wind gust, and wind speed are marked not implemented until external sensors are added.
- Thermal camera implements a minimal Alpaca camera surface. Cooling, gain/offset, pulse guiding, fast readout, asymmetric binning, CCD temperature control, and heat sink temperature are static or not implemented.
- Thermal camera `LastExposureStartTime` currently returns the ASCOM unknown timestamp.
- Thermal camera pixel size, full well capacity, electrons per ADU, max ADU, gains, offsets, and readout modes are fixed assumptions.
- Generic switch target is a simulation/demo and does not drive real GPIO.
- Focuser temperature compensation currently accepts settings without a real external temperature feedback path.
- Cover calibrator and observing conditions sources remain template/example devices unless wired into specific hardware.

## Useful Files

- [platformio.ini](platformio.ini): build environments and source filters
- [src/main.cpp](src/main.cpp): target selection, Alpaca server startup, MQTT loop, shared timer loop
- [src/Dome.cpp](src/Dome.cpp): Alpaca dome implementation and remote encoder/shutter polling
- [src/I2CMotor.h](src/I2CMotor.h): I2C DC motor implementation of the generic motor interface
- [src/ThermalCamera.cpp](src/ThermalCamera.cpp): MLX90640 camera implementation
- [src/AuxSensorSwitch.cpp](src/AuxSensorSwitch.cpp): MLX90614/TSL2591 switch implementation
- [scripts/test_dome_openapi.sh](scripts/test_dome_openapi.sh): dome API exercise script
- [scripts/test_thermal_camera.sh](scripts/test_thermal_camera.sh): thermal camera API exercise script
