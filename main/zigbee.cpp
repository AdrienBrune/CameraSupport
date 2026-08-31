#include "zigbee.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_zigbee_core.h"
#include "esp_check.h"
#include "esp_log.h"
#include "debug.hpp"
#include "servomotor/servo.hpp"
#include <atomic>

std::atomic<bool> connected = false;

static char modelName[] = { 0x0C, 'C', 'A', 'M', '-', 'P', 'L', 'A', 'T', 'F', 'O', 'R', 'M' };
static char manufacturerName[] = { 0x06, 'C', 'U', 'S', 'T', 'O', 'M'};

esp_zb_cluster_list_t *createLevelClusterList()
{
    static uint8_t appVersion = 1;
    static uint8_t stackVersion = 1;
    static uint8_t hwVersion = 1;

    esp_zb_cluster_list_t *list = esp_zb_zcl_cluster_list_create();
    
    // BASIC
    esp_zb_basic_cluster_cfg_t basic_cfg = { .zcl_version = 3, .power_source = 0x03 };
    esp_zb_attribute_list_t *basic_attr = esp_zb_basic_cluster_create(&basic_cfg);
    esp_zb_basic_cluster_add_attr(basic_attr, ESP_ZB_ZCL_ATTR_BASIC_APPLICATION_VERSION_ID, &appVersion);
    esp_zb_basic_cluster_add_attr(basic_attr, ESP_ZB_ZCL_ATTR_BASIC_STACK_VERSION_ID, &stackVersion);
    esp_zb_basic_cluster_add_attr(basic_attr, ESP_ZB_ZCL_ATTR_BASIC_HW_VERSION_ID, &hwVersion);
    esp_zb_basic_cluster_add_attr(basic_attr, ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID, modelName);
    esp_zb_basic_cluster_add_attr(basic_attr, ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID, manufacturerName);
    esp_zb_cluster_list_add_basic_cluster(list, basic_attr, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);

    // ON/OFF
    static bool onOffValue = true; 
    esp_zb_on_off_cluster_cfg_t on_off_cfg = { .on_off = onOffValue };
    esp_zb_attribute_list_t *on_off_attr = esp_zb_on_off_cluster_create(&on_off_cfg);
    esp_zb_cluster_list_add_on_off_cluster(list, on_off_attr, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);

    // LEVEL CONTROL
    static uint8_t currentLevel;
    currentLevel = static_cast<uint8_t>((Servo::GetServoMotor().GetTargetAngle() * 254 + 90) / 180);
    
    esp_zb_level_cluster_cfg_t level_cfg = { .current_level = currentLevel };
    esp_zb_attribute_list_t *level_attr = esp_zb_level_cluster_create(&level_cfg);

    static uint8_t  onLevel = 255; 
    static uint8_t  minLevel = 0;
    static uint8_t  maxLevel = 254;
    static uint16_t remainingTime = 0;
    static uint16_t transTime = 0;
    static uint8_t  options = 0x00;
    static uint16_t startUpLevel = 255;

    esp_zb_level_cluster_add_attr(level_attr, ESP_ZB_ZCL_ATTR_LEVEL_CONTROL_REMAINING_TIME_ID, &remainingTime);
    esp_zb_level_cluster_add_attr(level_attr, ESP_ZB_ZCL_ATTR_LEVEL_CONTROL_MIN_LEVEL_ID, &minLevel);
    esp_zb_level_cluster_add_attr(level_attr, ESP_ZB_ZCL_ATTR_LEVEL_CONTROL_MAX_LEVEL_ID, &maxLevel);
    esp_zb_level_cluster_add_attr(level_attr, ESP_ZB_ZCL_ATTR_LEVEL_CONTROL_OPTIONS_ID, &options);
    esp_zb_level_cluster_add_attr(level_attr, ESP_ZB_ZCL_ATTR_LEVEL_CONTROL_ON_LEVEL_ID, &onLevel);
    esp_zb_level_cluster_add_attr(level_attr, ESP_ZB_ZCL_ATTR_LEVEL_CONTROL_START_UP_CURRENT_LEVEL_ID, &startUpLevel);
    esp_zb_level_cluster_add_attr(level_attr, ESP_ZB_ZCL_ATTR_LEVEL_CONTROL_ON_OFF_TRANSITION_TIME_ID, &transTime);
    esp_zb_level_cluster_add_attr(level_attr, ESP_ZB_ZCL_ATTR_LEVEL_CONTROL_ON_TRANSITION_TIME_ID, &transTime);
    esp_zb_level_cluster_add_attr(level_attr, ESP_ZB_ZCL_ATTR_LEVEL_CONTROL_OFF_TRANSITION_TIME_ID, &transTime);

    esp_zb_cluster_list_add_level_cluster(list, level_attr, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);

    return list;
}

