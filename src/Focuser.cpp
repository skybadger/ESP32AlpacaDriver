/**************************************************************************************************
  Filename:       Focuser.cpp
  Revised:        $Date: 2024-07-24$
  Revision:       $Revision: 01 $
  Description:    ASCOM Alpaca Focuser Device Template

  Copyright 2024-2025 peter_n@gmx.de. All rights reserved.
**************************************************************************************************/
#include <debug_internal.h>
#include <Focuser.h>
#include <PubSubClient.h>

// Static instance pointer for timer callback bridge
static Focuser* g_focuser_instance = nullptr;

// Static timer callback function (bridge to class method)
void IRAM_ATTR Focuser::_timerInterruptStatic()
{
  if (g_focuser_instance != nullptr)
  {
    g_focuser_instance->_timerInterruptHandler();
  }
}

Focuser::Focuser() : AlpacaFocuser()
{
}

void Focuser::Begin( )
{
    // init Focuser

    AlpacaFocuser::Begin();
       
    // Initialize timer interrupt for time-based stepper control
    InitTimer();

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
}

//Loop is managed via timer interrupts in main. 
//Need a second timer to handle stepper stepping. 
void Focuser::Loop()
{   
    if( _focuserState == FocuserStates::FOCUSER_MOVING )
      SLOG_PRINTF(SLOG_DEBUG, "focuser position : %d\n", _position); 
    
    manageFocuserState( _targetFocuserState );

    //TODO manage MQTT client loop and reconnect if needed
    if ( /*client.connected()*/ false ) 
    {
        //client.loop();
    } 
    else 
    {
        // Attempt to reconnect
        //if (client.connect( thisID, _mqtt_user, _mqtt_pwd )) {
        //  client.subscribe(inTopic);
        //}
    }
}

/**
 * InitTimer - Initialize ESP32 hardware timer for focuser time-based operations
 * Timer runs at 100ms intervals (10 Hz) to drive stepper motor control
 */
void Focuser::InitTimer()
{
    // Set this instance as the global reference for the static callback
    g_focuser_instance = this;
    
    // Create a 1 MHz timer. Arduino ESP32 3.x uses frequency rather than timer ID/divider.
    _timer = timerBegin(1000000);
    
    if ( _timer != nullptr)
    {
        // Attach the interrupt handler
        timerAttachInterrupt( _timer, &_timerInterruptStatic);
        
        // Set the timer to interrupt at _TIMER_INTERVAL_US microseconds
        // The true parameter sets it to repeat (auto-reload), 0 means unlimited reloads.
        timerAlarm(_timer, _TIMER_INTERVAL_US, true, 0);
        
        timerStop(_timer);
        
        SLOG_PRINTF(SLOG_INFO, "Focuser timer initialized: %u us interval\n", _TIMER_INTERVAL_US);
    }
    else
    {
        SLOG_PRINTF(SLOG_ERROR, "Failed to initialize focuser timer\n");
    }
}

//Do this when we want to start moving the focuser - e.g. from idle to moving state.
void Focuser::StartTimer()
{
    if ( _timer != nullptr)
    {
        timerRestart(_timer);
        timerStart(_timer);
        SLOG_PRINTF(SLOG_INFO, "Focuser timer started\n");
    }
}   

/**
 * StopTimer - Stop the ESP32 hardware timer
 */
void Focuser::StopTimer()
{
    if ( _timer != nullptr)
    {
        timerStop(_timer);
        SLOG_PRINTF(SLOG_INFO, "Focuser timer stopped\n");
    }
}

/**
 * _timerInterruptHandler - ISR-level handler for timer interrupts
 * Sets flag for main loop processing (actual work is done in ProcessTimerInterrupt)
 */
void IRAM_ATTR Focuser::_timerInterruptHandler()
{
    const bool moving_out = _target_position > _position;
    _myMotor.step(moving_out ? _focuser_direction : !_focuser_direction); 
    _position = moving_out ? _position + 1 : _position - 1;
    if ( _position == _target_position )
    {
      StopTimer();
      _focuserState = FocuserStates::FOCUSER_HALTED;
    }
    _timer_interrupt_flag = true;
}

/**
 * ProcessTimerInterrupt - Called from main loop to process timer-based focuser operations
 * This runs at 10 Hz and drives stepper motor movements
 */
void Focuser::ProcessTimerInterrupt()
{
    if (!_timer_interrupt_flag)
        return;
    
    _timer_interrupt_flag = false;
    
    // Run the state machine every 100ms
    manageFocuserState( _targetFocuserState );
}

