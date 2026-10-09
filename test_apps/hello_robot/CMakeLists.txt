cmake_minimum_required(VERSION 3.16)
# Reuse the exact Waveshare V2 board / codec initialization from the production
# project, but do not include the production voice component (single mic owner).
set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/../../components/board"
    "${CMAKE_CURRENT_LIST_DIR}/../../components/audio")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(hello_robot_inference_test)