void sendServoPosition(uint8_t endpoint, uint8_t position)
{
    if (connected.load() == false)
    {
        DebugLogger::getInstance().print(DEBUG_ZIGBEE, DEBUG_ERROR, "not connected to zigbee, can't report attribute");
        return;
    }

    uint8_t servo_angle = ((uint32_t)position * 254) / 180;

    DebugLogger::getInstance().print(DEBUG_ZIGBEE, DEBUG_INFO, "Try to update ep%d attribute to %d", endpoint, servo_angle);

    if (esp_zb_lock_acquire(portMAX_DELAY))
    {
        DebugLogger::getInstance().print(DEBUG_ZIGBEE, DEBUG_INFO, "Try to change attribute localy");

        esp_zb_zcl_status_t status = esp_zb_zcl_set_attribute_val(
            endpoint,
            ESP_ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL,
            ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
            ESP_ZB_ZCL_ATTR_LEVEL_CONTROL_CURRENT_LEVEL_ID,
            &servo_angle,
            false // localy changed
        );
        if (status != ESP_ZB_ZCL_STATUS_SUCCESS)
        {
            DebugLogger::getInstance().print(DEBUG_ZIGBEE, DEBUG_ERROR, "set attribut localy failed");
            esp_zb_lock_release();
            return;
        }

        DebugLogger::getInstance().print(DEBUG_ZIGBEE, DEBUG_INFO, "Try to send attribute to coordinator");

        esp_zb_zcl_report_attr_cmd_t report_cmd;
        memset(&report_cmd, 0, sizeof(esp_zb_zcl_report_attr_cmd_t));
        report_cmd.zcl_basic_cmd.dst_addr_u.addr_short = 0x0000;
        report_cmd.zcl_basic_cmd.dst_endpoint = 1;
        report_cmd.zcl_basic_cmd.src_endpoint = endpoint;
        report_cmd.address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT;
        report_cmd.clusterID = ESP_ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL;
        report_cmd.attributeID = ESP_ZB_ZCL_ATTR_LEVEL_CONTROL_CURRENT_LEVEL_ID;
        report_cmd.direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_CLI;
        report_cmd.dis_default_resp = 1;
        report_cmd.manuf_specific = 0;
        esp_zb_zcl_report_attr_cmd_req(&report_cmd);

        esp_zb_lock_release();
    } 
}

static void bdb_start_top_level_commissioning_cb(uint8_t mode_mask)
{
    if(esp_zb_bdb_start_top_level_commissioning(mode_mask) != ESP_OK)
    {
        DebugLogger::getInstance().print(DEBUG_ZIGBEE, DEBUG_WARNING, "commissioning failed");
    }
}
void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct)
{
    uint32_t *p_sg_p = signal_struct->p_app_signal;
    esp_err_t err_status = signal_struct->esp_err_status;
    esp_zb_app_signal_type_t sig_type = static_cast<esp_zb_app_signal_type_t>(*p_sg_p);
    switch (sig_type)
    {
    case ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP:
        DebugLogger::getInstance().print(DEBUG_ZIGBEE, DEBUG_INFO, "initialize Zigbee stack");
        esp_zb_scheduler_alarm(bdb_start_top_level_commissioning_cb, ESP_ZB_BDB_MODE_INITIALIZATION, 1000);
        break;
        
    case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:
    case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT:
        if (err_status == ESP_OK)
        {
            DebugLogger::getInstance().print(DEBUG_ZIGBEE, DEBUG_INFO, "Deferred driver initialization ...");
            DebugLogger::getInstance().print(DEBUG_ZIGBEE, DEBUG_INFO, "Device started up in %s factory-reset mode", esp_zb_bdb_is_factory_new() ? "" : "non");
            if (esp_zb_bdb_is_factory_new())
            {
                DebugLogger::getInstance().print(DEBUG_ZIGBEE, DEBUG_INFO, "start network steering");
                esp_zb_scheduler_alarm(bdb_start_top_level_commissioning_cb, ESP_ZB_BDB_MODE_NETWORK_STEERING, 1000);
            }
            else
            {
                DebugLogger::getInstance().print(DEBUG_ZIGBEE, DEBUG_INFO, "device reconnected");
                connected.store(true);
            }
        }
        else
        {
            DebugLogger::getInstance().print(DEBUG_ZIGBEE, DEBUG_INFO, "failed to initialize Zigbee stack (status: %s)", esp_err_to_name(err_status));
        }
        break;

    case ESP_ZB_BDB_SIGNAL_STEERING:
        if (err_status == ESP_OK)
        {
            esp_zb_ieee_addr_t extended_pan_id;
            esp_zb_get_extended_pan_id(extended_pan_id);
            DebugLogger::getInstance().print(DEBUG_ZIGBEE, DEBUG_INFO, "joined network successfully (Extended PAN ID: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x, PAN ID: 0x%04hx, Channel:%d, Short Address: 0x%04hx)",
            extended_pan_id[7], extended_pan_id[6], extended_pan_id[5], extended_pan_id[4],
                     extended_pan_id[3], extended_pan_id[2], extended_pan_id[1], extended_pan_id[0],
                    esp_zb_get_pan_id(), esp_zb_get_current_channel(), esp_zb_get_short_address());
            connected.store(true);
        }
        else
        {
            DebugLogger::getInstance().print(DEBUG_ZIGBEE, DEBUG_INFO, "network steering was not successful (status: %s)", esp_err_to_name(err_status));
            esp_zb_scheduler_alarm((esp_zb_callback_t)bdb_start_top_level_commissioning_cb, ESP_ZB_BDB_MODE_NETWORK_STEERING, 1000);
        }
        break;        

    default:
        if(sig_type != ESP_ZB_COMMON_SIGNAL_CAN_SLEEP)
        {
            DebugLogger::getInstance().print(DEBUG_ZIGBEE, DEBUG_INFO, "ZDO signal: %s (0x%x), status: %s", esp_zb_zdo_signal_to_string(sig_type), sig_type, esp_err_to_name(err_status));
        }
        break;
    }
}

