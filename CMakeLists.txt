# Specify the minimum version of CMake required
cmake_minimum_required(VERSION 3.16)

# Set the path to the ESP-IDF framework, which is included as a submodule.
# This makes the project self-contained.
set(IDF_PATH ${CMAKE_CURRENT_SOURCE_DIR}/esp-idf)

# Explicitly set the target board.
# This avoids ambiguity and ensures the correct toolchain is used from the start.
set(IDF_TARGET esp32s3)

# Set the ESP-IDF toolchain file.
# This is the crucial step. This file tells CMake how to find the correct
# compilers, Python interpreter, and all the necessary tools for the ESP32.
# It automatically handles the environment setup that was being done manually before.
set(CMAKE_TOOLCHAIN_FILE ${IDF_PATH}/tools/cmake/toolchain.cmake)

# Include the main ESP-IDF project cmake file. This file contains all the
# logic for building ESP-IDF components and the final application.
include(${IDF_PATH}/tools/cmake/project.cmake)

# Define the project name. This must be called AFTER including project.cmake.
project(ccabn_tracker)