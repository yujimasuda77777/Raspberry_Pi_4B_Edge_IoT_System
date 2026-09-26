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
# 実行ファイルを作成する。
#
add_executable(
    Raspberry_Pi_4B_Edge_IoT_System

    #
    # メイン処理
    #
    src/main.cpp

    #
    # センサ処理
    #
    src/sensor/Dht11Sensor.cpp

    #
    # Cloudflare通信処理
    #
    src/communication/CloudflareClient.cpp

    #
    # Thread間データ受け渡し
    #
    src/ipc/SensorDataQueue.cpp
)

#
# ヘッダファイルの検索先を設定する。
#
target_include_directories(
    Raspberry_Pi_4B_Edge_IoT_System
    PRIVATE
    include
)

#
# 使用するライブラリをリンクする。
#
target_link_libraries(
    Raspberry_Pi_4B_Edge_IoT_System
    PRIVATE
    lgpio
    CURL::libcurl
)