/**************************************************************************************************
  Description:    ASCOM Alpaca ESP32 Server Test
  Copyright 2024-2025 peter_n@gmx.de. All rights reserved.
**************************************************************************************************/
#define VERSION "1.2.0"

// commend/uncommend to enable/disable device testsing with templates
//#define TEST_COVER_CALIBRATOR     // create CoverCalibrator device
//#define TEST_OBSERVING_CONDITIONS // create ObservingConditions device
//#define TEST_FOCUSER              // create Focuser device

#ifdef ESP32_DOME
#define TEST_DOME                  // create Dome device
#elif defined(ESP32_OBSERVING_CONDITIONS)
#define TEST_OBSERVING_CONDITIONS  // create ObservingConditions device
#elif defined(ESP32_SWITCH)
#define TEST_SWITCH                // create Switch device
#elif defined(ESP32_C3_MLX90640)
#define TEST_THERMAL_CAMERA        // create MLX90640 Camera device
#else
#define TEST_SWITCH                // create Switch device
#endif

// #define TEST_RESTART              // only for testing

#include "../include/UserConfig.h"
#include <SLog.h>
#include <AlpacaDebug.h>
#include <AlpacaServer.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <WiFiMulti.h>

#ifdef TEST_COVER_CALIBRATOR
#include <CoverCalibrator.h>
CoverCalibrator coverCalibrator;
#endif

#ifdef TEST_SWITCH
#include <Switch.h>
Switch switchDevice;
#endif

#ifdef TEST_OBSERVING_CONDITIONS
#include <ObservingConditions.h>
ObservingConditions observingConditions;
#endif

#ifdef TEST_FOCUSER
#include <Focuser.h>
Focuser focuser1(0);
Focuser focuser2(1);
#endif

#ifdef TEST_THERMAL_CAMERA
#include <AuxSensorSwitch.h>
#include <ThermalCamera.h>
ThermalCamera thermalCamera;
AuxSensorSwitch auxSensorSwitch;
#endif

#ifdef TEST_DOME
#include <Dome.h>
Dome dome;
#endif

#include "RuntimeSettings.h"

// MQTT heartbeat support. Device JSON configuration enables the connection.
// Optional build flags can seed default values:
//   -D MQTT_HOST=\"192.168.1.10\"
//   -D MQTT_PORT=1883
//   -D MQTT_HEARTBEAT_TOPIC=\"observatory/heartbeat\"
//   -D MQTT_STATUS_TOPIC_PREFIX=\"observatory/esp32alptherm1\"
#ifndef MQTT_HOST
#define MQTT_HOST ""
#endif
#ifndef MQTT_PORT
#define MQTT_PORT 1883
#endif
#ifndef MQTT_USER
#define MQTT_USER ""
#endif
#ifndef MQTT_PWD
#define MQTT_PWD ""
#endif
#ifndef MQTT_HEARTBEAT_TOPIC
#define MQTT_HEARTBEAT_TOPIC "observatory/heartbeat"
#endif
#ifndef MQTT_STATUS_TOPIC_PREFIX
#define MQTT_STATUS_TOPIC_PREFIX "observatory/esp32alptherm1"
#endif
#ifndef MQTT_RECONNECT_INTERVAL_MS
#define MQTT_RECONNECT_INTERVAL_MS 5000
#endif

#define MQTT_ENABLED
WiFiClient mqttWifiClient;
PubSubClient mqttClient(mqttWifiClient);
uint32_t mqttLastReconnectAttemptMs = 0;
bool mqttConfigured = false;
String mqttHost = MQTT_HOST;
uint16_t mqttPort = MQTT_PORT;
String mqttUser = MQTT_USER;
String mqttPwd = MQTT_PWD;
String mqttHealthTopic = MQTT_HEARTBEAT_TOPIC;

typedef bool (*MqttHeartbeatStatusFn)(char *buffer, size_t buffer_size);

struct MqttHeartbeatDeviceRegistration
{
  String function_topic;
  MqttHeartbeatStatusFn status_fn;
};

constexpr size_t kMaxMqttHeartbeatDevices = 4;
MqttHeartbeatDeviceRegistration mqttHeartbeatDevices[kMaxMqttHeartbeatDevices];
size_t mqttHeartbeatDeviceCount = 0;

