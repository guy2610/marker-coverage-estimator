cmake_minimum_required(VERSION 3.15)

project(
  MarkerCoverageEstimator
  VERSION 0.3.0
  DESCRIPTION "Detect a 3x3 color marker and estimate its image coverage"
  LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(OpenCV 4 REQUIRED)

add_executable(marker_coverage
  src/main.cpp
  src/detection.cpp
  src/visualization.cpp
  src/color_segmentation.cpp
  src/grid_detector.cpp
  src/coverage.cpp
)

set_target_properties(marker_coverage PROPERTIES
  OUTPUT_NAME "marker-coverage"
)

target_include_directories(marker_coverage PRIVATE
  ${OpenCV_INCLUDE_DIRS}
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(marker_coverage PRIVATE
  ${OpenCV_LIBS}
)

target_compile_definitions(marker_coverage PRIVATE
  MARKER_COVERAGE_VERSION="${PROJECT_VERSION}"
)

if(MSVC)
  target_compile_options(marker_coverage PRIVATE
    /W4
    /permissive-
    /Zc:__cplusplus
  )
else()
  target_compile_options(marker_coverage PRIVATE
    -Wall
    -Wextra
    -Wpedantic
  )
endif()
