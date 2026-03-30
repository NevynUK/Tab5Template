idf_component_register(SRCS "SDCard.cpp" "Touch.cpp"
                    INCLUDE_DIRS "."
                    REQUIRES m5gfx driver freertos sdmmc fatfs)