bool mqttThermalCameraStatus(char *buffer, size_t buffer_size);
bool mqttAuxSensorSwitchStatus(char *buffer, size_t buffer_size);
bool mqttDomeStatus(char *buffer, size_t buffer_size);
void registerMqttHeartbeatDevice(const char *status_topic, MqttHeartbeatStatusFn status_fn);
void configureMqttBroker(const char *host, uint16_t port, const char *user, const char *pwd, const char *health_topic);
void publishMqttHeartbeatStatus(const char *request_payload, size_t request_payload_len);
void mqttCallback(char *topic, byte *payload, unsigned int length);
void setupMqtt();
void loopMqtt();

#include <time.h>
static constexpr uint32_t LOOP_TIMER_FREQUENCY_HZ = 1000000;
static constexpr uint64_t LOOP_INTERVAL_US = 100000;

#ifndef WIFI_CONNECT_TIMEOUT_MS
#define WIFI_CONNECT_TIMEOUT_MS 20000
#endif

#ifndef WIFI_RECONNECT_INTERVAL_MS
#define WIFI_RECONNECT_INTERVAL_MS 30000
#endif

#ifndef NTP_RETRY_INTERVAL_MS
#define NTP_RETRY_INTERVAL_MS 60000
#endif

#ifndef WIFI_AP_FALLBACK_SSID
#define WIFI_AP_FALLBACK_SSID HOSTNAME "-setup"
#endif

#ifndef WIFI_AP_FALLBACK_PWD
#define WIFI_AP_FALLBACK_PWD "alpaca-setup"
#endif

hw_timer_t *loop_timer = nullptr;
volatile bool loop_timer_flag = false;
WiFiMulti wifi_multi;
bool wifi_fallback_ap_started = false;
bool ntp_configured = false;
uint32_t wifi_last_reconnect_attempt_ms = 0;
uint32_t ntp_last_attempt_ms = 0;
uint32_t ntp_last_deferred_log_ms = 0;

void IRAM_ATTR onLoopTimer()
{
  loop_timer_flag = true;
}

void setupLoopTimer()
{
  loop_timer = timerBegin(LOOP_TIMER_FREQUENCY_HZ);
  if (loop_timer == nullptr)
  {
    SLOG_ERROR_PRINTF("Failed to initialize loop timer\n");
    return;
  }

  timerAttachInterrupt(loop_timer, onLoopTimer);
  timerAlarm(loop_timer, LOOP_INTERVAL_US, true, 0);
  timerStart(loop_timer);

  SLOG_INFO_PRINTF("Loop timer initialized: %u us interval\n", static_cast<unsigned int>(LOOP_INTERVAL_US));
}

void logWifiAddress(const char *prefix, IPAddress ip)
{
  char wifi_ipstr[32] = "xxx.yyy.zzz.www";
  snprintf(wifi_ipstr, sizeof(wifi_ipstr), "%03d.%03d.%03d.%03d", ip[0], ip[1], ip[2], ip[3]);
  SLOG_INFO_PRINTF("%s %s\n", prefix, wifi_ipstr);
}

void startFallbackAccessPoint()
{
  if (wifi_fallback_ap_started)
  {
    return;
  }

  WiFi.mode(WIFI_AP_STA);
  if (WiFi.softAP(WIFI_AP_FALLBACK_SSID, WIFI_AP_FALLBACK_PWD))
  {
    wifi_fallback_ap_started = true;
    logWifiAddress("WiFi fallback AP started at", WiFi.softAPIP());
    SLOG_WARNING_PRINTF("WiFi station not connected; setup/OTA available on SSID=%s\n", WIFI_AP_FALLBACK_SSID);
  }
  else
  {
    SLOG_ERROR_PRINTF("Failed to start WiFi fallback AP SSID=%s\n", WIFI_AP_FALLBACK_SSID);
  }
}

void setupWifi()
{
  WiFi.persistent(false);
  WiFi.setHostname(HOSTNAME);
  WiFi.mode(WIFI_STA);

#ifdef DEFAULT_SSID
  wifi_multi.addAP(DEFAULT_SSID, DEFAULT_PWD);
#endif
#ifdef DEFAULT_SSID_2
  wifi_multi.addAP(DEFAULT_SSID_2, DEFAULT_PWD_2);
#endif
#ifdef DEFAULT_SSID_3
  wifi_multi.addAP(DEFAULT_SSID_3, DEFAULT_PWD_3);
#endif

  SLOG_INFO_PRINTF("Connecting to WiFi station networks for %u ms\n", static_cast<unsigned int>(WIFI_CONNECT_TIMEOUT_MS));
  const uint32_t start_ms = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - start_ms) < WIFI_CONNECT_TIMEOUT_MS)
  {
    wifi_multi.run(500);
    SLOG_INFO_PRINTF("Connecting to WiFi ..\n");
    delay(250);
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    logWifiAddress("WiFi station connected at", WiFi.localIP());
    return;
  }

  startFallbackAccessPoint();
}

