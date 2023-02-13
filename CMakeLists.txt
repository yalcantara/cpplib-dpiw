cmake_minimum_required(VERSION 3.10)
project(main C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include_directories(include)
include_directories(../cpplib-core/include)
include_directories(../odpi/include)

add_executable(main main.cpp ../odpi/embed/dpi.c)

target_link_libraries(main ${CMAKE_DL_LIBS})
