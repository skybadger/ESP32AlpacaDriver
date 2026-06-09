#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

namespace RuntimeSettings
{
const char *NtpServer1();
const char *NtpServer2();
const char *NtpServer3();
const char *TimeZoneName();
const char *TimeZonePosix();

void ReadJson(JsonObject &obj_config);
void WriteJson(JsonObject &obj_config);
}
