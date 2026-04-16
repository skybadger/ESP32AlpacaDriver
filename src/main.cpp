/**************************************************************************************************
  Description:    ASCOM Alpaca ESP32 Server Test
  Copyright 2024-2025 peter_n@gmx.de. All rights reserved.
**************************************************************************************************/
#define VERSION "1.1.0"

// commend/uncommend to enable/disable device testsing with templates
//#define TEST_COVER_CALIBRATOR     // create CoverCalibrator device
//#define TEST_SWITCH               // create Switch device
//#define TEST_OBSERVING_CONDITIONS // create ObservingConditions device
#define TEST_FOCUSER              // create Focuser device

// #define TEST_RESTART              // only for testing

// add your WIFI credentials and uncommend
// #define DEFAULT_SSID "my_ssid"
// #define DEFAULT_PWD "my_pwd"

#define DEFAULT_SSID "your_ssid"
#define DEFAULT_PWD "your_pwd"
#define HOSTNAME "ESP32AlpFoc1"

#ifndef DEFAULT_SSID 
#include "Credentials.h"
#endif

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
Focuser focuser1;
Focuser focuser2;
#endif

#include <time.h>
//Timer interrupts
hw_timer_t * timer2 = nullptr;
hw_timer_t * timer2 = nullptr;
volatile bool timer1_flag = false;
volatile bool timer2_flag = false;
IRAM_ATTR void onTimer1() 
{
  timer1_flag = true;
}

IRAM_ATTR void onTimer2() 
{
  timer2_flag = true;   
}

typedef struct { int pin; char* name } pinmap_t; 
pinmap_t pins[] = {
  { 0, "GPIO0" },
  { 1, "GPIO1" },
  { 2, "GPIO2" },
  { 3, "GPIO3" },
  { 4, "GPIO4" },
  { 5, "GPIO5" },
  { 6, "GPIO6" },
  { 7, "GPIO7" },
  { 8, "GPIO8" },
  { 9, "GPIO9" },
  {10, "GPIO10"},
  {11, "GPIO11"},
  {12, "GPIO12"},
  {13, "GPIO13"},
  {14, "GPIO14"},
  {15, "GPIO15"},
};


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

void setup()
{
  // setup logging and WiFi
  g_Slog.Begin(Serial, 115200);
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
  focuser1.Begin();
  focuser2.Begin();
  alpaca_server.AddDevice(&focuser1);
  alpaca_server.AddDevice(&focuser2);
#endif

  alpaca_server.RegisterCallbacks();
  alpaca_server.LoadSettings();

  // finalize logging setup
  g_Slog.Begin(alpaca_server.GetSyslogHost().c_str());
  SLOG_INFO_PRINTF("SYSLOG enabled and running log_lvl=%s enable_serial=%s\n", g_Slog.GetLvlMskStr().c_str(), alpaca_server.GetSerialLog() ? "true" : "false"); 
  g_Slog.SetLvlMsk(alpaca_server.GetLogLvl());
  g_Slog.SetEnableSerial(alpaca_server.GetSerialLog());

}

void loop()
{
#ifdef TEST_RESTART
  checkForRestart();
#endif

  if ( timer1_flag ) 
  {  
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
    focuser1.Loop();  
#endif
    timer1_flag = false;
  }

  if ( timer2_flag ) 
  { 
#ifdef TEST_FOCUSER
    // Process focuser timer-based operations (stepper control at 10 Hz)
    if (focuser1.IsTimerInterruptFlagged())
    {
      focuser1.ProcessTimerInterrupt();
      focuser1.ClearTimerInterruptFlag();
    }
    if (focuser2.IsTimerInterruptFlagged())
    {
      focuser2.ProcessTimerInterrupt();
      focuser2.ClearTimerInterruptFlag();
    }
#endif
    timer2_flag = false;
  }
}
