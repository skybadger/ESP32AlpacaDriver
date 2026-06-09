/**************************************************************************************************
  Filename:       Focuser.cpp
  Revised:        $Date: 2024-07-24$
  Revision:       $Revision: 01 $
  Description:    ASCOM Alpaca Focuser Device Template

  Copyright 2024-2025 peter_n@gmx.de. All rights reserved.
**************************************************************************************************/
#include <Focuser.h>

constexpr const char* Focuser::FocuserModeCh[];
constexpr const char* Focuser::FocuserStateCh[];

static constexpr uint8_t kMaxHardwareTimers = 4;
static Focuser* g_focuser_timer_owner[kMaxHardwareTimers] = { nullptr };

static void IRAM_ATTR dispatchFocuserTimerInterrupt(uint8_t timer_num)
{
  if (timer_num < kMaxHardwareTimers && g_focuser_timer_owner[timer_num] != nullptr)
  {
    g_focuser_timer_owner[timer_num]->_timerInterruptHandler();
  }
}

void IRAM_ATTR Focuser::_timerInterruptStatic0()
{
    dispatchFocuserTimerInterrupt(0);
}

void IRAM_ATTR Focuser::_timerInterruptStatic1()
{
    dispatchFocuserTimerInterrupt(1);
}

void IRAM_ATTR Focuser::_timerInterruptStatic2()
{
    dispatchFocuserTimerInterrupt(2);
}

void IRAM_ATTR Focuser::_timerInterruptStatic3()
{
    dispatchFocuserTimerInterrupt(3);
}

Focuser::Focuser(uint8_t timer_num) : AlpacaFocuser(), _timer_num(timer_num)
{
}

void Focuser::Begin( )
{
    // init Focuser

    AlpacaFocuser::Begin();
       
    // Initialize timer interrupt for time-based stepper control
    InitTimer();

    _focuserState = FocuserStates::FOCUSER_HALTED;
    _targetFocuserState = FocuserStates::FOCUSER_HALTED;
    _is_moving = false;
}

// Regular device housekeeping; motor stepping is driven by this focuser's hardware timer.
void Focuser::Loop()
{   
    if( _focuserState == FocuserStates::FOCUSER_MOVING )
      SLOG_PRINTF(SLOG_DEBUG, "focuser position : %d\n", _position); 
    
    manageFocuserState( _targetFocuserState );
}

/**
 * InitTimer - Initialize ESP32 hardware timer for focuser time-based operations
 * Timer runs at 100ms intervals (10 Hz) to drive stepper motor control
 */