//Function to manage focuser states
 int Focuser::manageFocuserState( FocuserStates targetFocuserState )
 {
  String msg = "";
  //need a targetFocuserState.
  if ( _focuserState == targetFocuserState ) 
    return 0;
   
  switch( _focuserState )
  {          
      //current status is Closed, new target status is asking for a different action. 
      case FocuserStates::FOCUSER_MOVING:
          //current status is moving, new target status is asking for a different action. 
          switch (targetFocuserState)
          {
            //If we change these target states while moving, the timer handler managing movement will respond automatically
            case FocuserStates::FOCUSER_IDLE:
                  SLOG_PRINTF(SLOG_DEBUG, "Still moving  - %s to %s\n", FocuserStateCh[_focuserState], FocuserStateCh[targetFocuserState] );
                  //No change
                  break;
            case FocuserStates::FOCUSER_HALTED:
                  SLOG_PRINTF(SLOG_DEBUG, "Change of state requested from Moving to Halted - halting Focuser if not already stopped\n");
                  //Stop the motor interrupt timer handler.   
                  StopTimer();
                  _focuserState = FocuserStates::FOCUSER_HALTED;
                  break;
            case FocuserStates::FOCUSER_MOVING:
                  SLOG_PRINTF(SLOG_DEBUG, "Change of state requested from Moving to Halted - halting Focuser if not already stopped\n");
                  StopTimer();
                  _focuserState = FocuserStates::FOCUSER_HALTED;
                  break;
            default:
                  SLOG_PRINTF(SLOG_WARNING, "Requested target state of %s while moving - does it make sense ?\n", FocuserStateCh[targetFocuserState] ); 
                  break;
          }
          break;
      //Used to be Halted but Halted is not an ASCOM value - unknown should be returned as the state when the motor is halted during movement or before initialised
      case FocuserStates::FOCUSER_IDLE: 
          switch( targetFocuserState )
          {
            case FocuserStates::FOCUSER_IDLE:
                 SLOG_PRINTF(SLOG_DEBUG, "targetFocuserState changed to IDLE from IDLE\n");
                 break;
            case FocuserStates::FOCUSER_MOVING:
                 SLOG_PRINTF(SLOG_DEBUG, "targetFocuserState changed to MOVING from IDLE. Starting Focuser\n");
                 //TODO - need to manage the position and direction of movement here.
                 
                 //_moveFocuser();
                 
                 //turn on the timer to move the servo smoothly
                 //StartTimer( &FocuserStateFlag );
                 _focuserState = FocuserStates::FOCUSER_MOVING;
                 break;
            case FocuserStates::FOCUSER_HALTED:
                 //Already idle. 
                 _putHalt();
            default:
                 SLOG_PRINTF(SLOG_WARNING, "Unexpected targetFocuserState %s from IDLE\n", FocuserStateCh[ targetFocuserState ] );
              break;
          }
          break;
      case FocuserStates::FOCUSER_HALTED: 
          switch( targetFocuserState )
          {
            case FocuserStates::FOCUSER_MOVING:
              SLOG_PRINTF(SLOG_DEBUG, "targetFocuserState set to MOVING from HALTED - position : %d\n", _position );
              _focuserState = FocuserStates::FOCUSER_MOVING;
              break;
            case FocuserStates::FOCUSER_HALTED:
              SLOG_PRINTF(SLOG_DEBUG, "targetFocuserState set to Halted from HALTED, Focuser status is: %d\n", _focuserState );
              break;
            case FocuserStates::FOCUSER_IDLE:
              SLOG_PRINTF(SLOG_DEBUG, "targetFocuserState set to IDLE from HALTED, position is %d\n", _position );
              _focuserState = FocuserStates::FOCUSER_IDLE;
              break;
            default:
              SLOG_PRINTF(SLOG_WARNING, "unexpected targetFocuserState from Open %s\n", FocuserStateCh[targetFocuserState]);
              break;
          }
      default:
          break;
  }
  //debugD( "Exiting - final state %s, targetFocuserState %s ", focuserStatusCh[focuserState], focuserStatusCh[targetfocuserState] );
  return 0;
}