const char *nullIfEmpty(const char *value)
{
  return (value == nullptr || value[0] == '\0') ? nullptr : value;
}

void setupNtpTime()
{
  if (ntp_configured)
  {
    return;
  }

  if (WiFi.status() != WL_CONNECTED)
  {
    const uint32_t now_ms = millis();
    if (ntp_last_deferred_log_ms == 0 || (now_ms - ntp_last_deferred_log_ms) >= NTP_RETRY_INTERVAL_MS)
    {
      ntp_last_deferred_log_ms = now_ms;
      SLOG_WARNING_PRINTF("NTP sync deferred until WiFi station is connected\n");
    }
    return;
  }

  const uint32_t now_ms = millis();
  if (ntp_last_attempt_ms != 0 && (now_ms - ntp_last_attempt_ms) < NTP_RETRY_INTERVAL_MS)
  {
    return;
  }
  ntp_last_attempt_ms = now_ms;

  setenv("TZ", RuntimeSettings::TimeZonePosix(), 1);
  tzset();
  configTzTime(RuntimeSettings::TimeZonePosix(),
               nullIfEmpty(RuntimeSettings::NtpServer1()),
               nullIfEmpty(RuntimeSettings::NtpServer2()),
               nullIfEmpty(RuntimeSettings::NtpServer3()));

  struct tm timeinfo;
  if (getLocalTime(&timeinfo, 2500))
  {
    char time_buffer[40] = {0};
    strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d %H:%M:%S %Z", &timeinfo);
    ntp_configured = true;
    SLOG_INFO_PRINTF("NTP synced: %s timezone=%s posix=%s\n",
                     time_buffer,
                     RuntimeSettings::TimeZoneName(),
                     RuntimeSettings::TimeZonePosix());
  }
  else
  {
    SLOG_WARNING_PRINTF("NTP sync started but no valid time received yet from %s/%s/%s\n",
                        RuntimeSettings::NtpServer1(),
                        RuntimeSettings::NtpServer2(),
                        RuntimeSettings::NtpServer3());
  }
}

void loopWifiRecovery()
{
  if (!wifi_fallback_ap_started || WiFi.status() == WL_CONNECTED)
  {
    if (wifi_fallback_ap_started && WiFi.status() == WL_CONNECTED)
    {
      WiFi.softAPdisconnect(true);
      WiFi.mode(WIFI_STA);
      wifi_fallback_ap_started = false;
      logWifiAddress("WiFi station recovered at", WiFi.localIP());
    }
    return;
  }

  const uint32_t now_ms = millis();
  if (now_ms - wifi_last_reconnect_attempt_ms < WIFI_RECONNECT_INTERVAL_MS)
  {
    return;
  }

  wifi_last_reconnect_attempt_ms = now_ms;
  SLOG_INFO_PRINTF("Retrying WiFi station connection while fallback AP is active\n");
  if (wifi_multi.run(500) == WL_CONNECTED)
  {
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    wifi_fallback_ap_started = false;
    logWifiAddress("WiFi station recovered at", WiFi.localIP());
    setupNtpTime();
  }
}


// ASCOM Alpaca server with discovery
AlpacaServer alpaca_server(ALPACA_MNG_SERVER_NAME, ALPACA_MNG_MANUFACTURE, ALPACA_MNG_MANUFACTURE_VERSION, ALPACA_MNG_LOCATION);

#ifdef TEST_RESTART
// ========================================================================
// SW Restart
bool restart = false;                          // enable/disable
uint32_t g_restart_start_time_ms = 0xFFFFFFFF; // Timer for countdown
uint32_t const k_RESTART_DELAY_MS = 10000;     // Restart Delay

/**
 * SetRestart
 */
void ActivateRestart()
{
  restart = true;
  g_restart_start_time_ms = millis();
}

