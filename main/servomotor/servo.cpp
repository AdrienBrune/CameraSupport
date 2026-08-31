#include "servomotor/servo.hpp"
#include "debug.hpp"
#include "memory/pers_mem.hpp"
#include "driver/ledc.h"

#define PWM_CHANNEL         LEDC_CHANNEL_0
#define PWM_TIMER           LEDC_TIMER_0
#define PWM_MODE            LEDC_LOW_SPEED_MODE // LEDC_HIGH_SPEED_MODE not available for this board
#define PWM_FREQ_HZ         50
#define PWM_RESOLUTION_BIT  LEDC_TIMER_16_BIT

#define MIN_PULSE_WIDTH_US  600     // 500 in DS
#define MAX_PULSE_WIDTH_US  2400    // 2500 in DS
#define PWM_WIDTH_US        20000   // 20 ms / 50Hz standard

#define MAX_ANGLE 180
#define MIN_ANGLE 0

Servo & Servo::GetServoMotor()
{
    static Servo instance;
    return instance;
}

void Servo::Init(uint32_t pin)
{
    mPin = pin;

    // Configure LEDC timer
    ledc_timer_config_t ledc_timer{};
    ledc_timer.duty_resolution = PWM_RESOLUTION_BIT;
    ledc_timer.freq_hz         = PWM_FREQ_HZ;
    ledc_timer.speed_mode      = PWM_MODE;
    ledc_timer.timer_num       = PWM_TIMER;
    ledc_timer.clk_cfg         = LEDC_AUTO_CLK;

    esp_err_t err = ledc_timer_config(&ledc_timer);
    ESP_ERROR_CHECK(err);

    // Configure LEDC channel
    ledc_channel_config_t ledc_channel{};
    ledc_channel.channel    = PWM_CHANNEL;
    ledc_channel.duty       = 0;
    ledc_channel.gpio_num   = static_cast<int>(mPin);
    ledc_channel.speed_mode = PWM_MODE;
    ledc_channel.hpoint     = 0;
    ledc_channel.timer_sel  = PWM_TIMER;

    err = ledc_channel_config(&ledc_channel);
    ESP_ERROR_CHECK(err);

    DebugLogger::getInstance().print(DEBUG_SERVO, DEBUG_INFO, "Servo driver initialized");

    // apply last angle
    uint8_t angle = Memory::GetMemory().Get<uint32_t>(DATA_SERVOANGLE);
    SetTargetAngle(angle);
    SetCurrentAngle(angle);
}

void Servo::SetCurrentAngle(uint8_t angle)
{
    if(mPin == UNINITIALIZED)
    {
        DebugLogger::getInstance().print(DEBUG_SERVO, DEBUG_ERROR, "Servo driver uninitialized !");
        return;
    }

    if(angle > MAX_ANGLE)
    {
        DebugLogger::getInstance().print(DEBUG_SERVO, DEBUG_ERROR, "Angle requested superior than 100 degres, forced to 100 degres");
        angle = MAX_ANGLE;
    }

    // angle to pulse width range [us]
    uint16_t pulse = MIN_PULSE_WIDTH_US + (uint32_t)angle * (MAX_PULSE_WIDTH_US - MIN_PULSE_WIDTH_US) / MAX_ANGLE;
    // pulse width to 16bit data word
    uint16_t word = (pulse * 65535) / PWM_WIDTH_US;

    // check word is in range
    if(word < 1638 || word > 8124)
    {
        DebugLogger::getInstance().print(DEBUG_SERVO, DEBUG_ERROR, "Angle calculation failure : %d", word);
        return;
    }

    ESP_ERROR_CHECK(ledc_set_duty(PWM_MODE, PWM_CHANNEL, word));
    ESP_ERROR_CHECK(ledc_update_duty(PWM_MODE, PWM_CHANNEL));
    DebugLogger::getInstance().print(DEBUG_SERVO, DEBUG_INFO, "%d degres applied (w:%d, p:%d[us])", angle, word, pulse);

    mCurrentAngle.store(angle);
}
