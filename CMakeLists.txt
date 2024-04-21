cmake_minimum_required(VERSION 3.28)

project(neovim-cpp)

set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 23)

# 3rd party
include(FetchContent)
FetchContent_Declare(
  Boost
  URL https://github.com/boostorg/boost/releases/download/boost-1.84.0/boost-1.84.0.tar.xz
  URL_MD5 893b5203b862eb9bbd08553e24ff146a
  DOWNLOAD_EXTRACT_TIMESTAMP ON
)
FetchContent_Declare(
  msgpack
  GIT_REPOSITORY https://github.com/msgpack/msgpack-c.git
  GIT_TAG 2c0040cef6e0cd9f871979bb6e6b57fa74de0d46 # cpp_master
)
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
)
FetchContent_Declare(
  fmt
  GIT_REPOSITORY https://github.com/fmtlib/fmt.git
  GIT_TAG 10.2.1
)
FetchContent_Declare(
  spdlog
  GIT_REPOSITORY https://github.com/gabime/spdlog.git
  GIT_TAG v1.13.0
)
FetchContent_Declare(
  range-v3
  GIT_REPOSITORY https://github.com/ericniebler/range-v3.git
)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        release-1.8.0
)

set(BOOST_INCLUDE_LIBRARIES asio cobalt filesystem process variant beast)
set(MSGPACK_CXX20 ON CACHE INTERNAL "")
set(MSGPACK_USE_X3_PARSE ON CACHE INTERNAL "")
set(MSGPACK_USE_STD_VARIANT_ADAPTOR ON CACHE INTERNAL "")
set(SPDLOG_FMT_EXTERNAL ON CACHE INTERNAL "")

FetchContent_MakeAvailable(Boost msgpack nlohmann_json fmt spdlog range-v3 googletest)
find_package(OpenCV REQUIRED COMPONENTS core imgproc imgcodecs)

# main library
add_library(${CMAKE_PROJECT_NAME})
target_sources(${CMAKE_PROJECT_NAME} PRIVATE 
  src/api.cpp
  src/kitty.cpp
  src/graphics.cpp
)
target_include_directories(${CMAKE_PROJECT_NAME} PUBLIC 
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${MSGPACK_SOURCE_DIR}/include
)
target_link_libraries(${CMAKE_PROJECT_NAME} PUBLIC  
  msgpack-cxx 
  nlohmann_json::nlohmann_json
  Boost::asio 
  Boost::beast 
  Boost::cobalt
  Boost::filesystem
  Boost::process
  Boost::variant
  fmt::fmt
  spdlog
  range-v3
  opencv_core 
  opencv_imgproc 
  opencv_imgcodecs
)
target_compile_options(${CMAKE_PROJECT_NAME} PUBLIC
  -Wno-deprecated-declarations
  -DBOOST_COBALT_NO_PMR
)

if(MSVC)
  target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE /W4 /WX)
else()
  target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

set(HEADER_FILES 
  <boost/asio.hpp>
  <boost/cobalt.hpp>
  <boost/process.hpp>
  <msgpack.hpp>
  <fmt/core.h>
  <range/v3/all.hpp>
  <opencv2/opencv.hpp>

  <fstream>
  <iterator>
  <cstdint>
  <iostream>
  <optional>
  <string>
  <unordered_map>
  <utility>
  <algorithm>
  <memory>
  <ranges>
)
target_precompile_headers(${CMAKE_PROJECT_NAME} PUBLIC ${HEADER_FILES})

# main executable
add_executable(main)
target_include_directories(main PUBLIC 
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_sources(main PRIVATE 
  src/main.cpp
  src/window.cpp
  src/handlers/images.cpp
  src/handlers/markdown.cpp
)
target_link_libraries(main ${CMAKE_PROJECT_NAME})

add_executable(test)
target_include_directories(test PUBLIC 
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_sources(test PRIVATE 
  src/api.t.cpp
)
target_link_libraries(test ${CMAKE_PROJECT_NAME} gtest gmock gtest_main)

