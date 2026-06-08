//File to hold common device-specific details. 
#pragma once
typedef struct { int pin; const char* name; } pinmap_t; 
#if defined WEMOS_D1_MINI32
extern pinmap_t pins[];
#endif 
