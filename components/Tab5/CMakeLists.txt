idf_component_register(SRCS "Console.cpp" "WiFi.cpp" "Coprocessor.cpp" "SDCard.cpp" "Touch.cpp" "Rtc.cpp"
                    INCLUDE_DIRS "."
                    REQUIRES m5gfx driver freertos sdmmc fatfs esp_driver_i2c esp_hosted esp_wifi esp_netif nvs_flash)
