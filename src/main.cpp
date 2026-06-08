/**************************************************************************************************
  Description:    ASCOM Alpaca ESP32 Server Test
  Copyright 2024-2025 peter_n@gmx.de. All rights reserved.
**************************************************************************************************/
#define VERSION "1.2.0"

// commend/uncommend to enable/disable device testsing with templates
//#define TEST_COVER_CALIBRATOR     // create CoverCalibrator device
#define TEST_SWITCH               // create Switch device
//#define TEST_OBSERVING_CONDITIONS // create ObservingConditions device
//#define TEST_FOCUSER              // create Focuser device
#define TEST_THERMAL_CAMERA        // create MLX90640 Camera device

// #define TEST_RESTART              // only for testing

#include "../include/UserConfig.h"
#include <SLog.h>
#include <AlpacaDebug.h>
#include <AlpacaServer.h>
#include <PubSubClient.h>

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

#ifdef TEST_THERMALCAMERA

#endif 

#include <time.h>
static constexpr uint32_t LOOP_INTERVAL_MS = 100;
static uint32_t last_loop_run_ms = 0;

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

void registerMqttHeartbeatDevice(const char *status_topic, MqttHeartbeatStatusFn status_fn)
{
  if (mqttHeartbeatDeviceCount >= kMaxMqttHeartbeatDevices)
  {
    SLOG_WARNING_PRINTF("MQTT heartbeat registration table full, cannot add %s\n", status_topic);
    return;
  }

  mqttHeartbeatDevices[mqttHeartbeatDeviceCount].status_topic = status_topic;
  mqttHeartbeatDevices[mqttHeartbeatDeviceCount].status_fn = status_fn;
  mqttHeartbeatDeviceCount++;
  SLOG_INFO_PRINTF("MQTT heartbeat registered %s\n", status_topic);
}

void mqttCallback(char *topic, byte *payload, unsigned int length)
{
  if (strcmp(topic, MQTT_HEARTBEAT_TOPIC) != 0)
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
      snprintf(topic, sizeof(topic), "%s/%s", MQTT_STATUS_TOPIC_PREFIX, device.status_topic);
      mqttClient.publish(topic, payload, true);
    }
  }

  snprintf(payload,
           sizeof(payload),
           "{\"host\":\"%s\",\"heartbeatTopic\":\"%s\",\"request\":\"%s\",\"uptimeMs\":%u,\"freeHeap\":%u,\"rssi\":%d}",
           HOSTNAME,
           MQTT_HEARTBEAT_TOPIC,
           request_id,
           millis(),
           ESP.getFreeHeap(),
           WiFi.RSSI());
  snprintf(topic, sizeof(topic), "%s/device/status", MQTT_STATUS_TOPIC_PREFIX);
  mqttClient.publish(topic, payload, true);
}

void setupMqtt()
{
  mqttHeartbeatDeviceCount = 0;
#ifdef TEST_THERMAL_CAMERA
  registerMqttHeartbeatDevice("camera/0/status", mqttThermalCameraStatus);
  registerMqttHeartbeatDevice("switch/0/status", mqttAuxSensorSwitchStatus);
#endif

  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  SLOG_INFO_PRINTF("MQTT heartbeat enabled host=%s port=%u heartbeat_topic=%s status_prefix=%s\n",
                   MQTT_HOST,
                   MQTT_PORT,
                   MQTT_HEARTBEAT_TOPIC,
                   MQTT_STATUS_TOPIC_PREFIX);
}

void loopMqtt()
{
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
      if (mqttClient.connect(client_id))
      {
        mqttClient.subscribe(MQTT_HEARTBEAT_TOPIC);
        SLOG_INFO_PRINTF("MQTT connected and subscribed to %s\n", MQTT_HEARTBEAT_TOPIC);
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

void registerMqttHeartbeatDevice(const char *status_topic, MqttHeartbeatStatusFn status_fn)
{
  if (mqttHeartbeatDeviceCount >= kMaxMqttHeartbeatDevices)
  {
    SLOG_WARNING_PRINTF("MQTT heartbeat registration table full, cannot add %s\n", status_topic);
    return;
  }

  mqttHeartbeatDevices[mqttHeartbeatDeviceCount].status_topic = status_topic;
  mqttHeartbeatDevices[mqttHeartbeatDeviceCount].status_fn = status_fn;
  mqttHeartbeatDeviceCount++;
  SLOG_INFO_PRINTF("MQTT heartbeat registered %s\n", status_topic);
}

void mqttCallback(char *topic, byte *payload, unsigned int length)
{
  if (strcmp(topic, MQTT_HEARTBEAT_TOPIC) != 0)
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
      snprintf(topic, sizeof(topic), "%s/%s", MQTT_STATUS_TOPIC_PREFIX, device.status_topic);
      mqttClient.publish(topic, payload, true);
    }
  }

  snprintf(payload,
           sizeof(payload),
           "{\"host\":\"%s\",\"heartbeatTopic\":\"%s\",\"request\":\"%s\",\"uptimeMs\":%u,\"freeHeap\":%u,\"rssi\":%d}",
           HOSTNAME,
           MQTT_HEARTBEAT_TOPIC,
           request_id,
           millis(),
           ESP.getFreeHeap(),
           WiFi.RSSI());
  snprintf(topic, sizeof(topic), "%s/device/status", MQTT_STATUS_TOPIC_PREFIX);
  mqttClient.publish(topic, payload, true);
}

