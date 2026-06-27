# ESP32 Mesh Node Firmware CMake Configuration
# Adaptive mesh topology with UDP unicast and dynamic TX power

cmake_minimum_required(VERSION 3.5)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(emergent_esp32_mesh)

# Set project version
set(PROJECT_VERSION "1.0.0")

# ============================================================================
# Configuration Options
# ============================================================================

# Enable mesh networking
set(COMPONENT_CONFIG_ESP_MESH_ENABLED ON)
set(COMPONENT_CONFIG_ESP_MESH_MAX_HOPS 10)
set(COMPONENT_CONFIG_ESP_MESH_VOTE_PERCENTAGE 1)

# Enable WiFi
set(COMPONENT_CONFIG_ESP_WIFI_SOFTAP_SUPPORT ON)
set(COMPONENT_CONFIG_ESP_WIFI_ESPNOW_SUPPORT ON)
set(COMPONENT_CONFIG_ESP_WIFI_MESH_SUPPORT ON)

# Enable lwIP
set(COMPONENT_CONFIG_LWIP_DHCP_SERVER ON)
set(COMPONENT_CONFIG_LWIP_DNS_SERVER ON)
set(COMPONENT_CONFIG_LWIP_IPV6 ON)

# Enable promiscuous mode
set(COMPONENT_CONFIG_ESP_WIFI_SOFTAP_PROMISCUOUS ON)

# ============================================================================
# Source Files
# ============================================================================

set(MAIN_SRC_FILES
    main/main.cpp
    main/mesh_node.cpp
    main/beacon_monitor.cpp
    main/state_manager.cpp
)

set(MAIN_INCLUDE_DIRS
    main
)

# ============================================================================
# Component Configuration
# ============================================================================

# Add main component
idf_component_register(
    SRCS ${MAIN_SRC_FILES}
    INCLUDE_DIRS ${MAIN_INCLUDE_DIRS}
    REQUIRES esp_wifi esp_mesh lwip esp_event esp_timer driver
)

# ============================================================================
# Target Configuration
# ============================================================================

# Set target chip (ESP32-C5 for 5GHz support)
set(ESP_PLATFORM "esp32c5")

# Enable C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Optimization flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -Wall -Wextra")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2 -Wall -Wextra")

# Debug symbols
if(CONFIG_DEBUG)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g")
endif()

# ============================================================================
# SDK Configuration
# ============================================================================

# Create default sdkconfig
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/sdkconfig.defaults
    ${CMAKE_BINARY_DIR}/sdkconfig
    COPYONLY
)

# ============================================================================
# Build Targets
# ============================================================================

# Add custom targets
add_custom_target(
    flash
    COMMAND idf.py -p ${FLASH_PORT} flash
    DEPENDS ${PROJECT_NAME}.elf
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_custom_target(
    monitor
    COMMAND idf.py -p ${FLASH_PORT} monitor
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_custom_target(
    fullclean
    COMMAND rm -rf ${CMAKE_BINARY_DIR} build
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
