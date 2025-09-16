cmake_minimum_required(VERSION 3.26)
project(hft_sim LANGUAGES CXX)

# Options
option(HFT_ENABLE_LTO "Enable Link Time Optimization in Release" ON)
option(HFT_NATIVE_ARCH "Enable -march=native in Release" ON)
option(HFT_STRICT "Enable extra warnings" ON)
option(HFT_BUILD_TESTS "Build unit tests" ON)
option(HFT_ENABLE_COVERAGE "Enable coverage instrumentation" OFF)

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
  if(HFT_ENABLE_COVERAGE)
    add_compile_options(-O0 -g --coverage)
    add_link_options(--coverage)
  elseif(CMAKE_BUILD_TYPE MATCHES "Release")
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
add_library(hft_core INTERFACE)
target_include_directories(hft_core INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
if(NOT MSVC)
  target_link_libraries(hft_core INTERFACE pthread)
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
  target_compile_options(hft_core INTERFACE /MP)
  target_compile_options(hft_app PRIVATE /MP)
  target_compile_options(sim_app PRIVATE /MP)
  target_compile_options(sim_scenarios PRIVATE /MP)
endif()

if(HFT_BUILD_TESTS)
  include(CTest)
  enable_testing()

  include(FetchContent)
  set(FETCHCONTENT_QUIET OFF)
  FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.15.0.zip
  )
  # For MSVC: prevent overriding the parent project's compiler/linker settings
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)

  file(GLOB_RECURSE HFT_TEST_SOURCES CONFIGURE_DEPENDS tests/unit/*.cpp)
  add_executable(hft_unit_tests ${HFT_TEST_SOURCES})
  target_link_libraries(hft_unit_tests PRIVATE hft_core GTest::gtest_main GTest::gmock)
  target_compile_features(hft_unit_tests PRIVATE cxx_std_23)

  if(HFT_ENABLE_COVERAGE AND NOT MSVC)
    target_link_options(hft_unit_tests PRIVATE --coverage)
  endif()

  include(GoogleTest)
  gtest_discover_tests(hft_unit_tests)
endif()

if(HFT_ENABLE_COVERAGE AND NOT MSVC)
  find_program(GCOVR_EXECUTABLE gcovr)
  if(GCOVR_EXECUTABLE)
    set(COVERAGE_OUTPUT_DIR ${CMAKE_BINARY_DIR}/coverage)
    add_custom_target(
      coverage
      DEPENDS hft_unit_tests
      COMMAND ${CMAKE_COMMAND} -E make_directory ${COVERAGE_OUTPUT_DIR}
      COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
      COMMAND ${GCOVR_EXECUTABLE}
              --root ${CMAKE_SOURCE_DIR}
              --object-directory ${CMAKE_BINARY_DIR}
              --exclude "${CMAKE_SOURCE_DIR}/tests/functional.*"
              --exclude "${CMAKE_SOURCE_DIR}/build/.*"
              --xml ${COVERAGE_OUTPUT_DIR}/coverage.xml
      COMMAND ${GCOVR_EXECUTABLE}
              --root ${CMAKE_SOURCE_DIR}
              --object-directory ${CMAKE_BINARY_DIR}
              --exclude "${CMAKE_SOURCE_DIR}/tests/functional.*"
              --exclude "${CMAKE_SOURCE_DIR}/build/.*"
              --html --html-details ${COVERAGE_OUTPUT_DIR}/index.html
      COMMAND ${GCOVR_EXECUTABLE}
              --root ${CMAKE_SOURCE_DIR}
              --object-directory ${CMAKE_BINARY_DIR}
              --exclude "${CMAKE_SOURCE_DIR}/tests/functional.*"
              --exclude "${CMAKE_SOURCE_DIR}/build/.*"
              --txt ${COVERAGE_OUTPUT_DIR}/summary.txt
      COMMENT "Generating gcovr coverage report"
      VERBATIM)
  else()
    message(WARNING "gcovr was not found; coverage report target will be unavailable")
  endif()
endif()
