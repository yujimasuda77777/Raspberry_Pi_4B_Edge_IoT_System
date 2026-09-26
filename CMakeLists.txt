cmake_minimum_required(VERSION 3.16)

project(Raspberry_Pi_4B_Edge_IoT_System)

#
# 使用するC++規格
#
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#
# libcurlを検索する。
#
find_package(CURL REQUIRED)

#
# Sensor Process
#
add_executable(
    sensor-process

    src/sensor/main.cpp
    src/sensor/Dht11Sensor.cpp
    src/ipc/SensorDataMessageQueue.cpp
)

#
# Sensor Processのヘッダ検索先
#
target_include_directories(
    sensor-process
    PRIVATE
    include
)

#
# Sensor Processで使用するライブラリ
#
target_link_libraries(
    sensor-process
    PRIVATE
    lgpio
    rt
)

#
# Communication Process
#
add_executable(
    communication-process

    src/communication/main.cpp
    src/communication/CloudflareClient.cpp
    src/ipc/SensorDataMessageQueue.cpp
)

#
# Communication Processのヘッダ検索先
#
target_include_directories(
    communication-process
    PRIVATE
    include
)

#
# Communication Processで使用するライブラリ
#
target_link_libraries(
    communication-process
    PRIVATE
    CURL::libcurl
    rt
)