/**
 * @file main.cpp
 * @brief Raspberry Pi 4B Edge IoT System メイン処理
 */

#include <chrono>
#include <iostream>
#include <thread>

#include "sensor/Dht11Sensor.h"

/**
 * @brief メイン関数
 *
 * @return 0: 正常終了
 * @return 1: 異常終了
 */
int main()
{
    /*
     * DHT11は既存のPython動作確認と同じ
     * BCM GPIO14を使用する。
     */
    constexpr int DHT11_GPIO = 14;

    /*
     * DHT11の最大読み取り試行回数。
     */
    constexpr int MAX_READ_RETRY_COUNT = 5;

    /*
     * DHT11読み取り間隔。
     *
     * DHT11は連続して読み取らず、
     * 約1秒以上の間隔を空ける。
     */
    constexpr int READ_INTERVAL_MS = 1000;

    Dht11Sensor sensor(DHT11_GPIO);

    if (!sensor.initialize())
    {
        std::cerr
            << "DHT11 initialize failed."
            << std::endl;

        return 1;
    }

    float temperature = 0.0f;
    float humidity = 0.0f;

    /*
     * DHT11読み取りを複数回試行する。
     */
    for (int retryCount = 1;
         retryCount <= MAX_READ_RETRY_COUNT;
         ++retryCount)
    {
        std::cout
            << "DHT11 read attempt "
            << retryCount
            << "/"
            << MAX_READ_RETRY_COUNT
            << std::endl;

        if (sensor.read(temperature, humidity))
        {
            /*
             * 読み取り成功。
             */
            std::cout
                << "Temperature: "
                << temperature
                << " C"
                << std::endl;

            std::cout
                << "Humidity: "
                << humidity
                << " %"
                << std::endl;

            return 0;
        }

        /*
         * 今回の読み取りに失敗。
         */
        std::cerr
            << "DHT11 read failed."
            << std::endl;

        /*
         * 次回読み取りまで待つ。
         */
        if (retryCount < MAX_READ_RETRY_COUNT)
        {
            std::cout
                << "Retry after "
                << READ_INTERVAL_MS
                << " ms."
                << std::endl;

            std::this_thread::sleep_for(
                std::chrono::milliseconds(
                    READ_INTERVAL_MS));
        }
    }

    /*
     * 規定回数すべて失敗。
     */
    std::cerr
        << "DHT11 read failed after "
        << MAX_READ_RETRY_COUNT
        << " attempts."
        << std::endl;

    return 1;
}