static esp_err_t zbActionHandler(esp_zb_core_action_callback_id_t callback_id, const void *data)
{
    switch (callback_id)
    {
    case ESP_ZB_CORE_SET_ATTR_VALUE_CB_ID:
    {
        auto *message = (esp_zb_zcl_set_attr_value_message_t*)data;
        if (!message)
        {
            DebugLogger::getInstance().print(DEBUG_ZIGBEE, DEBUG_ERROR, "empty message");
            return ESP_FAIL;
        }

        if (message->info.status != ESP_ZB_ZCL_STATUS_SUCCESS)
        {
            DebugLogger::getInstance().print(DEBUG_ZIGBEE, DEBUG_ERROR, "message status error : %d", message->info.status);
            return ESP_FAIL;
        }

        DebugLogger::getInstance().print(DEBUG_ZIGBEE, DEBUG_INFO, "Received message: endpoint(%d), cluster(0x%x), attribute(0x%x), data size(%d)",
                                         message->info.dst_endpoint, message->info.cluster, message->attribute.id, message->attribute.data.size);

        if (message->info.dst_endpoint == EP_SERVOMOTOR)
        {
            if (message->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL)
            {
                if (message->attribute.id == ESP_ZB_ZCL_ATTR_LEVEL_CONTROL_CURRENT_LEVEL_ID && message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_U8)
                {
                    uint8_t level = message->attribute.data.value ? *(uint8_t*)message->attribute.data.value : 0;
                    DebugLogger::getInstance().print(DEBUG_ZIGBEE, DEBUG_INFO, "Slider level received: %d / 254", level);

                    uint8_t servo_angle = ((uint32_t)level * 180 + 90) / 254;
                    DebugLogger::getInstance().print(DEBUG_ZIGBEE, DEBUG_INFO, "Mapped servo angle: %d°", servo_angle);

                    Servo::GetServoMotor().SetTargetAngle(servo_angle);
                }
            }
        }

        DebugLogger::getInstance().print(DEBUG_ZIGBEE, DEBUG_INFO, "Zigbee action callback handled (ESP_ZB_CORE_SET_ATTR_VALUE_CB_ID)");
    }
    break;

    case ESP_ZB_CORE_CMD_READ_ATTR_RESP_CB_ID:
        DebugLogger::getInstance().print(DEBUG_ZIGBEE, DEBUG_INFO, "Asked to read attribute");
        break;

    case ESP_ZB_CORE_REPORT_ATTR_CB_ID:
        DebugLogger::getInstance().print(DEBUG_ZIGBEE, DEBUG_INFO, "Attribute successfully reported to coordinator");
        break;

    case ESP_ZB_CORE_IAS_ZONE_ENROLL_RESPONSE_VALUE_CB_ID:
        DebugLogger::getInstance().print(DEBUG_ZIGBEE, DEBUG_INFO, "Enrolling successful");
        break;

    case ESP_ZB_CORE_CMD_DEFAULT_RESP_CB_ID:
        DebugLogger::getInstance().print(DEBUG_ZIGBEE, DEBUG_INFO, "Attribute report successful");
        break;

    default:
        DebugLogger::getInstance().print(DEBUG_ZIGBEE, DEBUG_INFO, "Unhandled action (ID:%d)", callback_id);
        break;
    }
    return ESP_OK;
}


esp_err_t initZigbee()
{
    esp_zb_cfg_t cfg{};
    cfg.esp_zb_role = ESP_ZB_DEVICE_TYPE_ED;
    cfg.install_code_policy = false;
    cfg.nwk_cfg.zed_cfg.ed_timeout = ESP_ZB_ED_AGING_TIMEOUT_64MIN;
    cfg.nwk_cfg.zed_cfg.keep_alive = 3600;
    esp_zb_init(&cfg);

    return ESP_OK;
}


esp_err_t initDevice(void)
{
    esp_zb_ep_list_t *ep_list = esp_zb_ep_list_create();

    // EP 1 - Level control
    esp_zb_endpoint_config_t ep1_config = { .endpoint = EP_SERVOMOTOR, .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID, 
                                            .app_device_id = ESP_ZB_HA_CUSTOM_TUNNEL_DEVICE_ID, .app_device_version = 0 };
    esp_zb_ep_list_add_ep(ep_list, createLevelClusterList(), ep1_config);

    esp_zb_device_register(ep_list);
    esp_zb_core_action_handler_register(zbActionHandler);

    return ESP_OK;
}
