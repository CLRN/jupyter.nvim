cmake_minimum_required(VERSION 3.28)

project(neovim_client_cpp)

set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 23)

include_directories(
    include
    src
    gen
    ${MSGPACK_SOURCE_DIR}/include
)

# add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/third_party/boost/asio)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/third_party/msgpack)

add_executable(test)

target_sources(test
  PRIVATE
    src/main.cpp
  PUBLIC
  FILE_SET CXX_MODULES FILES
)

target_link_libraries(test msgpack-cxx)

find_package(Python3)
if (Python3_Interpreter_FOUND)
    add_custom_target(auto_gen_api
        COMMAND python3 ${CMAKE_SOURCE_DIR}/gen_api.py
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Autogen nvim interface"
    )
endif()

add_dependencies(test auto_gen_api)
