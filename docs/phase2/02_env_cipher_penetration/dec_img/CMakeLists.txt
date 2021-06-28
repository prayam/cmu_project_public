cmake_minimum_required(VERSION 3.8 FATAL_ERROR)
set(PROJECT_NAME dbDecToJPG)
project(${PROJECT_NAME})

set (CMAKE_CXX_STANDARD 11)

include(FindPkgConfig)
# OpenCV
pkg_check_modules(OPENCV REQUIRED opencv4)

set(CMAKE_CXX_FLAGS  "-Wno-deprecated-declarations")

include_directories(
  ${OPENCV_INCLUDE_DIRS}
)

AUX_SOURCE_DIRECTORY(./src DIR_SRCS)
message("DIR_SRCS = ${DIR_SRCS}")
add_executable(${PROJECT_NAME}
  ${DIR_SRCS}
)

target_link_libraries(${PROJECT_NAME}
  ${OPENCV_LIBRARIES}
)
