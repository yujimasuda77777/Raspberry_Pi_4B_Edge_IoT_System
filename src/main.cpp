/**
 * @file main.cpp
 * @brief Raspberry Pi 4B Edge IoT System メイン処理
 */

#include <iostream>

#include "sensor/Dht11Sensor.h"

/**
 * @brief メイン関数
 * @return 0: 正常終了
 */
int main()
{
    /*
     * DHT11は既存のPython動作確認と同じ
     * BCM GPIO14を使用する。
     */
    constexpr int DHT11_GPIO = 14;

    Dht11Sensor sensor(DHT11_GPIO);

    if (!sensor.initialize())
    {
        std::cerr << "DHT11 initialize failed." << std::endl;
        return 1;
    }

    float temperature = 0.0f;
    float humidity = 0.0f;

    if (!sensor.read(temperature, humidity))
    {
        std::cerr << "DHT11 read failed." << std::endl;
        return 1;
    }

    std::cout << "Temperature: "
              << temperature
              << " C"
              << std::endl;

    std::cout << "Humidity: "
              << humidity
              << " %"
              << std::endl;

    return 0;
}