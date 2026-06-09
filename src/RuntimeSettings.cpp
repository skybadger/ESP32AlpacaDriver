#include "RuntimeSettings.h"

#include "../include/UserConfig.h"

#ifndef DEFAULT_NTP_SERVER_1
#define DEFAULT_NTP_SERVER_1 "pool.ntp.org"
#endif

#ifndef DEFAULT_NTP_SERVER_2
#define DEFAULT_NTP_SERVER_2 "time.nist.gov"
#endif

#ifndef DEFAULT_NTP_SERVER_3
#define DEFAULT_NTP_SERVER_3 "time.google.com"
#endif

#ifndef DEFAULT_TIMEZONE_NAME
#define DEFAULT_TIMEZONE_NAME "Europe/London"
#endif

#ifndef DEFAULT_TIMEZONE_POSIX
#define DEFAULT_TIMEZONE_POSIX "GMT0BST,M3.5.0/1,M10.5.0"
#endif

namespace
{
String ntp_server_1 = DEFAULT_NTP_SERVER_1;
String ntp_server_2 = DEFAULT_NTP_SERVER_2;
String ntp_server_3 = DEFAULT_NTP_SERVER_3;
String timezone_name = DEFAULT_TIMEZONE_NAME;
String timezone_posix = DEFAULT_TIMEZONE_POSIX;
}

namespace RuntimeSettings
{
const char *NtpServer1() { return ntp_server_1.c_str(); }
const char *NtpServer2() { return ntp_server_2.c_str(); }
const char *NtpServer3() { return ntp_server_3.c_str(); }
const char *TimeZoneName() { return timezone_name.c_str(); }
const char *TimeZonePosix() { return timezone_posix.c_str(); }

void ReadJson(JsonObject &obj_config)
{
  ntp_server_1 = obj_config["NTPServer1"] | ntp_server_1;
  ntp_server_2 = obj_config["NTPServer2"] | ntp_server_2;
  ntp_server_3 = obj_config["NTPServer3"] | ntp_server_3;
  timezone_name = obj_config["TimeZoneName"] | timezone_name;
  timezone_posix = obj_config["TimeZonePosix"] | timezone_posix;
}

void WriteJson(JsonObject &obj_config)
{
  obj_config["NTPServer1"] = ntp_server_1;
  obj_config["NTPServer2"] = ntp_server_2;
  obj_config["NTPServer3"] = ntp_server_3;
  obj_config["TimeZoneName"] = timezone_name;
  obj_config["TimeZonePosix"] = timezone_posix;
}
}
