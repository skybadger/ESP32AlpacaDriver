/**************************************************************************************************
 *  Motor control class for focuser device
 * 
 *  This class provides basic motor control functionality, including pin initialization and stepping.
 *  It is designed to be used with a stepper motor driver and can be integrated into a focuser device
 *  implementation.
 * 
 *  Note: This is a simplified example and may need to be expanded with additional features such as
 *  speed control, acceleration, and error handling based on the specific requirements of your focuser
 *  device.
 * 
 *  Copyright 2024-2025
 * */
#pragma once
#include <Arduino.h>

#define DIRN_CW LOW
#define DIRN_CCW HIGH
  
//Basic Focuser info - update based on your Focuser 
 class Motor {
  public:
  enum EnableModes { ENABLE_NONE, ENABLE_LOW, ENABLE_HIGH };
  enum Directions { DIRECTION_CW = DIRN_CW, DIRECTION_CCW = DIRN_CCW };

  protected: 
  //Hardware Control motor pins
  int _stepPin = 0;
  int _dirPin = 0;
  int _enPin = 0;
  int _enableMode = EnableModes::ENABLE_NONE;
  int _last_direction = DIRN_CW;
  bool _configured = false;
      
  void init(void) 
  {
      if (!_configured)
      {
        return;
      }

      //Setup hardware
      pinMode(_dirPin, OUTPUT);
      pinMode(_stepPin, OUTPUT);
      if (_enableMode != EnableModes::ENABLE_NONE)
      {
        pinMode(_enPin, OUTPUT);
      }

      digitalWrite( _dirPin, DIRN_CW );
      digitalWrite( _stepPin, LOW);
      disableMotor();
  }
  
  public: 
  
  Motor() = default;
  virtual ~Motor() = default;
  Motor(int step_pin, int dir_pin, int enable_pin, int enable_mode) : _stepPin(step_pin), _dirPin(dir_pin), _enPin(enable_pin), _enableMode(enable_mode), _configured(true){ init();};
  
  //Re-allocate the assigned pins. 
  void setPins(int step_pin, int dir_pin, int enable_pin, int enable_mode)
  {
    _stepPin = step_pin;
    _dirPin = dir_pin;
    _enPin = enable_pin;
    _enableMode = enable_mode;
    _configured = true;
    init();
  }

  virtual bool isConfigured() const
  {
    return _configured;
  }

  virtual void enableMotor()
  {
    if (!_configured || _enableMode == EnableModes::ENABLE_NONE)
    {
      return;
    }

    digitalWrite(_enPin, _enableMode == EnableModes::ENABLE_LOW ? LOW : HIGH);
  }

  virtual void stepMotor( int direction)
  {
    if (!_configured)
    {
      return;
    }

    if ( direction != _last_direction )
    {
      digitalWrite( _dirPin, direction );
      _last_direction = direction;
    } 
    digitalWrite( _stepPin, HIGH);
    delayMicroseconds(2);
    digitalWrite( _stepPin, LOW);
    delayMicroseconds(2);
  }

  virtual void step(int direction)
  {
    stepMotor(direction);
  }

  virtual void step()
  {
    stepMotor(_last_direction);
  }

  virtual void disableMotor()
  {
    if (!_configured || _enableMode == EnableModes::ENABLE_NONE)
    {
      return;
    }

    if (_enableMode == EnableModes::ENABLE_LOW)
    {
      digitalWrite(_enPin, HIGH); // Disable motor (active low)
    }
    else if (_enableMode == EnableModes::ENABLE_HIGH)
    {
      digitalWrite(_enPin, LOW); // Disable motor (active high)
    }
  }

  virtual bool setSpeedDirection(uint8_t speed, uint8_t direction)
  {
    if (speed == 0)
    {
      disableMotor();
      return true;
    }

    enableMotor();
    stepMotor(direction);
    return true;
  }

  virtual uint8_t getSpeed() const { return 0; }
  virtual uint8_t getDirection() const { return _last_direction; }
};
