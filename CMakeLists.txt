cmake_minimum_required(VERSION 3.28)

project(neovim_client_cpp)

set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 23)
add_compile_options(
  -Wno-deprecated-declarations
  -DMSGPACK_USE_X3_PARSE
  -DBOOST_COBALT_NO_PMR
)

include(FetchContent)

FetchContent_Declare(
  Boost
  URL https://github.com/boostorg/boost/releases/download/boost-1.84.0/boost-1.84.0.tar.xz
  URL_MD5 893b5203b862eb9bbd08553e24ff146a
  DOWNLOAD_EXTRACT_TIMESTAMP ON
)

set(BOOST_INCLUDE_LIBRARIES asio cobalt)
FetchContent_MakeAvailable(Boost)

# find_package(Boost REQUIRED COMPONENTS asio)

include_directories(
    include
    src
    gen
    ${Boost_INCLUDE_DIRS}
    ${MSGPACK_SOURCE_DIR}/include
)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/third_party/msgpack)

add_executable(test)

target_sources(test
  PRIVATE
    src/main.cpp
  PUBLIC
  FILE_SET CXX_MODULES FILES
)

target_link_libraries(test msgpack-cxx Boost::asio Boost::cobalt)

set(HEADER_FILES 
  <boost/asio/awaitable.hpp>
  <boost/asio/co_spawn.hpp>
  <boost/asio/connect.hpp>
  <boost/asio/detached.hpp>
  <boost/asio/experimental/awaitable_operators.hpp>
  <boost/asio/experimental/coro.hpp>
  <boost/asio/experimental/use_coro.hpp>
  <boost/asio/ip/tcp.hpp>
  <boost/asio/this_coro.hpp>
  <boost/asio/use_awaitable.hpp>
  <boost/cobalt.hpp>
  <boost/process.hpp>
  <iostream>
  <string>
  <vector>
  <msgpack.hpp>
)
target_precompile_headers(test PRIVATE ${HEADER_FILES})

find_package(Python3)
if (Python3_Interpreter_FOUND)
    add_custom_target(auto_gen_api
        COMMAND python3 ${CMAKE_SOURCE_DIR}/gen_api.py
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Autogen nvim interface"
    )
endif()

add_dependencies(test auto_gen_api)
