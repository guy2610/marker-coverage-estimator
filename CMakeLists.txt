cmake_minimum_required(VERSION 3.15)
project(MarkerCoverageEstimator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# warnings
if(MSVC)
  add_compile_options(/W4 /permissive- /Zc:__cplusplus)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# OpenCV via vcpkg or system
find_package(OpenCV REQUIRED)

add_executable(MarkerCoverageEstimator
  src/main.cpp
  src/color_segmentation.cpp
  src/grid_detector.cpp
  src/coverage.cpp
)

target_include_directories(MarkerCoverageEstimator PRIVATE
  ${OpenCV_INCLUDE_DIRS}
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(MarkerCoverageEstimator PRIVATE ${OpenCV_LIBS})
