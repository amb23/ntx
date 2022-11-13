cmake_minimum_required(VERSION 3.20.0)

project(NTX VERSION 1.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)
find_package(Boost 1.74.0 COMPONENTS program_options)

set(CMAKE_BUILD_TYPE Debug)

if (Boost_FOUND)
    include_directories(${Boost_INCLUDE_DIRS})
    #add_executable(ntx main.cpp blox.cpp detextion.cpp parsers.cpp)
    add_executable(ntx main.cpp)
    target_link_libraries(ntx ${Boost_LIBRARIES})
else()
    message(FATAL_ERROR "failed to find boost")
endif()
