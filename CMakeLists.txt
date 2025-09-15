# Specify the minimum version of CMake required
cmake_minimum_required(VERSION 3.16)

# Set the path to the ESP-IDF framework, which is included as a submodule.
# This makes the project self-contained.
set(IDF_PATH ${CMAKE_CURRENT_SOURCE_DIR}/esp-idf)

# Force ESP32-S3 target for all builds - this must be set before any ESP-IDF includes
# This ensures consistent target selection on fresh clones
set(ENV{IDF_TARGET} esp32s3)
set(ENV{IDF_PATH} ${IDF_PATH})

# Auto-install ESP-IDF tools if not present
if(NOT EXISTS "${IDF_PATH}/tools/tools.json")
    message(FATAL_ERROR "ESP-IDF submodule not initialized. Run: git submodule update --init --recursive")
endif()

# Auto-detect and set up ESP-IDF toolchain and Python environment
file(GLOB ESP_TOOLCHAIN_DIRS "$ENV{HOME}/.espressif/tools/xtensa-esp-elf/*/xtensa-esp-elf/bin")
file(GLOB ESP_PYTHON_DIRS "$ENV{HOME}/.espressif/python_env/idf*/bin")
file(GLOB ESP_ROM_ELF_DIRS "$ENV{HOME}/.espressif/tools/esp-rom-elfs/*/")

if(ESP_TOOLCHAIN_DIRS AND ESP_PYTHON_DIRS AND ESP_ROM_ELF_DIRS)
    list(GET ESP_TOOLCHAIN_DIRS 0 ESP_TOOLCHAIN_DIR)
    list(GET ESP_PYTHON_DIRS 0 ESP_PYTHON_DIR)
    list(GET ESP_ROM_ELF_DIRS 0 ESP_ROM_ELF_DIR)
    set(ENV{PATH} "${ESP_PYTHON_DIR}:${ESP_TOOLCHAIN_DIR}:$ENV{PATH}")
    set(ENV{ESP_ROM_ELF_DIR} "${ESP_ROM_ELF_DIR}")
    message(STATUS "Found ESP toolchain at: ${ESP_TOOLCHAIN_DIR}")
    message(STATUS "Found ESP Python environment at: ${ESP_PYTHON_DIR}")
    message(STATUS "Found ESP ROM ELF directory at: ${ESP_ROM_ELF_DIR}")
else()
    message(STATUS "ESP-IDF tools not found. Installing for esp32s3...")
    execute_process(
        COMMAND ${IDF_PATH}/install.sh esp32s3
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        RESULT_VARIABLE INSTALL_RESULT
    )
    if(NOT INSTALL_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to install ESP-IDF tools")
    endif()

    # Find the newly installed toolchain, Python environment, and ROM ELF directory
    file(GLOB ESP_TOOLCHAIN_DIRS "$ENV{HOME}/.espressif/tools/xtensa-esp-elf/*/xtensa-esp-elf/bin")
    file(GLOB ESP_PYTHON_DIRS "$ENV{HOME}/.espressif/python_env/idf*/bin")
    file(GLOB ESP_ROM_ELF_DIRS "$ENV{HOME}/.espressif/tools/esp-rom-elfs/*/")

    if(ESP_TOOLCHAIN_DIRS AND ESP_PYTHON_DIRS AND ESP_ROM_ELF_DIRS)
        list(GET ESP_TOOLCHAIN_DIRS 0 ESP_TOOLCHAIN_DIR)
        list(GET ESP_PYTHON_DIRS 0 ESP_PYTHON_DIR)
        list(GET ESP_ROM_ELF_DIRS 0 ESP_ROM_ELF_DIR)
        set(ENV{PATH} "${ESP_PYTHON_DIR}:${ESP_TOOLCHAIN_DIR}:$ENV{PATH}")
        set(ENV{ESP_ROM_ELF_DIR} "${ESP_ROM_ELF_DIR}")
        message(STATUS "Installed and found ESP toolchain at: ${ESP_TOOLCHAIN_DIR}")
        message(STATUS "Installed and found ESP Python environment at: ${ESP_PYTHON_DIR}")
        message(STATUS "Installed and found ESP ROM ELF directory at: ${ESP_ROM_ELF_DIR}")
    else()
        message(FATAL_ERROR "Failed to find ESP-IDF toolchain, Python environment, or ROM ELF directory after installation")
    endif()
endif()

# Include the main ESP-IDF project cmake file. This file contains all the
# logic for building ESP-IDF components and the final application.
include(${IDF_PATH}/tools/cmake/project.cmake)

# Define the project name. This must be called AFTER including project.cmake.
project(ccabn_tracker)