# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

# Fix behavior of CMAKE_CXX_STANDARD when targeting macOS.
if (POLICY CMP0025)
    cmake_policy(SET CMP0025 NEW)
endif ()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

#
#   Set the language standards.
#
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_STANDARD 11)

#
#   Add extra components.
#
list(APPEND EXTRA_COMPONENT_DIRS components/Tab5)

project(Tab5Template)