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

    # Ensure ESP-IDF environment is properly set up
    execute_process(
        COMMAND bash -c "source ${IDF_PATH}/export.sh && env"
        OUTPUT_VARIABLE ESP_ENV_OUTPUT
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    # Parse the environment variables and set them
    string(REPLACE "\n" ";" ESP_ENV_LIST "${ESP_ENV_OUTPUT}")
    foreach(ENV_VAR ${ESP_ENV_LIST})
        if(ENV_VAR MATCHES "^([^=]+)=(.*)$")
            set(ENV{${CMAKE_MATCH_1}} "${CMAKE_MATCH_2}")
        endif()
    endforeach()

    message(STATUS "Found ESP toolchain at: ${ESP_TOOLCHAIN_DIR}")
    message(STATUS "Found ESP Python environment at: ${ESP_PYTHON_DIR}")
    message(STATUS "Found ESP ROM ELF directory at: ${ESP_ROM_ELF_DIR}")
    message(STATUS "ESP-IDF environment configured")
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

        # Install ESP-IDF Python requirements if not already installed
        execute_process(
            COMMAND ${ESP_PYTHON_DIR}/pip install -r ${IDF_PATH}/tools/requirements/requirements.core.txt
            OUTPUT_VARIABLE PIP_OUTPUT
            ERROR_VARIABLE PIP_ERROR
            RESULT_VARIABLE PIP_RESULT
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_STRIP_TRAILING_WHITESPACE
        )
        if(PIP_RESULT EQUAL 0)
            message(STATUS "ESP-IDF Python requirements verified/installed")
        else()
            message(WARNING "Failed to install ESP-IDF Python requirements: ${PIP_ERROR}")
        endif()

        message(STATUS "Installed and found ESP toolchain at: ${ESP_TOOLCHAIN_DIR}")
        message(STATUS "Installed and found ESP Python environment at: ${ESP_PYTHON_DIR}")
        message(STATUS "Installed and found ESP ROM ELF directory at: ${ESP_ROM_ELF_DIR}")
        message(STATUS "ESP-IDF environment configured")
    else()
        message(FATAL_ERROR "Failed to find ESP-IDF toolchain, Python environment, or ROM ELF directory after installation")
    endif()
endif()

# Ensure ESP-IDF environment is properly set up before including project.cmake
execute_process(
    COMMAND bash -c "source ${IDF_PATH}/export.sh && env"
    OUTPUT_VARIABLE ESP_ENV_OUTPUT
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
# Parse the environment variables and set them
string(REPLACE "\n" ";" ESP_ENV_LIST "${ESP_ENV_OUTPUT}")
foreach(ENV_VAR ${ESP_ENV_LIST})
    if(ENV_VAR MATCHES "^([^=]+)=(.*)$")
        set(ENV{${CMAKE_MATCH_1}} "${CMAKE_MATCH_2}")
    endif()
endforeach()

# Explicitly set the compilers to ensure bootloader sub-project finds them
find_program(CMAKE_C_COMPILER xtensa-esp32s3-elf-gcc)
find_program(CMAKE_CXX_COMPILER xtensa-esp32s3-elf-g++)
find_program(CMAKE_ASM_COMPILER xtensa-esp32s3-elf-gcc)

# Force ESP-IDF to use the correct Python interpreter
if(ESP_PYTHON_DIRS)
    list(GET ESP_PYTHON_DIRS 0 ESP_PYTHON_DIR)
    set(PYTHON "${ESP_PYTHON_DIR}/python3")
    message(STATUS "Using ESP-IDF Python: ${PYTHON}")
endif()

# Set cache variables for bootloader sub-project to inherit
set(CMAKE_C_COMPILER "${CMAKE_C_COMPILER}" CACHE FILEPATH "C compiler for bootloader")
set(CMAKE_CXX_COMPILER "${CMAKE_CXX_COMPILER}" CACHE FILEPATH "CXX compiler for bootloader")
set(CMAKE_ASM_COMPILER "${CMAKE_ASM_COMPILER}" CACHE FILEPATH "ASM compiler for bootloader")
set(PYTHON "${PYTHON}" CACHE FILEPATH "Python interpreter for bootloader")

# Set environment variables that the bootloader configuration will inherit
set(ENV{CC} "${CMAKE_C_COMPILER}")
set(ENV{CXX} "${CMAKE_CXX_COMPILER}")
set(ENV{ASM} "${CMAKE_ASM_COMPILER}")
set(ENV{CMAKE_C_COMPILER} "${CMAKE_C_COMPILER}")
set(ENV{CMAKE_CXX_COMPILER} "${CMAKE_CXX_COMPILER}")
set(ENV{CMAKE_ASM_COMPILER} "${CMAKE_ASM_COMPILER}")

message(STATUS "ESP-IDF environment configured before project setup")

# Include the main ESP-IDF project cmake file. This file contains all the
# logic for building ESP-IDF components and the final application.
include(${IDF_PATH}/tools/cmake/project.cmake)

# Define the project name. This must be called AFTER including project.cmake.
project(ccabn_tracker)

# Custom target for full flash using idf.py (bypasses bootloader environment issues)
add_custom_target(flash-all
    COMMAND bash -c "source ${IDF_PATH}/export.sh && python3 ${IDF_PATH}/tools/idf.py build"
    COMMAND bash -c "source ${IDF_PATH}/export.sh && python3 ${IDF_PATH}/tools/idf.py flash"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Building and flashing complete firmware using idf.py"
    USES_TERMINAL
)

# Custom target for full erase and flash
add_custom_target(flash-fresh
    COMMAND bash -c "source ${IDF_PATH}/export.sh && python3 ${IDF_PATH}/tools/idf.py erase_flash"
    COMMAND bash -c "source ${IDF_PATH}/export.sh && python3 ${IDF_PATH}/tools/idf.py build"
    COMMAND bash -c "source ${IDF_PATH}/export.sh && python3 ${IDF_PATH}/tools/idf.py flash"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Erasing flash and flashing complete firmware for fresh ESP32 chips"
    USES_TERMINAL
)