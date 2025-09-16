cmake_minimum_required(VERSION 3.26)
project(hft_sim LANGUAGES CXX)

# Options
option(HFT_ENABLE_LTO "Enable Link Time Optimization in Release" ON)
option(HFT_NATIVE_ARCH "Enable -march=native in Release" ON)
option(HFT_STRICT "Enable extra warnings" ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build types
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
endif()

# Common flags
if(MSVC)
  add_compile_options(/W4 /permissive- /Zc:preprocessor /Zc:__cplusplus)
  add_compile_definitions(_CRT_SECURE_NO_WARNINGS NOMINMAX)
else()
  add_compile_options(-Wall -Wextra -Wpedantic -Wconversion -Wno-sign-conversion)
  add_compile_options(-fno-exceptions -fno-rtti) # optional, code avoids exceptions/RTTI
  if(CMAKE_BUILD_TYPE MATCHES "Release")
    add_compile_options(-O3 -DNDEBUG)
    if(HFT_NATIVE_ARCH)
      add_compile_options(-march=native)
    endif()
  else()
    add_compile_options(-O0 -g3 -fno-omit-frame-pointer)
    # Address/UB sanitizers are useful during dev
    add_compile_options(-fsanitize=address,undefined)
    add_link_options(-fsanitize=address,undefined)
  endif()
endif()

if(HFT_ENABLE_LTO AND CMAKE_BUILD_TYPE MATCHES "Release")
  include(CheckIPOSupported)
  check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error)
  if(ipo_supported)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
  endif()
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Library target with core components (header-only where possible)
add_library(hft_core STATIC
    src/market/matching_engine.cpp
    src/market/simulator.cpp
    src/strategy/mean_reversion.cpp
    src/risk/risk_manager.cpp
    src/gateway/gateway_sim.cpp
)
target_include_directories(hft_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
if(NOT MSVC)
  target_link_libraries(hft_core PRIVATE pthread)
endif()

# Executables
add_executable(hft_app src/app/hft_main.cpp)
target_link_libraries(hft_app PRIVATE hft_core)

add_executable(sim_app src/app/sim_main.cpp)
target_link_libraries(sim_app PRIVATE hft_core)

add_executable(sim_scenarios tests/functional_scenarios.cpp)
target_link_libraries(sim_scenarios PRIVATE hft_core)

# Nice defaults for faster local iteration
if(MSVC)
  target_compile_options(hft_core PRIVATE /MP)
  target_compile_options(hft_app PRIVATE /MP)
  target_compile_options(sim_app PRIVATE /MP)
  target_compile_options(sim_scenarios PRIVATE /MP)
endif()
