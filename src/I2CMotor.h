/**************************************************************************************************
  Description: I2C DC motor adapter for the shared motor interface.
**************************************************************************************************/
#pragma once

#include "motor.h"

#include <Wire.h>

class I2CMotor : public Motor
{
public:
  static constexpr uint8_t kDefaultAddress = 0xB0 >> 1;
  static constexpr uint8_t kSpeedOff = 0;
  static constexpr uint8_t kSpeedSlowSlew = 180;
  static constexpr uint8_t kSpeedFastSlew = 220;

  explicit I2CMotor(uint8_t address = kDefaultAddress, TwoWire *wire = &Wire)
      : _wire(wire), _address(address)
  {
  }

  void configure(uint8_t address, TwoWire *wire = &Wire)
  {
    _address = address;
    _wire = wire;
    _configured = false;
  }

  bool begin()
  {
    if (_wire == nullptr)
    {
      _configured = false;
      return false;
    }

    _configured = check();
    if (_configured)
    {
      initController();
    }
    return _configured;
  }

  bool check()
  {
    if (_wire == nullptr)
    {
      return false;
    }

    _wire->beginTransmission(_address);
    _wire->write(static_cast<uint8_t>(7));
    return _wire->endTransmission() == 0;
  }

  bool initController()
  {
    if (_wire == nullptr)
    {
      return false;
    }

    uint8_t payload[] = {0, 0, 128, 128, 255};
    _wire->beginTransmission(_address);
    _wire->write(payload, sizeof(payload));
    _configured = _wire->endTransmission() == 0;
    _speed = kSpeedOff;
    _last_direction = DIRECTION_CW;
    return _configured;
  }

  bool setSpeedDirection(uint8_t speed, uint8_t direction) override
  {
    if (_wire == nullptr)
    {
      return false;
    }

    uint8_t motor_speed = 128;
    if (speed > 0)
    {
      motor_speed = speed / 2;
      motor_speed = (direction == DIRECTION_CW) ? 128 - motor_speed : 128 + motor_speed;
    }

    uint8_t payload[] = {1, motor_speed, motor_speed};
    _wire->beginTransmission(_address);
    _wire->write(payload, sizeof(payload));
    const bool ok = _wire->endTransmission() == 0;

    if (ok)
    {
      _configured = true;
      _speed = speed;
      _last_direction = direction;
    }
    else
    {
      _configured = false;
    }
    return ok;
  }

  void disableMotor() override
  {
    setSpeedDirection(kSpeedOff, _last_direction);
  }

  bool getSpeedDirection(uint8_t &speed, uint8_t &direction)
  {
    if (_wire == nullptr)
    {
      return false;
    }

    _wire->beginTransmission(_address);
    _wire->write(static_cast<uint8_t>(1));
    if (_wire->endTransmission() != 0)
    {
      _configured = false;
      return false;
    }

    if (_wire->requestFrom(static_cast<int>(_address), 3) != 3)
    {
      _configured = false;
      return false;
    }

    const uint8_t raw_direction = _wire->read();
    speed = _wire->read();
    _wire->read();

    direction = raw_direction < 128 ? DIRECTION_CW : DIRECTION_CCW;
    _speed = speed;
    _last_direction = direction;
    _configured = true;
    return true;
  }

  uint8_t getSpeed() const override { return _speed; }
  uint8_t getDirection() const override { return static_cast<uint8_t>(_last_direction); }
  uint8_t address() const { return _address; }

private:
  TwoWire *_wire = &Wire;
  uint8_t _address = kDefaultAddress;
  uint8_t _speed = kSpeedOff;
};