/*
 */
void checkForRestart()
{
  if (alpaca_server.GetResetRequest() || restart)
  {
    uint32_t timer_ms = millis() - g_restart_start_time_ms;
    uint32_t coun_down_sec = (k_RESTART_DELAY_MS - timer_ms) / 1000;

    if (timer_ms >= k_RESTART_DELAY_MS)
    {
      ESP.restart();
    }
  }
  else
  {
    g_restart_start_time_ms = millis();
  }
}
#endif

#ifdef MQTT_ENABLED
bool mqttThermalCameraStatus(char *buffer, size_t buffer_size)
{
#ifdef TEST_THERMAL_CAMERA
  return thermalCamera.GetMqttHeartbeatJson(buffer, buffer_size);
#else
  return false;
#endif
}

bool mqttAuxSensorSwitchStatus(char *buffer, size_t buffer_size)
{
#ifdef TEST_THERMAL_CAMERA
  return auxSensorSwitch.GetMqttHeartbeatJson(buffer, buffer_size);
#else
  return false;
#endif
}

bool mqttDomeStatus(char *buffer, size_t buffer_size)
{
#ifdef TEST_DOME
  return dome.GetMqttHeartbeatJson(buffer, buffer_size);
#else
  return false;
#endif
}

void registerMqttHeartbeatDevice(const char *status_topic, MqttHeartbeatStatusFn status_fn)
{
  if (status_topic == nullptr || status_topic[0] == '\0')
  {
    SLOG_WARNING_PRINTF("MQTT heartbeat registration ignored empty function topic\n");
    return;
  }

  if (mqttHeartbeatDeviceCount >= kMaxMqttHeartbeatDevices)
  {
    SLOG_WARNING_PRINTF("MQTT heartbeat registration table full, cannot add %s\n", status_topic);
    return;
  }

  mqttHeartbeatDevices[mqttHeartbeatDeviceCount].function_topic = status_topic;
  mqttHeartbeatDevices[mqttHeartbeatDeviceCount].status_fn = status_fn;
  mqttHeartbeatDeviceCount++;
  SLOG_INFO_PRINTF("MQTT heartbeat registered %s\n", status_topic);
}

void configureMqttBroker(const char *host, uint16_t port, const char *user, const char *pwd, const char *health_topic)
{
  if (host == nullptr || host[0] == '\0')
  {
    return;
  }
  mqttHost = host;
  if (port > 0)
  {
    mqttPort = port;
  }
  if (user != nullptr)
  {
    mqttUser = user;
  }
  if (pwd != nullptr)
  {
    mqttPwd = pwd;
  }
  if (health_topic != nullptr && health_topic[0] != '\0')
  {
    mqttHealthTopic = health_topic;
  }
}

void mqttCallback(char *topic, byte *payload, unsigned int length)
{
  if (mqttHealthTopic.isEmpty() || strcmp(topic, mqttHealthTopic.c_str()) != 0)
  {
    return;
  }

  publishMqttHeartbeatStatus(reinterpret_cast<const char *>(payload), length);
}

void publishMqttHeartbeatStatus(const char *request_payload, size_t request_payload_len)
{
  char topic[128] = {0};
  char payload[512] = {0};
  char request_id[65] = {0};

  const size_t copy_len = min(request_payload_len, sizeof(request_id) - 1);
  memcpy(request_id, request_payload, copy_len);
  request_id[copy_len] = '\0';

  for (size_t device_idx = 0; device_idx < mqttHeartbeatDeviceCount; device_idx++)
  {
    const MqttHeartbeatDeviceRegistration &device = mqttHeartbeatDevices[device_idx];
    if ((device.status_fn != nullptr) && device.status_fn(payload, sizeof(payload)))
    {
      snprintf(topic, sizeof(topic), "%s", device.function_topic.c_str());
      mqttClient.publish(topic, payload, true);
    }
  }

  const String health_status_topic = mqttHealthTopic + "/status";
  snprintf(payload,
           sizeof(payload),
           "{\"host\":\"%s\",\"heartbeatTopic\":\"%s\",\"request\":\"%s\",\"uptimeMs\":%u,\"freeHeap\":%u,\"rssi\":%d}",
           HOSTNAME,
           mqttHealthTopic.c_str(),
           request_id,
           millis(),
           ESP.getFreeHeap(),
           WiFi.RSSI());
  snprintf(topic, sizeof(topic), "%s", health_status_topic.c_str());
  mqttClient.publish(topic, payload, true);
}

