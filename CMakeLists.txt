cmake_minimum_required(VERSION 3.28)

project(neovim_client_cpp)

set(CMAKE_CXX_EXTENSIONS OFF)

# find_package(Boost 1.55.0 REQUIRED COMPONENTS system coroutine context thread)

set (CMAKE_CXX_STANDARD 23)
set(MSGPACK_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/msgpack-c)

#add_subdirectory(${MSGPACK_SOURCE_DIR})
set(CMAKE_CXX_FLAGS "-DMSGPACK_USE_BOOST ${CMAKE_CXX_FLAGS}") #TODO: import every flags directly from msgpack-c

include_directories(
    include
    gen
    ${Boost_INCLUDE_DIR}
    ${MSGPACK_SOURCE_DIR}/include
)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/third_party/boost/asio)

add_executable(test)

target_sources(test
  PRIVATE
    src/main.cpp
  PUBLIC
  FILE_SET CXX_MODULES FILES
    src/std.cpp
    src/tracker.cpp
    src/coro.cpp
    src/socket.cpp
    src/asocket.cpp
)

# Link to the library foo
# target_link_libraries(test ${Boost_LIBRARIES})
target_link_libraries(test asio)

# find_package(Python3)
# if (Python3_Interpreter_FOUND)
#     add_custom_target(auto_gen_api
#         COMMAND python3 ${CMAKE_SOURCE_DIR}/gen_api.py
#         WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
#         COMMENT "Autogen nvim interface"
#     )
# endif()

# add_dependencies(test auto_gen_api)
