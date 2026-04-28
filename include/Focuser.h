/**************************************************************************************************
  Filename:       Focuser.h
  Revised:        $Date: 2024-07-24$
  Revision:       $Revision: 01 $
  Description:    ASCOM Alpaca Focuser Device Template

  Copyright 2024-2025 peter_n@gmx.de. All rights reserved.
**************************************************************************************************/
#pragma once
#include <AlpacaFocuser.h>
#include <device.h>
#include <motor.h>

class Focuser : public AlpacaFocuser
{
  public:
  enum FocuserMode{ FOCUSER_MODE_ABSOLUTE, FOCUSER_MODE_RELATIVE };
  static const char* FocuserModech[2][] = { {"ABSOLUTE"}, {"RELATIVE"} };
  enum FocuserStates {  FOCUSER_INIT=0,  FOCUSER_IDLE, FOCUSER_MOVING,  FOCUSER_HALTED,  FOCUSER_STOPPING };
  static const char* FocuserStateCh[5][] = { {"INIT"}, {"IDLE"}, {"MOVING"}, {"HALTED"}, {"STOPPING"} };

  private:
  pinmap_t *_pins;
  size_t _num_pins; 
  void moveFocuser();
  Motor _myMotor();

  //Position defaults and limits. 
  const int32_t _motor_step_min_limit = 1;//system limit
  const int32_t _motor_step_min_default = 1;//system default 
  const int32_t _motor_step_max_limit = (int32_t) ( ( 2<<31)  - 1) ; //system limit
  const int32_t _motor_step_max_default = 100000; //system default 
  int32_t _motor_step_min = _motor_step_min_limit; //user specified 
  int32_t _motor_step_max = _motor_step_max_default; //user specified
    
  // Increment defaults and limits.
  const int32_t _increment_min = _motor_step_min_default;
  const int32_t _increment_max = _increment_min +( ( _motor_step_max_default - _motor_step_min_default ) /64 ) *64 ;
  const int32_t _increment_default = _motor_step_min_default;

  //Not sure I care for mm. 
  const int32_t _focuser_mm_min_limit = 0;
  const int32_t _focuser_mm_max_limit = 100;
  int32_t _focuser_mm_min = 0;
  int32_t _focuser_mm_max = 100;
    
  //FocuserStates tracks our current and requested focuser operational state. 
  //start with init at start for both
  FocuserStates _focuserState = FocuserStates::FOCUSER_INIT;
  FocuserStates _targetFocuserState = FocuserStates::FOCUSER_INIT;
  FocuserMode _focuser_mode = FOCUSER_MODE_ABSOLUTE;
  
  //Absolute vs relative 
  int32_t _base_absolute_count = 0;
  bool _absolute_mode = true;
 
  //Backlash compensation parameters
  const bool _backlash_comp_default = false;
  const bool _backlash_comp_default = DIRN_CW;
  const bool _outrack_direction_default = DIRN_CW;
  bool _backlash_comp = _backlash_comp_default;
  bool _backlashDirection = _backlash_comp_default; // true = outward, false = inward
  bool _outrack_direction = _outrack_direction_default;
  int32_t _backlash_size = 0;

  //Temperature and dew monitoring for temp compensation
  const bool _k_temp_comp_available = false;
  int32_t** _temp_comp = nullptr; //Sorted list of temp compensation values for different temperatures; to be defined by specific implementation if temp comp is supported.
  bool _temp_compen_enabled = false;
  double _temperature = 99.0;

  bool _is_moving = false;
  uint32_t _position = 0;
  uint32_t _target_position = _position;
    
  // Timer interrupt handling for time-based operations
  hw_timer_t* _timer = nullptr;
  volatile bool _timer_interrupt_flag = false;
  const uint32_t _TIMER_DIVIDER = 80;  // 80 MHz / 80 = 1 MHz resolution
  const uint32_t _TIMER_INTERVAL_US = 100000; // 100ms interrupt interval (10 Hz)
  
  //MQTT parameters and flags
  String _mqtt_server;
  unsigned int  _mqtt_port;
  String _mqtt_user;
  String _mqtt_pwd;
  String _mqtt_health_topic;
  String _mqtt_function_topic;
  volatile bool _callbackFlag = false;

  // Alpaca command handlers
  void AlpacaReadJson(JsonObject &root);
  void AlpacaWriteJson(JsonObject &root);

  const bool _putTempComp(bool temp_comp) { return true; };
  const bool _putHalt();
  const bool _putMove(int32_t target_position_steps);

  // optional Alpaca service: to be implemented if needed
  const bool _putAction(const char *const action, const char *const parameters, char *string_response, size_t string_response_size) { return false; }
  const bool _putCommandBlind(const char *const command, const char *const raw, bool &bool_response) { return false; };
  const bool _putCommandBool(const char *const command, const char *const raw, bool &bool_response) { return false; };
  const bool _putCommandString(const char *const command_str, const char *const raw, char *string_response, size_t string_response_size) { return false; };

  const bool _getAbsolute() { return _focuser_mode == FocuserMode::FOCUSER_MODE_ABSOLUTE; };
  const bool _getIsMoving() { return _is_moving; };
  const int32_t _getMaxIncrement() { return _max_increment; };
  const int32_t _getMaxStep() { return _max_motor_step; };
  const int32_t _getPosition() { return _position; };
  const double _getStepSize() { return _step_size; };
  const bool _getTempComp() { return _temp_comp; };
  const bool _getTempCompAvailable() { return _temp_comp_available; };
  const double _getTemperature() { return _temperature; };

  //Extra functions to manage focuser state and configuration
  const bool setReverse(bool reverse) { _reverse = reverse; return _reverse; };
  const bool _setBacklashEnabled( bool backlash_enabled ) {  _backlash_comp = backlash_enabled; return true; };
  const bool _setBacklashSize( int32_t backlash_size) { _backlash_size = backlash_size; };
  const bool _setBacklashDirection( int32_t backlash_direction) { _backlashDirection = backlash_direction; return _backlashDirection; };
  const bool _getBacklashEnabled() { return _backlash_comp; };
  const int32_t _getBacklashSize() { return _backlash_size; };  
  const int32_t _getBacklashDirection() { return _backlashDirection; };


  int Focuser::manageFocuserState( FocuserStates targetFocuserState );

  public:
  Focuser();
  // Override constructor to pass pin configuration for specific focuser implementation; e.g. for stepper motor driver  
  Focuser(pinmap_t *pins, size_t num_pins): _pins(pins), _num_pins(num_pins) {  };

  void setMQTT( String mqtt_server, uint16_t mqtt_port, String mqtt_user, String mqtt_pwd, String health_topic, String function_topic ) : _mqtt_server(mqtt_server), _mqtt_port(mqtt_port), _mqtt_user(mqtt_user), _mqtt_pwd(mqtt_pwd), _mqtt_health_topic(health_topic), _mqtt_function_topic(function_topic) {} ;
  void Begin();
  void Loop();
  
  // Static callback bridge for timer interrupt
  static void IRAM_ATTR _timerInterruptStatic();
  void IRAM_ATTR _timerInterruptHandler();

  
  
  // Timer management methods
  void InitTimer();
  void StopTimer();
  void StartTimer();
  bool IsTimerInterruptFlagged() const { return _timer_interrupt_flag; }
  void ClearTimerInterruptFlag() { _timer_interrupt_flag = false; }
  void ProcessTimerInterrupt();

  //MQTT support methods
  void MQTTCallback(char* topic, byte* payload, unsigned int length);

};