#include "string.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_zigbee_core.h"
#include "zigbee.hpp"
#include "nvs_flash.h"
#include "esp_sleep.h"
#include "debug.hpp"
#include "servomotor/servo.hpp"

#define PIN_PWM_SERVO 14 // GPIO14
#define KEEPALIVE_TIMEOUT_MS (uint64_t)(5*60*1000)

#define USEC_TO_MS  1000
#define MS_TO_SEC   1000
#define SEC_TO_HOUR 3600
#define RESTART_DELAY_HOURS 12

void _task_servo(void *pvParameters)
{
    const uint32_t STEP = 5;
    const int PERIOD_MS = 50;
    int error = 0;
    uint8_t angle = 0, target = 0;
    TickType_t startTick = xTaskGetTickCount();

    DebugLogger::getInstance().print(DEBUG_TASK, DEBUG_ERROR, "SERVO task started");
    while(1)
    {
        angle = Servo::GetServoMotor().GetCurrentAngle();
        target = Servo::GetServoMotor().GetTargetAngle();
        error = (int)target - (int)angle;

        if(error != 0)
        {
            int sign = (error > 0) - (error < 0);
            if(abs(error) > STEP)
            {
                angle += STEP * sign;
            }
            else
            {
                angle = target;
            }
            Servo::GetServoMotor().SetCurrentAngle(angle);
        }

        if ((xTaskGetTickCount() - startTick) >= pdMS_TO_TICKS(KEEPALIVE_TIMEOUT_MS))
        {
            startTick = xTaskGetTickCount();
            sendServoPosition(EP_SERVOMOTOR, target); // keep alive signal
        }

        vTaskDelay(pdMS_TO_TICKS(PERIOD_MS));
    }
}

void _task_zigbee(void *pvParameters)
{
    if(initZigbee() != ESP_OK)
    {
        DebugLogger::getInstance().print(DEBUG_TASK, DEBUG_ERROR, "ZIGBEE init failure");
        return;
    }

    if(initDevice() != ESP_OK)
    {
        DebugLogger::getInstance().print(DEBUG_TASK, DEBUG_ERROR, "ZIGBEE device registration failure");
        return;
    }

    if(esp_zb_start(false) != ESP_OK)
    {
        DebugLogger::getInstance().print(DEBUG_TASK, DEBUG_ERROR, "ZIGBEE hasn't started");
        return;
    }

    DebugLogger::getInstance().print(DEBUG_TASK, DEBUG_INFO, "ZIGBEE main loop started");
    esp_zb_stack_main_loop();
}

extern "C" void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        DebugLogger::getInstance().print(DEBUG_GENERIC, DEBUG_ERROR, "NVS flash init");
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    if (err != ESP_OK)
    {
        DebugLogger::getInstance().print(DEBUG_GENERIC, DEBUG_ERROR, "NVS init failure, program stopped");
        return;
    }

    esp_zb_platform_config_t config = {
        .radio_config = { .radio_mode = ZB_RADIO_MODE_NATIVE },
        .host_config = { .host_connection_mode = ZB_HOST_CONNECTION_MODE_NONE },
    };
    if(esp_zb_platform_config(&config) != ESP_OK)
    {
        DebugLogger::getInstance().print(DEBUG_GENERIC, DEBUG_ERROR, "ZIGBEE init failure, program stopped");
        return;
    }

    Memory::GetMemory().Load();

    Servo::GetServoMotor().Init(PIN_PWM_SERVO);

    DebugLogger::getInstance().print(DEBUG_GENERIC, DEBUG_INFO, "init completed");

    xTaskCreate(_task_zigbee, "zigbee", 8124, NULL, 1, NULL);
    xTaskCreate(_task_servo, "servo", 4096, NULL, 1, NULL);

    // // Enable restart each day to be sure zigbee is synchronized
    // esp_sleep_enable_timer_wakeup(5 * MS_TO_SEC * USEC_TO_MS);
    // DebugLogger::getInstance().print(DEBUG_GENERIC, DEBUG_INFO, "Automatic restart setup each day");
    // while(1)
    // {
    //     static TickType_t startTick = xTaskGetTickCount();
    //     if ((xTaskGetTickCount() - startTick) >= pdMS_TO_TICKS(RESTART_DELAY_HOURS * SEC_TO_HOUR * MS_TO_SEC))
    //     {
    //         DebugLogger::getInstance().print(DEBUG_GENERIC, DEBUG_INFO, "ESP32 will enter deep sleep ...");
    //         vTaskDelay(pdMS_TO_TICKS(1000));
    //         esp_deep_sleep_start();
    //     }

    //     vTaskDelay(pdMS_TO_TICKS(60 * MS_TO_SEC));
    // }
}
