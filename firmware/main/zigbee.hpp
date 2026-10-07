#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "ha/esp_zigbee_ha_standard.h"
#include "zcl_utility.h"
#include <cstdint>

/* Zigbee configuration */
#define EP_SERVOMOTOR 1

esp_err_t initZigbee();
esp_err_t initDevice();
void sendServoPosition(uint8_t endpoint, uint8_t position);
