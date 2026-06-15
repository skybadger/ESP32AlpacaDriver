/**************************************************************************************************
 *  Generic motor control classes for Alpaca devices.
 *
 *  The Motor base class owns common enable/disable behavior and exposes a
 *  small virtual interface. Concrete motor types map that interface to the
 *  hardware normally used by that motor family.
 **************************************************************************************************/
#pragma once
#include <Arduino.h>
#include <device.h>

#define DIRN_CW LOW
#define DIRN_CCW HIGH

struct MotorPinSet
{
  int primary = -1;
  int direction = -1;
  int enable = -1;
  int auxiliary = -1;

  MotorPinSet() = default;
  MotorPinSet(int primary_pin, int direction_pin, int enable_pin = -1, int auxiliary_pin = -1)
      : primary(primary_pin), direction(direction_pin), enable(enable_pin), auxiliary(auxiliary_pin)
  {
  }
};

class Motor
{
public:
  enum EnableModes { ENABLE_NONE, ENABLE_LOW, ENABLE_HIGH };
  enum MotorType { MOTOR_UNKNOWN, MOTOR_STEPPER, MOTOR_BRUSHED_DC, MOTOR_BRUSHLESS };
  enum Directions { DIRECTION_CW = DIRN_CW, DIRECTION_CCW = DIRN_CCW };

protected:
  MotorPinSet _pins;
  EnableModes _enableMode = ENABLE_NONE;
  int _last_direction = DIRN_CW;
  bool _configured = false;
  MotorType _type = MOTOR_UNKNOWN;

  void configureEnablePin()
  {
    if (_enableMode != ENABLE_NONE && _pins.enable >= 0)
    {
      pinMode(_pins.enable, OUTPUT);
      disableMotor();
    }
  }

  void writeEnable(bool enabled)
  {
    if (!_configured || _enableMode == ENABLE_NONE || _pins.enable < 0)
      return;

    if (_enableMode == ENABLE_LOW)
      digitalWrite(_pins.enable, enabled ? LOW : HIGH);
    else
      digitalWrite(_pins.enable, enabled ? HIGH : LOW);
  }

public:
  Motor() = default;

  Motor(MotorType type, MotorPinSet pins, EnableModes enable_mode = ENABLE_NONE)
      : _pins(pins), _enableMode(enable_mode), _configured(true), _type(type)
  {
  }

  virtual ~Motor() = default;

  virtual void begin()
  {
    if (!_configured)
      return;

    configureEnablePin();
  }

  virtual void setPins(MotorPinSet pins, EnableModes enable_mode = ENABLE_NONE)
  {
    _pins = pins;
    _enableMode = enable_mode;
    _configured = true;
    begin();
  }

  bool isConfigured() const { return _configured; }
  MotorType type() const { return _type; }

  virtual void enableMotor() { writeEnable(true); }
  virtual void disableMotor() { writeEnable(false); }
  virtual void stop() { disableMotor(); }

  virtual void step(int direction) { _last_direction = direction; }
  virtual void step() { step(_last_direction); }
  virtual void run(int direction, uint8_t speed) { (void)direction; (void)speed; }
  virtual void update() {}

  int lastDirection() const { return _last_direction; }
};

class StepperMotor : public Motor
{
public:
  StepperMotor() : Motor()
  {
    _type = MOTOR_STEPPER;
  }

  StepperMotor(int step_pin, int direction_pin, int enable_pin = -1, EnableModes enable_mode = ENABLE_NONE)
      : Motor(MOTOR_STEPPER, MotorPinSet(step_pin, direction_pin, enable_pin), enable_mode)
  {
    begin();
  }

  StepperMotor(pinmap_t *pins, size_t num_pins, EnableModes enable_mode = ENABLE_NONE)
      : Motor(MOTOR_STEPPER, pinsFromPinMap(pins, num_pins), enable_mode)
  {
    begin();
  }

  static MotorPinSet pinsFromPinMap(pinmap_t *pins, size_t num_pins)
  {
    MotorPinSet result;
    if (pins != nullptr && num_pins > 0)
      result.primary = pins[0].pin;   // step pulse
    if (pins != nullptr && num_pins > 1)
      result.direction = pins[1].pin; // direction
    if (pins != nullptr && num_pins > 2)
      result.enable = pins[2].pin;    // enable
    return result;
  }

  void begin() override
  {
    if (!_configured || _pins.primary < 0 || _pins.direction < 0)
      return;

    pinMode(_pins.direction, OUTPUT);
    pinMode(_pins.primary, OUTPUT);
    digitalWrite(_pins.direction, DIRN_CW);
    digitalWrite(_pins.primary, LOW);
    configureEnablePin();
  }

  void setPins(int step_pin, int direction_pin, int enable_pin = -1, EnableModes enable_mode = ENABLE_NONE)
  {
    Motor::setPins(MotorPinSet(step_pin, direction_pin, enable_pin), enable_mode);
  }

