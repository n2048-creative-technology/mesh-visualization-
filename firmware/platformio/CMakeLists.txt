# PlatformIO CMake Configuration for ESP32 Mesh Node
# This file is used when building with PlatformIO in VS Code

cmake_minimum_required(VERSION 3.10)

# Project name
project(emergent_esp32_mesh CXX C)

# Set project version
set(PROJECT_VERSION "1.0.0")

# ============================================================================
# Platform Detection
# ============================================================================

# Detect platform from PlatformIO environment
if(DEFINED ENV{PIO_ENV})
    set(PIO_ENV $ENV{PIO_ENV})
else()
    set(PIO_ENV "esp32c5_devkitm_1")
endif()

# Set platform-specific flags
if(PIO_ENV MATCHES "esp32c3")
    add_definitions(-DESP_PLATFORM=ESP32_C3)
    add_definitions(-DWIFI_CHANNEL=6)  # 2.4 GHz channel
    message(STATUS "Configuring for ESP32-C3 (2.4 GHz)")
elif(PIO_ENV MATCHES "esp32c5")
    add_definitions(-DESP_PLATFORM=ESP32_C5)
    add_definitions(-DWIFI_CHANNEL=36)  # 5 GHz channel
    message(STATUS "Configuring for ESP32-C5 (5 GHz)")
else()
    add_definitions(-DESP_PLATFORM=ESP32)
    add_definitions(-DWIFI_CHANNEL=6)  # 2.4 GHz channel
    message(STATUS "Configuring for Generic ESP32 (2.4 GHz)")
endif()

# ============================================================================
# Source Files
# ============================================================================

# Main source files
set(SRC_FILES
    src/main.cpp
    src/mesh_node.cpp
    src/beacon_monitor.cpp
    src/state_manager.cpp
)

# Include directories
set(INCLUDE_DIRS
    include
)

# ============================================================================
# ESP-IDF Configuration
# ============================================================================

# Enable required ESP-IDF components
set(EXTRA_COMPONENT_DIRS "")

# ============================================================================
# Target Configuration
# ============================================================================

# Set target based on environment
if(PIO_ENV MATCHES "esp32c3")
    set(ESP_PLATFORM "esp32c3")
elif(PIO_ENV MATCHES "esp32c5")
    set(ESP_PLATFORM "esp32c5")
else()
    set(ESP_PLATFORM "esp32")
endif()

# ============================================================================
# Compiler Flags
# ============================================================================

# Standard flags
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Optimization flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -Wall -Wextra -Werror")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2 -Wall -Wextra -Werror")

# Debug symbols
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -DDEBUG")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -DDEBUG")
    add_definitions(
        -DDEBUG_MESH=true
        -DDEBUG_BEACON=true
        -DDEBUG_UDP=true
        -DDEBUG_NEIGHBORS=true
        -DDEBUG_TX_POWER=true
    )
endif()

# Platform-specific flags
add_definitions(
    -DMESH_MAX_HOPS=10
    -DMESH_VOTE_PERCENT=1
    -DUDP_PORT=1234
    -DMAX_NEIGHBORS=8
    -DRSSI_THRESHOLD=3
    -DNEIGHBOR_TIMEOUT_MS=5000
    -DTARGET_RSSI=-65
    -DMIN_TX_POWER=0
    -DMAX_TX_POWER=20
    -DTX_POWER_STEP=2
    -DSTATE_UPDATE_INTERVAL=100
    -DTEMPERATURE_SCALE=10.0
    -DVISUALIZATION_IP="192.168.1.254"
    -DVISUALIZATION_PORT=1234
)

# ============================================================================
# Create Executable
# ============================================================================

# Add executable for each platform
if(PIO_ENV MATCHES "esp32c3")
    add_executable(esp32c3_firmware ${SRC_FILES})
    target_include_directories(esp32c3_firmware PRIVATE ${INCLUDE_DIRS})
    target_compile_definitions(esp32c3_firmware PRIVATE ESP_PLATFORM=ESP32_C3)
    
elif(PIO_ENV MATCHES "esp32c5")
    add_executable(esp32c5_firmware ${SRC_FILES})
    target_include_directories(esp32c5_firmware PRIVATE ${INCLUDE_DIRS})
    target_compile_definitions(esp32c5_firmware PRIVATE ESP_PLATFORM=ESP32_C5)
    
else()
    add_executable(esp32_firmware ${SRC_FILES})
    target_include_directories(esp32_firmware PRIVATE ${INCLUDE_DIRS})
    target_compile_definitions(esp32_firmware PRIVATE ESP_PLATFORM=ESP32)
endif()

# ============================================================================
# PlatformIO Integration
# ============================================================================

# PlatformIO will use its own build system, so this CMakeLists.txt
# is mainly for reference and IDE integration

message(STATUS "PlatformIO project configured for: ${PIO_ENV}")
message(STATUS "Platform: ${ESP_PLATFORM}")
