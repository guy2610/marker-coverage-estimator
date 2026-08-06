cmake_minimum_required(VERSION 3.16)

project(
  MarkerCoverageEstimator
  VERSION 0.5.0
  DESCRIPTION
    "Detect a 3x3 color marker and estimate its image coverage"
  LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(OpenCV 4 REQUIRED)

function(enable_project_warnings target_name)
  if(MSVC)
    target_compile_options(
      ${target_name}
      PRIVATE
        /W4
        /permissive-
    )
  else()
    target_compile_options(
      ${target_name}
      PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
  endif()
endfunction()

add_library(
  marker_coverage_core
  STATIC
    src/detection.cpp
    src/color_segmentation.cpp
    src/grid_detector.cpp
    src/coverage.cpp
)

target_include_directories(
  marker_coverage_core
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_include_directories(
  marker_coverage_core
  SYSTEM
  PUBLIC
    ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(
  marker_coverage_core
  PUBLIC
    ${OpenCV_LIBS}
)

enable_project_warnings(marker_coverage_core)

add_executable(
  marker_coverage
  src/main.cpp
  src/visualization.cpp
)

set_target_properties(
  marker_coverage
  PROPERTIES
    OUTPUT_NAME "marker-coverage"
)

target_link_libraries(
  marker_coverage
  PRIVATE
    marker_coverage_core
)

target_compile_definitions(
  marker_coverage
  PRIVATE
    MARKER_COVERAGE_VERSION="${PROJECT_VERSION}"
)

enable_project_warnings(marker_coverage)

add_executable(
  marker_coverage_synthetic_generator
  tools/generate_synthetic_dataset.cpp
)

set_target_properties(
  marker_coverage_synthetic_generator
  PROPERTIES
    OUTPUT_NAME "generate-synthetic-dataset"
)

target_include_directories(
  marker_coverage_synthetic_generator
  SYSTEM
  PRIVATE
    ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(
  marker_coverage_synthetic_generator
  PRIVATE
    ${OpenCV_LIBS}
)

enable_project_warnings(
  marker_coverage_synthetic_generator
)

include(CTest)

if(BUILD_TESTING)
  find_package(
    Python3
    REQUIRED
    COMPONENTS Interpreter
  )

  add_executable(
    marker_coverage_unit_tests
    tests/test_core.cpp
  )

  target_link_libraries(
    marker_coverage_unit_tests
    PRIVATE
      marker_coverage_core
  )

  enable_project_warnings(
    marker_coverage_unit_tests
  )

  add_test(
    NAME core_unit_tests
    COMMAND marker_coverage_unit_tests
  )

  add_test(
    NAME cli_regression
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_cli.py
      --executable
      $<TARGET_FILE:marker_coverage>
      --generator
      $<TARGET_FILE:marker_coverage_synthetic_generator>
      --project-root
      ${CMAKE_CURRENT_SOURCE_DIR}
  )

  set_tests_properties(
    cli_regression
    PROPERTIES
      TIMEOUT 60
  )
endif()
