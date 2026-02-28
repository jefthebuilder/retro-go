cmake_minimum_required(VERSION 3.16)

# Define your app's name
set(PROJECT_NAME "my-custom-app")

# Set the path to retro-go root
# Respect environment variable if set, otherwise use relative path
if(DEFINED ENV{RETRO_GO_PATH})
    set(RETRO_GO_PATH "$ENV{RETRO_GO_PATH}" CACHE PATH "Path to retro-go root")
else()
    set(RETRO_GO_PATH "${CMAKE_CURRENT_SOURCE_DIR}/.." CACHE PATH "Path to retro-go root")
endif()

# Set build target (device type) - defaults to jaf1 if not specified
if(NOT DEFINED RG_BUILD_TARGET)
    set(RG_BUILD_TARGET "RG_TARGET_JAF1" CACHE STRING "Build target device type")
endif()

# EXTRA_COMPONENT_DIRS should point to directories CONTAINING components
# The retro-go/components directory contains both retro-go and appstore components
set(EXTRA_COMPONENT_DIRS "${RETRO_GO_PATH}/components")

message(STATUS "RETRO_GO_PATH: ${RETRO_GO_PATH}")
message(STATUS "EXTRA_COMPONENT_DIRS: ${EXTRA_COMPONENT_DIRS}")

# Include retro-go's base.cmake (this includes project.cmake)
include("${RETRO_GO_PATH}/base.cmake")

# Define the project
project(${PROJECT_NAME})