void setupMqtt()
{
  mqttHeartbeatDeviceCount = 0;
#ifdef TEST_THERMAL_CAMERA
  configureMqttBroker(thermalCamera.GetMqttHost(),
                      thermalCamera.GetMqttPort(),
                      thermalCamera.GetMqttUser(),
                      thermalCamera.GetMqttPassword(),
                      thermalCamera.GetMqttHealthTopic());
  configureMqttBroker(auxSensorSwitch.GetMqttHost(),
                      auxSensorSwitch.GetMqttPort(),
                      auxSensorSwitch.GetMqttUser(),
                      auxSensorSwitch.GetMqttPassword(),
                      auxSensorSwitch.GetMqttHealthTopic());
  registerMqttHeartbeatDevice(thermalCamera.GetMqttFunctionTopic(), mqttThermalCameraStatus);
  registerMqttHeartbeatDevice(auxSensorSwitch.GetMqttFunctionTopic(), mqttAuxSensorSwitchStatus);
#endif
#ifdef TEST_DOME
  configureMqttBroker(dome.GetMqttHost(),
                      dome.GetMqttPort(),
                      dome.GetMqttUser(),
                      dome.GetMqttPassword(),
                      dome.GetMqttHealthTopic());
  registerMqttHeartbeatDevice(dome.GetMqttFunctionTopic(), mqttDomeStatus);
#endif

  mqttConfigured = !mqttHost.isEmpty();
  if (!mqttConfigured)
  {
    SLOG_INFO_PRINTF("MQTT disabled: no MQTTHost configured\n");
    return;
  }

  mqttClient.setServer(mqttHost.c_str(), mqttPort);
  mqttClient.setCallback(mqttCallback);
  SLOG_INFO_PRINTF("MQTT heartbeat enabled host=%s port=%u health_topic=%s\n",
                   mqttHost.c_str(),
                   mqttPort,
                   mqttHealthTopic.c_str());
}

void loopMqtt()
{
  if (!mqttConfigured)
  {
    return;
  }

  if (WiFi.status() != WL_CONNECTED)
  {
    return;
  }

  if (!mqttClient.connected())
  {
    const uint32_t now_ms = millis();
    if (now_ms - mqttLastReconnectAttemptMs >= MQTT_RECONNECT_INTERVAL_MS)
    {
      mqttLastReconnectAttemptMs = now_ms;
      char client_id[64] = {0};
      snprintf(client_id, sizeof(client_id), "%s-%06X", HOSTNAME, static_cast<unsigned int>(ESP.getEfuseMac() & 0xFFFFFF));
      const bool connected = mqttUser.isEmpty()
                               ? mqttClient.connect(client_id)
                               : mqttClient.connect(client_id, mqttUser.c_str(), mqttPwd.c_str());
      if (connected)
      {
        mqttClient.subscribe(mqttHealthTopic.c_str());
        SLOG_INFO_PRINTF("MQTT connected and subscribed to %s\n", mqttHealthTopic.c_str());
      }
      else
      {
        SLOG_WARNING_PRINTF("MQTT connect failed state=%d\n", mqttClient.state());
      }
    }
  }
  else
  {
    mqttClient.loop();
  }
}
#endif

/*
MQTT registration is per-device not per driver soince only one port is available for receiving call backs. 
Let the server handle the callbacks and then dish them out to the devices via a new device::reportHealth() function

*/
void registerMQTT(void )
{
    //setup MQTT client - driver specific
/*
    WiFiClient espClient;
    PubSubClient client(espClient);
    client.setServer( _mqtt_server, _mqtt_port );
    client.connect( thisID, _mqtt_user, _mqtt_pwd ); 
    String lastWillTopic = _mqtt_health_topic; 
    lastWillTopic.concat( myHostname );
    client.connect( thisID, _mqtt_user, _mqtt_pwd , lastWillTopic.c_str(), 1, true, "Disconnected", false ); 
    //Create a heartbeat-based callback that causes this device to read the local i2C bus devices for data to publish.
    //TODO Update callback to replace with another that listens for the temperature data required for temp compensation  - set compEn false if not found. 
    client.setCallback( callback ); 
    client.subscribe( inTopic );
    client.subscribe(mqttTemperatureSource);
  */
 SLOG_PRINTF(SLOG_INFO, "Dummy MQTT client setup performed - update when ready\n");
}

