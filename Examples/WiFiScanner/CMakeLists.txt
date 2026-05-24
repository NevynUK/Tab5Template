cmake_minimum_required(VERSION 3.16)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_STANDARD 11)

list(APPEND EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/../../components/Tab5")
list(APPEND EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/../../managed_components")

set(IDF_TARGET "esp32p4")
set(SDKCONFIG_DEFAULTS "${CMAKE_CURRENT_SOURCE_DIR}/../../sdkconfig.defaults")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(WiFiScanner)

#
#   Suppress spurious warnings from ESP-IDF internal headers.
#   -Wno-literal-suffix silences the invalid-suffix-on-literal warning
#   in esp_hw_support/periph_ctrl.h caused by a missing space between
#   a string literal and the CONFIG_IDF_TARGET macro in ESP-IDF v5.5.x.
#   The flag is only valid for C++/ObjC++, so guard it with a generator
#   expression to prevent it being passed to the C compiler.
#
#   -Wno-missing-field-initializers silences the warning fired by the
#   gpio_ll_matrix_out_default() inline function in the ESP32-P4 GPIO
#   HAL header (gpio_ll.h) when included by third-party components.
#   The HAL header is not treated as a system header by GCC, so the
#   warning is not suppressed by the per-component -w flag above.
#   Valid for both C and C++; no generator expression required.
#
idf_build_set_property(COMPILE_OPTIONS "$<$<COMPILE_LANGUAGE:CXX>:-Wno-literal-suffix>" APPEND)
idf_build_set_property(COMPILE_OPTIONS "-Wno-missing-field-initializers" APPEND)
