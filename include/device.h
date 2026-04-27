//File to hold common device-specific details. 
#pragma once
typedef struct { int pin; const char* name; } pinmap_t; 
#if defined WEMOS_D1_MINI32
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
#endif 