void Focuser::InitTimer()
{
    if (_timer_num >= kMaxHardwareTimers)
    {
        SLOG_PRINTF(SLOG_ERROR, "Invalid focuser timer number: %u\n", _timer_num);
        return;
    }

    g_focuser_timer_owner[_timer_num] = this;
    
    // Create a 1 MHz timer from the 80 MHz APB clock.
    _timer = timerBegin(_timer_num, _TIMER_DIVIDER, true);
    
    if ( _timer != nullptr)
    {
        void (*interrupt_handler)() = nullptr;
        switch (_timer_num)
        {
            case 0:
                interrupt_handler = &_timerInterruptStatic0;
                break;
            case 1:
                interrupt_handler = &_timerInterruptStatic1;
                break;
            case 2:
                interrupt_handler = &_timerInterruptStatic2;
                break;
            case 3:
                interrupt_handler = &_timerInterruptStatic3;
                break;
            default:
                break;
        }

        // Attach the interrupt handler
        timerAttachInterrupt( _timer, interrupt_handler, true);
        
        // Set the timer to interrupt at _TIMER_INTERVAL_US microseconds
        timerAlarmWrite(_timer, _TIMER_INTERVAL_US, true);
        timerAlarmEnable(_timer);
        
        timerStop(_timer);
        
        SLOG_PRINTF(SLOG_INFO, "Focuser timer %u initialized: %u us interval\n", _timer_num, _TIMER_INTERVAL_US);
    }
    else
    {
        g_focuser_timer_owner[_timer_num] = nullptr;
        SLOG_PRINTF(SLOG_ERROR, "Failed to initialize focuser timer %u\n", _timer_num);
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
      _targetFocuserState = FocuserStates::FOCUSER_HALTED;
      _is_moving = false;
      _myMotor.disableMotor();
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
  {
    if ( _command_queue.empty())
    {
        return false;
    }

    command_t command;
    if (!_command_queue.pop(command))
    {
        return false;
    }

    _current_command = command;
    _targetFocuserState = command.command_argument;

    //Check for other queued commands
    return 0;
  } 
   
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
                 if (_focuserState == FocuserStates::FOCUSER_HALTED || _focuserState == FocuserStates::FOCUSER_IDLE)
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
    obj_states["#TempCompAvailable"] = _temp_comp_available;
    if ( ! _temp_comp_available )
        obj_states["#TempCompEnabled"] = false ; 
    else 
        obj_states["TempCompEnabled"] = _temp_comp_enabled;
    //obj_states["TempComp"] = new JsonObject(); Handkle arrays of temperature compensation offsets later. 
    
    //Backlash Compensation
    obj_states["#BacklashCompAvailable"] = _backlash_comp_available;
    obj_states["BacklashCompEnabled"] = _backlash_comp_enabled;
    obj_states["BacklashSize"] = _backlash_size;
    obj_states["BacklashDirection"] = _backlash_direction;   

    DBG_JSON_PRINTFJ(SLOG_NOTICE, root, "... END root=<%s>\n", _ser_json_);
}

const bool Focuser::_putMove(int32_t target_position)
{
    return _putMove(target_position, 0, 0);
}

const bool Focuser::_putMove(int32_t target_position, uint32_t client_id, uint32_t client_transaction_id)
{
    bool result = false;

    if ( target_position >= _motor_step_min && 
         target_position <= _motor_step_max ) 
    {
        command_t command;
        command.clientId = client_id;
        command.clientTransactionId = client_transaction_id;
        command.command_type = CommandType::COMMAND_MOVE_ABSOLUTE;
        snprintf(command.command_argument, sizeof(command.command_argument), "%ld", (long)target_position);
        result = enqueueCommand(command);
    }
    return result;
}

const bool Focuser::_putHalt()
{
    return _putHalt(0, 0);
}

const bool Focuser::_putHalt(uint32_t client_id, uint32_t client_transaction_id)
{
    bool result = false;

    _command_queue.clear();
    _current_command.clientId = client_id;
    _current_command.clientTransactionId = client_transaction_id;
    _current_command.command_type = CommandType::COMMAND_HALT;
    strlcpy(_current_command.command_argument, "halt", sizeof(_current_command.command_argument));

    if ( _focuserState == FocuserStates::FOCUSER_MOVING || _focuserState == FocuserStates::FOCUSER_STOPPING)
    {
        StopTimer();
        _focuserState = FocuserStates::FOCUSER_HALTED;
        _targetFocuserState = FocuserStates::FOCUSER_HALTED;
        _is_moving = false;
        _myMotor.disableMotor();
        result = true;
    }
    else 
    {
        StopTimer();
        _focuserState = FocuserStates::FOCUSER_HALTED;
        _targetFocuserState = FocuserStates::FOCUSER_HALTED;
        _is_moving = false;
        _myMotor.disableMotor();
        result = true;
    }
    return result;
}

bool Focuser::enqueueCommand(const command_t &command)
{
    bool queued = _command_queue.push(command);
    if (!queued)
    {
        SLOG_PRINTF(SLOG_WARNING, "Focuser command queue full, command type=%d rejected\n", command.command_type);
    }
    return queued;
}

bool Focuser::insertCommand(size_t index, const command_t &command)
{
    //split a single command into parts here so it only happens once. 
    switch (command.command_type)
    {
        case CommandType::COMMAND_MOVE_ABSOLUTE:
            if ( _backlash_comp_available && _backlash_comp_enabled )
            {
                int32_t distance = (int32_t) strtol( command.command_argument, nullptr, 10 );
                bool direction = ( distance >= 0 )? DIRN_CCW:  DIRN_CW ; 
                if ( direction == _backlash_direction )
                {
                       //We're already taking up the backlash by moving in the same direction as the backlash setting
                       updated_cmd = command_t( )
                       bool queued = _command_queue.insert(index = 0 , command);                      
                }
                else
                {
                    //go past the target by the backlash distance and come back in teh backlash direction. 
                    /* code */
                }
                

            }
        
        
        return beginMoveTo((int32_t)strtol(command.command_argument, nullptr, 10));

        case CommandType::COMMAND_MOVE_RELATIVE:
            return beginMoveTo((int32_t)_position + (int32_t)strtol(command.command_argument, nullptr, 10));

        case CommandType::COMMAND_HALT:
            return _putHalt(command.clientId, command.clientTransactionId);
        
        default: 

    }
    
    bool queued = _command_queue.insert(index, command);
    if (!queued)
    {
        SLOG_PRINTF(SLOG_WARNING, "Focuser command queue insert failed index=%u type=%d\n", (unsigned int)index, command.command_type);
    }
    return queued;
}

bool Focuser::processNextCommand()
{
    if ( _command_queue.empty())
    {
        return false;
    }

    command_t command;
    if (!_command_queue.pop(command))
    {
        return false;
    }

    _current_command = command;
    return executeCommand(command);
}

bool Focuser::executeCommand(const command_t &command)
{
    switch (command.command_type)
    {
        case CommandType::COMMAND_MOVE_ABSOLUTE:
            return beginMoveTo((int32_t)strtol(command.command_argument, nullptr, 10));

        case CommandType::COMMAND_MOVE_RELATIVE:
            return beginMoveTo((int32_t)_position + (int32_t)strtol(command.command_argument, nullptr, 10));

        case CommandType::COMMAND_HALT:
            _command_queue.clear();
            return _putHalt(command.clientId, command.clientTransactionId);

        default:
            SLOG_PRINTF(SLOG_WARNING, "Unknown focuser command type=%d\n", command.command_type);
            return false;
    }
}

bool Focuser::beginMoveTo(int32_t target_position)
{
    if (target_position < _motor_step_min || target_position > _motor_step_max)
    {
        SLOG_PRINTF(SLOG_WARNING, "Focuser move target out of range: %ld\n", (long)target_position);
        return false;
    }

    _target_position = (uint32_t)target_position;
    if (_target_position == _position)
    {
        _focuserState = FocuserStates::FOCUSER_HALTED;
        _targetFocuserState = FocuserStates::FOCUSER_HALTED;
        _is_moving = false;
        return true;
    }

    _is_moving = true;
    _focuserState = FocuserStates::FOCUSER_MOVING;
    _targetFocuserState = FocuserStates::FOCUSER_MOVING;
    StartTimer();
    return true;
}
