# Set the minimum required version of CMake
cmake_minimum_required(VERSION 3.15)

# Define the project name and language
project(sylar_study CXX)

# --- Compiler Settings ---
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)


# --- Source Files ---
# **This is the corrected section**
# List all your .cpp files with their correct paths relative to this CMakeLists.txt file.
set(PROJECT_SOURCES
        log.cpp
        Config.cpp
       Config.h
        singleton.h
        thread.cpp
        thread.h
        mutex.cpp
        mutex.h                # This file is in the root directory
        # This file is inside the sylar/ directory
)


# --- Define the Executable Target ---
# Only pass the source (.cpp) files to the executable.
add_executable(sylar_study ${PROJECT_SOURCES}
        log.h
        log.cpp)

find_library(YAMLCPP yaml-cpp)

# --- Target Properties ---
# Tell the compiler where to find headers (#include "...")
target_include_directories(sylar_study PRIVATE
        # Add the project's root directory to the include path.
        # This lets you use #include "log.h" and #include "sylar/properties.h"
        ${CMAKE_CURRENT_SOURCE_DIR}
)