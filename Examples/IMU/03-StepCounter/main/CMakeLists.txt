idf_component_register(
    SRCS "Example03_StepCounter.cpp"
    INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/../../../../components/Tab5"
    REQUIRES Tab5 esp_driver_i2c
)