void Focuser::AlpacaReadJson(JsonObject &root)
{
    DBG_JSON_PRINTFJ(SLOG_NOTICE, root, "BEGIN (root=<%s>) ...\n", _ser_json_);

    AlpacaFocuser::AlpacaReadJson(root);
    if (JsonObject obj_config = root["FocuserConfiguration"])
    {
        _position = obj_config["Position"] | _position;
        _absolute_mode = obj_config["MovementMode"] | _absolute_mode;
        _focuser_direction = obj_config["OutboundDirection"] | _focuser_direction;

        _motor_step_max = obj_config["UserMaxLimit"] | _motor_step_max;
        _motor_step_min = obj_config["UserMinLimit"] | _motor_step_min;
        _motor_step_increment = obj_config["UserStepSize"] | _motor_step_increment;
        
       
        _backlash_comp_available = obj_config["BacklashCompAvailable"] | _backlash_comp_available;
        _backlash_comp_enabled = obj_config["BacklashCompEnabled"] | _backlash_comp_enabled;
        _backlash_size = obj_config["BacklashSize"] | _backlash_size;
        _backlash_direction = obj_config["BacklashDirection"] | _backlash_direction;

        _temp_comp_enabled = obj_config["TempComp"] | _temp_comp_enabled;
        _temp_comp_available = obj_config["TempCompAvailable"] | _temp_comp_available;

        _mqtt_server = obj_config["MQTTHost"] | _mqtt_server;
        _mqtt_port = obj_config["MQTTPort"] | _mqtt_port;
        _mqtt_user = obj_config["MQTTUser"] | _mqtt_user;
        _mqtt_pwd = obj_config["MQTTPwd"] | _mqtt_pwd;
        _mqtt_health_topic = obj_config["MQTTHealthTopic"] | _mqtt_health_topic;
        _mqtt_function_topic = obj_config["MQTTFunctionTopic"] | _mqtt_function_topic;

        SLOG_PRINTF(SLOG_INFO, "... END \n");
    }
    else
    {
        SLOG_PRINTF(SLOG_WARNING, "... END no configuration\n");
    }
}

void Focuser::AlpacaWriteJson(JsonObject &root)
{
    SLOG_PRINTF(SLOG_NOTICE, "BEGIN ...\n");
    AlpacaFocuser::AlpacaWriteJson(root);

    // Config
    JsonObject obj_config = root["FocuserConfiguration"].to<JsonObject>();

    // #add # for read only
    JsonObject obj_states = root["#States"].to<JsonObject>();
    //current static states
    obj_states["Position"] = _position;
    obj_states["AbsolutePosition"] = _position;
    obj_states["MovementMode"] =  _absolute_mode;
;
    //Motor positions
    obj_states["UserStepSize"] = _motor_step_increment;
    obj_states["UserMaxLimit"] = _motor_step_max; 
    obj_states["UserMinLimit"] = _motor_step_min;
    obj_states["OutboundDirection"] = _focuser_direction; 
    obj_states["MinDrawDistance"] = _focuser_mm_min; 
    obj_states["MaxDrawDistance"] = _focuser_mm_max; 
   
    //Temp compensation
    obj_states["TempCompAvailable"] = _temp_comp_available;
    obj_states["TempCompEnabled"] = _temp_comp_enabled;
    //obj_states["TempComp"] = new JsonObject(); Handkle arrays of temperature compensation offsets later. 
    
    //Backlash Compensation
    obj_states["BacklashCompAvailable"] = _backlash_comp_available;
    obj_states["BacklashCompEnabled"] = _backlash_comp_enabled;
    obj_states["BacklashSize"] = _backlash_size;
    obj_states["BacklashDirection"] = _backlash_direction;   

    //MQTT interface
    obj_states["MQTTHost"] = _mqtt_server;
    obj_states["MQTTPort"] = _mqtt_port;
    obj_states["MQTTUser"] = _mqtt_user;
    obj_states["MQTTPwd"] = _mqtt_pwd;
    obj_states["MQTTHealthTopic"] = _mqtt_health_topic;
    obj_states["MQTTFunctionTopic"] = _mqtt_function_topic; 

    DBG_JSON_PRINTFJ(SLOG_NOTICE, root, "... END root=<%s>\n", _ser_json_);
}

const bool Focuser::_putMove(int32_t target_position)
{
    bool result = false;

    if ( _focuserState == FocuserStates::FOCUSER_HALTED || 
        _focuserState == FocuserStates::FOCUSER_STOPPING || 
        _focuserState == FocuserStates::FOCUSER_IDLE )
    {
        if ( target_position >= _motor_step_min && 
             target_position <= _motor_step_max ) 
        {
            _target_position = target_position;
            const bool moving_out = _target_position > _position;
            _myMotor.step(moving_out ? _focuser_direction : !_focuser_direction);
            StartTimer();
            _focuserState = FocuserStates::FOCUSER_MOVING;
            result = true;
        }
        else
        {
            // wrong state
        }
    }
    return result;
}

const bool Focuser::_putHalt()
{
    bool result = false;

    if ( _focuserState == FocuserStates::FOCUSER_MOVING || _focuserState == FocuserStates::FOCUSER_STOPPING)
    {
        StopTimer();
        _focuserState = FocuserStates::FOCUSER_HALTED;
        result = true;
    }
    else 
    {
        StopTimer();
    }
    return result;
}
