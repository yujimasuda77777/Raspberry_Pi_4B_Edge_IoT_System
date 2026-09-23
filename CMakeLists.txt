cmake_minimum_required(VERSION 3.16)

project(Raspberry_Pi_4B_Edge_IoT_System)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(Raspberry_Pi_4B_Edge_IoT_System
    src/main.cpp
    src/sensor/Dht11Sensor.cpp
)

target_include_directories(Raspberry_Pi_4B_Edge_IoT_System
    PRIVATE
        include
)