void setupMqtt()
{
  mqttHeartbeatDeviceCount = 0;
#ifdef TEST_THERMAL_CAMERA
  registerMqttHeartbeatDevice("camera/0/status", mqttThermalCameraStatus);
  registerMqttHeartbeatDevice("switch/0/status", mqttAuxSensorSwitchStatus);
#endif

  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  SLOG_INFO_PRINTF("MQTT heartbeat enabled host=%s port=%u heartbeat_topic=%s status_prefix=%s\n",
                   MQTT_HOST,
                   MQTT_PORT,
                   MQTT_HEARTBEAT_TOPIC,
                   MQTT_STATUS_TOPIC_PREFIX);
}

void loopMqtt()
{
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
      if (mqttClient.connect(client_id))
      {
        mqttClient.subscribe(MQTT_HEARTBEAT_TOPIC);
        SLOG_INFO_PRINTF("MQTT connected and subscribed to %s\n", MQTT_HEARTBEAT_TOPIC);
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

  WiFi.setHostname(HOSTNAME);
  WiFi.mode(WIFI_STA);
  WiFi.begin(DEFAULT_SSID, DEFAULT_PWD);

  while (WiFi.status() != WL_CONNECTED)
  {
    SLOG_INFO_PRINTF("Connecting to WiFi ..\n");
    delay(1000);
  }
  {
    IPAddress ip = WiFi.localIP();
    char wifi_ipstr[32] = "xxx.yyy.zzz.www";
    snprintf(wifi_ipstr, sizeof(wifi_ipstr), "%03d.%03d.%03d.%03d", ip[0], ip[1], ip[2], ip[3]);
    // initialize SLog host
    g_Slog.Begin(String(SYSLOG_HOST), 514);
    SLOG_INFO_PRINTF("connected with %s\n", wifi_ipstr);
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

  alpaca_server.RegisterCallbacks();
  alpaca_server.LoadSettings();

  // finalize logging setup
  g_Slog.Begin(alpaca_server.GetSyslogHost().c_str());
  SLOG_INFO_PRINTF("SYSLOG enabled and running log_lvl=%s enable_serial=%s\n", g_Slog.GetLvlMskStr().c_str(), alpaca_server.GetSerialLog() ? "true" : "false"); 
  g_Slog.SetLvlMsk(alpaca_server.GetLogLvl());
  g_Slog.SetEnableSerial(alpaca_server.GetSerialLog());

#ifdef MQTT_ENABLED
  setupMqtt();
#endif

}

void loop()
{
  static boolean loop_flag = false;

  #ifdef TEST_RESTART
  checkForRestart();
#endif

#ifdef TEST_THERMAL_CAMERA
  alpaca_server.Loop();
  thermalCamera.Loop();
  auxSensorSwitch.Loop();
#ifdef MQTT_ENABLED
  loopMqtt();
#endif
  delay(10);
#else

#ifdef TEST_THERMAL_CAMERA
  alpaca_server.Loop();
  thermalCamera.Loop();
  auxSensorSwitch.Loop();
#ifdef MQTT_ENABLED
  loopMqtt();
#endif
  delay(10);
#else

  uint32_t now_ms = millis();
  uint32_t duration = now_ms - last_loop_run_ms ;

  if ( duration >= LOOP_INTERVAL_MS ) 
  { 
    loop_flag = true; 
  }
  
  if ( loop_flag )
  {  
    last_loop_run_ms = now_ms;

    alpaca_server.Loop();
#ifdef TEST_COVER_CALIBRATOR
    coverCalibrator.Loop();
    delay(10);
#endif

#ifdef TEST_SWITCH
    switchDevice.Loop();
    delay(10);
#endif

#ifdef TEST_OBSERVING_CONDITIONS
    observingConditions.Loop();
    delay(10);
#endif

#ifdef TEST_FOCUSER
    focuser1.Loop();
    focuser2.Loop();
#endif

#ifdef TEST_THERMAL_CAMERA
    thermalCamera.Loop();
    auxSensorSwitch.Loop();
#ifdef MQTT_ENABLED
    loopMqtt();
#endif
#endif
    timer1_flag = false;
  }
#endif

  /* Check - should move to focuser code, we need this to be more responsive and capable of higher rates. 
  if ( timer2_flag ) 
  { 
#ifdef TEST_FOCUSER
    // Process focuser timer-based operations (stepper control at 10 Hz)
    if (focuser2.IsTimerInterruptFlagged())
    {
      focuser2.ProcessTimerInterrupt();
      focuser2.ClearTimerInterruptFlag();
    }
#endif
    timer2_flag = false;
  }
}


