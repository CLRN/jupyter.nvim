cmake_minimum_required(VERSION 3.28)

project(neovim-cpp)

set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 23)

# Boost
include(FetchContent)
FetchContent_Declare(
  Boost
  URL https://github.com/boostorg/boost/releases/download/boost-1.84.0/boost-1.84.0.tar.xz
  URL_MD5 893b5203b862eb9bbd08553e24ff146a
  DOWNLOAD_EXTRACT_TIMESTAMP ON
)
FetchContent_Declare(
  fmt
  GIT_REPOSITORY https://github.com/fmtlib/fmt.git
)
set(BOOST_INCLUDE_LIBRARIES asio cobalt)
FetchContent_MakeAvailable(Boost fmt)

# msgpack
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/third_party/msgpack)

# main library
add_library(${CMAKE_PROJECT_NAME})
target_sources(${CMAKE_PROJECT_NAME} PRIVATE 
  src/nvim.cpp
)
target_include_directories(${CMAKE_PROJECT_NAME} PUBLIC 
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${MSGPACK_SOURCE_DIR}/include
)
target_link_libraries(${CMAKE_PROJECT_NAME} PUBLIC  
  msgpack-cxx 
  Boost::asio 
  Boost::cobalt
  fmt
)
target_compile_options(${CMAKE_PROJECT_NAME} PUBLIC
  -Wno-deprecated-declarations
  -DBOOST_COBALT_NO_PMR
)

set(HEADER_FILES 
  <boost/asio.hpp>
  <boost/cobalt.hpp>
  <msgpack.hpp>
  <fmt/core.h>
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

# test executable
add_executable(test)
target_sources(test PRIVATE src/main.cpp)
target_link_libraries(test ${CMAKE_PROJECT_NAME})

