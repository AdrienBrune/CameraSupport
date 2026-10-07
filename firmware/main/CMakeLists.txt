idf_component_register(
    SRCS "main.cpp" "zigbee.cpp" "memory/pers_mem.cpp" "servomotor/servo.cpp"
    INCLUDE_DIRS "."
    PRIV_REQUIRES nvs_flash esp_driver_gpio esp_driver_uart ieee802154
)
