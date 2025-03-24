cmake_minimum_required(VERSION 3.13.1)

#Applications

#Services
list(APPEND ZEPHYR_EXTRA_MODULES ${CMAKE_CURRENT_LIST_DIR}/services/TEMP_ACC_SENSE_SERVICE)


#General
include_directories(${CMAKE_CURRENT_LIST_DIR}/general)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(CapstoneProj)
target_sources(app PRIVATE applications/main.c)
target_include_directories(app PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/services/TEMP_ACC_SENSE_SERVICE
    )