cmake_minimum_required(VERSION 3.16)
project(input_nav_harness LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(PROJECT_ROOT "${CMAKE_CURRENT_LIST_DIR}/../..")
set(SEEDSIGNER_DIR "${PROJECT_ROOT}/components/seedsigner")
set(NLOHMANN_JSON_INCLUDE_DIR "${PROJECT_ROOT}/components/nlohmann_json/include")

if(NOT DEFINED LVGL_ROOT OR LVGL_ROOT STREQUAL "")
    set(LVGL_ROOT "${PROJECT_ROOT}/../micropython/ports/esp32/managed_components/lvgl__lvgl")
endif()

if(NOT EXISTS "${LVGL_ROOT}/lvgl.h")
    set(LVGL_ROOT "${PROJECT_ROOT}/references/micropython/ports/esp32/managed_components/lvgl__lvgl")
endif()

if(NOT EXISTS "${LVGL_ROOT}/lvgl.h")
    message(FATAL_ERROR "LVGL root not found. Pass -DLVGL_ROOT=<path>")
endif()

if(NOT EXISTS "${NLOHMANN_JSON_INCLUDE_DIR}/nlohmann/json.hpp")
  message(FATAL_ERROR "nlohmann/json.hpp not found at: ${NLOHMANN_JSON_INCLUDE_DIR}")
endif()

file(GLOB_RECURSE LVGL_SOURCES CONFIGURE_DEPENDS
  "${LVGL_ROOT}/src/*.c"
)

set(SEEDSIGNER_FONT_SOURCES
  "${SEEDSIGNER_DIR}/fonts/opensans_semibold_20_4bpp_125x.c"
  "${SEEDSIGNER_DIR}/fonts/opensans_semibold_18_4bpp_125x.c"
  "${SEEDSIGNER_DIR}/fonts/opensans_regular_17_4bpp_125x.c"
  "${SEEDSIGNER_DIR}/fonts/seedsigner_icons_24_4bpp_125x.c"
  "${SEEDSIGNER_DIR}/fonts/seedsigner_icons_36_4bpp_125x.c"
)

set(SEEDSIGNER_CORE_SOURCES
  "${SEEDSIGNER_DIR}/components.cpp"
  "${SEEDSIGNER_DIR}/input_profile.cpp"
  "${SEEDSIGNER_DIR}/navigation.cpp"
  "${SEEDSIGNER_DIR}/seedsigner.cpp"
)

add_executable(input_nav_harness
  ${LVGL_SOURCES}
  ${SEEDSIGNER_FONT_SOURCES}
  ${SEEDSIGNER_CORE_SOURCES}
  "${CMAKE_CURRENT_LIST_DIR}/input_nav_harness.cpp"
)

target_compile_definitions(input_nav_harness PRIVATE LV_CONF_SKIP)

target_include_directories(input_nav_harness PRIVATE
  "${LVGL_ROOT}"
  "${SEEDSIGNER_DIR}"
  "${NLOHMANN_JSON_INCLUDE_DIR}"
)
