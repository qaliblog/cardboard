# Copyright 2024 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cmake_minimum_required(VERSION 3.22.1)
project(videoplayer_android VERSION 1.0.0 LANGUAGES CXX)

# C++ flags.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
add_compile_options(-Wall -Wextra)

# Standard Android dependencies
find_library(ANDROID_LIB android)
find_library(GLESv2_LIB GLESv2)
find_library(GLESv3_LIB GLESv3)
find_library(LOG_LIB log)

set(libs_dir ${CMAKE_CURRENT_SOURCE_DIR}/libraries)

set(CARDBOARD_LIB ${libs_dir}/jni/${ANDROID_ABI}/libGfxPluginCardboard.so)
set(CARDBOARD_INCLUDE_DIR ${libs_dir})

# OpenCV - try to find it in the Maven dependency
find_package(OpenCV QUIET)
if(NOT OpenCV_FOUND)
    # Try to find OpenCV in common Android locations
    set(OpenCV_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/main/jni/opencv)
    if(EXISTS ${OpenCV_DIR})
        find_package(OpenCV REQUIRED PATHS ${OpenCV_DIR})
    else()
        message(WARNING "OpenCV not found, building without OpenCV support")
        set(OPENCV_AVAILABLE FALSE)
    endif()
else()
    set(OPENCV_AVAILABLE TRUE)
endif()

# === Video Player ===
# Sources
file(GLOB native_srcs "src/main/jni/*.cc")
# Output binary
add_library(videoplayer_jni SHARED ${native_srcs})
# Includes
target_include_directories(videoplayer_jni PRIVATE ${CARDBOARD_INCLUDE_DIR})
target_include_directories(videoplayer_jni PRIVATE ${OpenCV_INCLUDE_DIRS})
# Build
target_link_libraries(videoplayer_jni
    ${ANDROID_LIB}
    ${GLESv2_LIB}
    ${GLESv3_LIB}
    ${LOG_LIB}
    ${CARDBOARD_LIB}
    ${OpenCV_LIBS})