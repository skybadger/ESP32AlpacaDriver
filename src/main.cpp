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

  alpaca_server.RegisterCallbacks();
  alpaca_server.LoadSettings();

  // finalize logging setup
  g_Slog.Begin(alpaca_server.GetSyslogHost().c_str());
  SLOG_INFO_PRINTF("SYSLOG enabled and running log_lvl=%s enable_serial=%s\n", g_Slog.GetLvlMskStr().c_str(), alpaca_server.GetSerialLog() ? "true" : "false"); 
  g_Slog.SetLvlMsk(alpaca_server.GetLogLvl());
  g_Slog.SetEnableSerial(alpaca_server.GetSerialLog());

  last_loop_run_ms = millis();
  SLOG_PRINTF(SLOG_INFO, "Main loop soft timer initialized: %u ms interval\n", LOOP_INTERVAL_MS);

  registerMQTT();

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


