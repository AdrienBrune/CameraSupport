#pragma once

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>
#include <atomic>
#include "memory/pers_mem.hpp"

#define UNINITIALIZED 0xFF

class Servo
{
public:
    typedef struct
    {
        uint8_t angle;
    } servo_cmd_t;

private:
    Servo():
        mTargetAngle(0),
        mCurrentAngle(0),
        mPin(UNINITIALIZED)
    {}
    ~Servo(){}

public:
    void Init(uint32_t pin);

    inline uint8_t GetTargetAngle(){ return mTargetAngle.load(); }
    inline void SetTargetAngle(uint8_t angle)
    {
        if(angle != mTargetAngle.load())
        {
            mTargetAngle.store(angle);

            // update memory
            Memory::GetMemory().Set<uint32_t>(DATA_SERVOANGLE, mTargetAngle.load());
        }
    }

    inline uint8_t GetCurrentAngle(){ return mCurrentAngle.load(); }
    void SetCurrentAngle(uint8_t angle);

private:
    std::atomic<uint8_t> mTargetAngle;
    std::atomic<uint8_t> mCurrentAngle;
    uint8_t mPin;

public:
    static Servo & GetServoMotor();
};