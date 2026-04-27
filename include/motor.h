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
//#include <ESP32.h>
#define DIRN_CW LOW
#define DIRN_CCW HIGH
  
//Basic Focuser info - update based on your Focuser 
 class Motor {
  public:
  enum EnableModes { ENABLE_NONE, ENABLE_LOW, ENABLE_HIGH };

  protected: 
  //Hardware Control motor pins
  int _stepPin = 0;
  int _dirPin = 0;
  int _enPin = 0;
  int _enableMode = EnableModes::ENABLE_NONE;
      
  void _init(void) 
  {
      //Setup hardware
      pinMode(_dirPin, OUTPUT);
      pinMode(_stepPin, OUTPUT);
      pinMode(_enPin, OUTPUT);
      digitalWrite( _dirPin, DIRN_CW );
      digitalWrite( _stepPin, LOW);
      digitalWrite( _enPin , HIGH); //Active low.
  }
  
  public: 
  
  Motor(int step_pin, int dir_pin, int enable_pin, int enable_mode) : _stepPin(step_pin), _dirPin(dir_pin), _enPin(enable_pin), _enableMode(enable_mode), _init() {};
  
  //Re-allocate the assigned pins. 
  void setPins(int step_pin, int dir_pin, int enable_pin, int enable_mode)
  {
    _stepPin = step_pin;
    _dirPin = dir_pin;
    _enPin = enable_pin;
    _enableMode = enable_mode;
    _init();
  }

  void stepMotor()
  {
    digitalWrite( _stepPin, HIGH);
    delayMicroseconds(2);
    digitalWrite( _stepPin, LOW);
    delayMicroseconds(2);
  }

  void disableMotor()
  {
    if (_enableMode == EnableModes::ENABLE_LOW)
    {
      digitalWrite(_enPin, HIGH); // Disable motor (active low)
    }
    else if (_enableMode == EnableModes::ENABLE_HIGH)
    {
      digitalWrite(_enPin, LOW); // Disable motor (active high)
    }
  }
};