void callback( String topic)
{
if ( topic.indexOf( "heartbeat" ) >= 0 )
  {
//enumerate devices and call their reporting function 
// consider adding to the registered callbacks handler
SLOG_PRINTF(SLOG_INFO, "Callback received: %s: \n", topic );
  }
}

void setup()
{
  // setup logging and WiFi
  
  // initialize SLog serial interface
  g_Slog.Begin( Serial0, 115200);
  //g_Slog.Begin( SYSLOG_HOST, 115200);
#ifdef LOLIN_S2_MINI  
  delay(5000); // time to detect USB device
#endif  

  SLOG_INFO_PRINTF("BigPet ESP32ALPACADeviceDemo started ...\n");

  setupWifi();
  if (WiFi.status() == WL_CONNECTED)
  {
    // initialize SLog host
    g_Slog.Begin(String(SYSLOG_HOST), 514);
  }

  // setup ESP32AlpacaDevices
  // 1. Init AlpacaServer
  // 2. Init and add devices
  // 3. Finalize AlpacaServer
  alpaca_server.Begin();

#ifdef TEST_COVER_CALIBRATOR
  coverCalibrator.Begin();
  alpaca_server.AddDevice(&coverCalibrator);
#endif

#ifdef TEST_SWITCH
  switchDevice.Begin();
  alpaca_server.AddDevice(&switchDevice);
#endif

#ifdef TEST_OBSERVING_CONDITIONS
  observingConditions.Begin();
  alpaca_server.AddDevice(&observingConditions);
#endif

#ifdef TEST_FOCUSER
  focuser1.Begin(  );
  alpaca_server.AddDevice(&focuser1);
  
  focuser2.Begin(  );
  alpaca_server.AddDevice(&focuser2);
#endif

#ifdef TEST_THERMAL_CAMERA
  thermalCamera.Begin();
  alpaca_server.AddDevice(&thermalCamera);
  auxSensorSwitch.Begin(&Wire);
  alpaca_server.AddDevice(&auxSensorSwitch);
#endif

#ifdef TEST_DOME
  dome.Begin();
  alpaca_server.AddDevice(&dome);
#endif

  alpaca_server.RegisterCallbacks();
  SLOG_INFO_PRINTF("OTA update endpoint enabled at /update\n");
  alpaca_server.LoadSettings();
  setupNtpTime();

  // finalize logging setup
  g_Slog.Begin(alpaca_server.GetSyslogHost().c_str());
  SLOG_INFO_PRINTF("SYSLOG enabled and running log_lvl=%s enable_serial=%s\n", g_Slog.GetLvlMskStr().c_str(), alpaca_server.GetSerialLog() ? "true" : "false"); 
  g_Slog.SetLvlMsk(alpaca_server.GetLogLvl());
  g_Slog.SetEnableSerial(alpaca_server.GetSerialLog());

#ifdef MQTT_ENABLED
  setupMqtt();
#endif

  setupLoopTimer();

}

void processLoopTimerEvent()
{
  loopWifiRecovery();
  setupNtpTime();

#ifdef TEST_RESTART
  checkForRestart();
#endif

  alpaca_server.Loop();
#ifdef TEST_COVER_CALIBRATOR
  coverCalibrator.Loop();
#endif

#ifdef TEST_SWITCH
  switchDevice.Loop();
#endif

#ifdef TEST_OBSERVING_CONDITIONS
  observingConditions.Loop();
#endif

#ifdef TEST_FOCUSER
  focuser1.Loop();
  focuser2.Loop();
  if (focuser1.IsTimerInterruptFlagged())
  {
    focuser1.ProcessTimerInterrupt();
  }
  if (focuser2.IsTimerInterruptFlagged())
  {
    focuser2.ProcessTimerInterrupt();
  }
#endif

#ifdef TEST_THERMAL_CAMERA
  thermalCamera.Loop();
  auxSensorSwitch.Loop();
#endif

#ifdef TEST_DOME
  dome.Loop();
#endif

#ifdef MQTT_ENABLED
  loopMqtt();
#endif
}

void loop()
{
  if (!loop_timer_flag)
  {
    return;
  }

  loop_timer_flag = false;
  processLoopTimerEvent();
}
