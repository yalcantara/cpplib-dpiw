cmake_minimum_required(VERSION 3.10)
project(main C CXX)

set(CMAKE_CXX_STANDARD 17)


include_directories(include)
include_directories(../external/odpi/include)
link_libraries(ociei)
link_directories(../external/instantclient_19_6)
add_executable(main main.cpp ../external/odpi/embed/dpi.c)



#Uncomment the following line if dl lib is needed
target_link_libraries(main ${CMAKE_DL_LIBS})



find_package(Boost REQUIRED COMPONENTS system)
find_package(OpenSSL 1.0.0 REQUIRED)


target_link_libraries(main
        cpprest
        -lstdc++fs
        Boost::boost
        Boost::system
        OpenSSL::SSL)