  void step(int direction) override
  {
    if (!_configured)
      return;

    enableMotor();
    if (direction != _last_direction)
    {
      digitalWrite(_pins.direction, direction);
      _last_direction = direction;
    }

    digitalWrite(_pins.primary, HIGH);
    delayMicroseconds(2);
    digitalWrite(_pins.primary, LOW);
    delayMicroseconds(2);
  }
};

class BrushedDcMotor : public Motor
{
  uint32_t _run_started_ms = 0;
  uint32_t _run_time_ms = 0;
  bool _running = false;

public:
  BrushedDcMotor() : Motor()
  {
    _type = MOTOR_BRUSHED_DC;
  }

  BrushedDcMotor(int drive_pin, int direction_pin, int enable_pin = -1, EnableModes enable_mode = ENABLE_NONE)
      : Motor(MOTOR_BRUSHED_DC, MotorPinSet(drive_pin, direction_pin, enable_pin), enable_mode)
  {
    begin();
  }

  BrushedDcMotor(pinmap_t *pins, size_t num_pins, EnableModes enable_mode = ENABLE_NONE)
      : Motor(MOTOR_BRUSHED_DC, pinsFromPinMap(pins, num_pins), enable_mode)
  {
    begin();
  }

  static MotorPinSet pinsFromPinMap(pinmap_t *pins, size_t num_pins)
  {
    MotorPinSet result;
    if (pins != nullptr && num_pins > 0)
      result.primary = pins[0].pin;   // drive/PWM
    if (pins != nullptr && num_pins > 1)
      result.direction = pins[1].pin; // direction
    if (pins != nullptr && num_pins > 2)
      result.enable = pins[2].pin;    // enable
    return result;
  }

  void begin() override
  {
    if (!_configured || _pins.primary < 0 || _pins.direction < 0)
      return;

    pinMode(_pins.primary, OUTPUT);
    pinMode(_pins.direction, OUTPUT);
    digitalWrite(_pins.primary, LOW);
    digitalWrite(_pins.direction, DIRN_CW);
    configureEnablePin();
  }

  void run(int direction, uint8_t speed) override
  {
    if (!_configured)
      return;

    enableMotor();
    digitalWrite(_pins.direction, direction);
    _last_direction = direction;
    analogWrite(_pins.primary, speed);
    if (!_running)
    {
      _run_started_ms = millis();
      _running = true;
    }
  }

  void stop() override
  {
    if (_running)
    {
      _run_time_ms += millis() - _run_started_ms;
      _running = false;
    }
    if (_configured)
      analogWrite(_pins.primary, 0);
    disableMotor();
  }

  uint32_t runTimeMs() const
  {
    return _running ? _run_time_ms + (millis() - _run_started_ms) : _run_time_ms;
  }
};

class BrushlessMotor : public Motor
{
  uint8_t _last_speed = 0;

public:
  BrushlessMotor() : Motor()
  {
    _type = MOTOR_BRUSHLESS;
  }

  BrushlessMotor(int throttle_pin, int direction_pin = -1, int enable_pin = -1, EnableModes enable_mode = ENABLE_NONE)
      : Motor(MOTOR_BRUSHLESS, MotorPinSet(throttle_pin, direction_pin, enable_pin), enable_mode)
  {
    begin();
  }

  BrushlessMotor(pinmap_t *pins, size_t num_pins, EnableModes enable_mode = ENABLE_NONE)
      : Motor(MOTOR_BRUSHLESS, pinsFromPinMap(pins, num_pins), enable_mode)
  {
    begin();
  }

  static MotorPinSet pinsFromPinMap(pinmap_t *pins, size_t num_pins)
  {
    MotorPinSet result;
    if (pins != nullptr && num_pins > 0)
      result.primary = pins[0].pin;   // throttle/PWM
    if (pins != nullptr && num_pins > 1)
      result.direction = pins[1].pin; // optional direction
    if (pins != nullptr && num_pins > 2)
      result.enable = pins[2].pin;    // enable/arming
    return result;
  }

  void begin() override
  {
    if (!_configured || _pins.primary < 0)
      return;

    pinMode(_pins.primary, OUTPUT);
    if (_pins.direction >= 0)
      pinMode(_pins.direction, OUTPUT);
    analogWrite(_pins.primary, 0);
    configureEnablePin();
  }

  void run(int direction, uint8_t speed) override
  {
    if (!_configured)
      return;

    enableMotor();
    if (_pins.direction >= 0)
      digitalWrite(_pins.direction, direction);
    _last_direction = direction;
    _last_speed = speed;
    analogWrite(_pins.primary, speed);
  }

  void stop() override
  {
    _last_speed = 0;
    if (_configured)
      analogWrite(_pins.primary, 0);
    disableMotor();
  }

  uint8_t speed() const { return _last_speed; }
};
