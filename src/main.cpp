/**************************************************************************************************
  Description:    ASCOM Alpaca ESP32 Server Test
  Copyright 2024-2025 peter_n@gmx.de. All rights reserved.
**************************************************************************************************/
#define VERSION "1.1.0"

// commend/uncommend to enable/disable device testsing with templates
//#define TEST_COVER_CALIBRATOR     // create CoverCalibrator device
#define TEST_SWITCH               // create Switch device
//#define TEST_OBSERVING_CONDITIONS // create ObservingConditions device
#define TEST_FOCUSER              // create Focuser device

// #define TEST_RESTART              // only for testing
#include "../include/UserConfig.h"
#include <SLog.h>
#include <AlpacaDebug.h>
#include <AlpacaServer.h>
#include <PubSubClient.h>

//NTP services
#include <time.h>
#include <sys/time.h>
time_t now; //use as 'gmtime(&now);


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

#include <time.h>
static constexpr uint32_t LOOP_INTERVAL_MS = 100;
static uint32_t last_loop_run_ms = 0;

// ASCOM Alpaca server with discovery
AlpacaServer alpaca_server(ALPACA_MNG_SERVER_NAME, ALPACA_MNG_MANUFACTURE, ALPACA_MNG_MANUFACTURE_VERSION, ALPACA_MNG_LOCATION);

WiFiClient mqtt_wifi_client;
PubSubClient mqtt_client(mqtt_wifi_client);
char mqtt_heartbeat_topic[96] = MQTT_DEFAULT_HEARTBEAT_TOPIC;
uint32_t last_mqtt_reconnect_ms = 0;
static constexpr uint32_t MQTT_RECONNECT_INTERVAL_MS = 5000;

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

void publishMqttStatus()
{
  alpaca_server.PublishMqttStatus(mqtt_client);
}

void mqttCallback(char* topic, byte* payload, unsigned int length)
{
  (void)payload;
  (void)length;

  if (strcmp(topic, mqtt_heartbeat_topic) == 0 || strstr(topic, "heartbeat") != nullptr)
  {
    SLOG_PRINTF(SLOG_INFO, "MQTT heartbeat received: %s\n", topic);
    publishMqttStatus();
  }
}

void configureMQTT()
{
  const char *host = nullptr;
  const char *user = nullptr;
  const char *password = nullptr;
  const char *heartbeat_topic = nullptr;
  uint16_t port = 0;

  if (!alpaca_server.GetMqttConnectionSettings(host, port, user, password, heartbeat_topic))
  {
    SLOG_PRINTF(SLOG_WARNING, "MQTT disabled or no valid MQTT host configured\n");
    return;
  }

  strlcpy(mqtt_heartbeat_topic, heartbeat_topic, sizeof(mqtt_heartbeat_topic));
  mqtt_client.setServer(host, port);
  mqtt_client.setCallback(mqttCallback);
  mqtt_client.setBufferSize(1200);
  SLOG_PRINTF(SLOG_INFO, "MQTT configured host=%s port=%u heartbeat=%s\n", host, port, mqtt_heartbeat_topic);
}

void serviceMQTT()
{
  if (WiFi.status() != WL_CONNECTED)
    return;

  if (!mqtt_client.connected())
  {
    uint32_t now_ms = millis();
    if ((uint32_t)(now_ms - last_mqtt_reconnect_ms) < MQTT_RECONNECT_INTERVAL_MS)
      return;

    last_mqtt_reconnect_ms = now_ms;

    const char *host = nullptr;
    const char *user = nullptr;
    const char *password = nullptr;
    const char *heartbeat_topic = nullptr;
    uint16_t port = 0;
    if (!alpaca_server.GetMqttConnectionSettings(host, port, user, password, heartbeat_topic))
      return;

    strlcpy(mqtt_heartbeat_topic, heartbeat_topic, sizeof(mqtt_heartbeat_topic));
    mqtt_client.setServer(host, port);

    char client_id[48] = {0};
    snprintf(client_id, sizeof(client_id), "%s-%s", HOSTNAME, alpaca_server.GetUID());

    bool connected = false;
    if (strlen(user) > 0)
      connected = mqtt_client.connect(client_id, user, password);
    else
      connected = mqtt_client.connect(client_id);

    if (connected)
    {
      mqtt_client.subscribe(mqtt_heartbeat_topic);
      SLOG_PRINTF(SLOG_INFO, "MQTT connected and subscribed to %s\n", mqtt_heartbeat_topic);
      publishMqttStatus();
    }
    else
    {
      SLOG_PRINTF(SLOG_WARNING, "MQTT connect failed state=%d\n", mqtt_client.state());
    }
  }

  mqtt_client.loop();
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
  
  //Start time
  configTime(TZ_SEC, DST_MN, timeServer1, timeServer2, timeServer3 );
  
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

  alpaca_server.RegisterCallbacks();
  alpaca_server.LoadSettings();

  // finalize logging setup
  g_Slog.Begin(alpaca_server.GetSyslogHost().c_str());
  SLOG_INFO_PRINTF("SYSLOG enabled and running log_lvl=%s enable_serial=%s\n", g_Slog.GetLvlMskStr().c_str(), alpaca_server.GetSerialLog() ? "true" : "false"); 
  g_Slog.SetLvlMsk(alpaca_server.GetLogLvl());
  g_Slog.SetEnableSerial(alpaca_server.GetSerialLog());

  last_loop_run_ms = millis();
  SLOG_PRINTF(SLOG_INFO, "Main loop soft timer initialized: %u ms interval\n", LOOP_INTERVAL_MS);

  configureMQTT();

}

void loop()
{
  static boolean loop_flag = false;

  #ifdef TEST_RESTART
  checkForRestart();
#endif

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
    serviceMQTT();
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
    loop_flag = false;
  }
}


