/**
 * @file main.cpp
 * @brief Raspberry Pi 4B Edge IoT System メイン処理
 */

#include "sensor/Dht11Sensor.h"

#include <chrono>
#include <cstdio>
#include <thread>

namespace
{
/**
 * @brief DHT11を接続しているBCM GPIO
 */
constexpr unsigned int DHT11_GPIO = 14;

/**
 * @brief 読み取り失敗時の再試行間隔
 */
constexpr int RETRY_INTERVAL_SEC = 3;
}

/**
 * @brief アプリケーションエントリポイント
 */
int main()
{
    /*
     * DHT11センサ生成。
     */
    Dht11Sensor sensor(DHT11_GPIO);

    /*
     * 初期化。
     */
    if (!sensor.initialize())
    {
        std::printf(
            "DHT11 initialize failed.\n");

        return 1;
    }

    /*
     * DHT11読み取りを繰り返す。
     */
    while (true)
    {
        float temperature = 0.0F;
        float humidity = 0.0F;

        /*
         * 温湿度取得。
         */
        if (sensor.read(
                temperature,
                humidity))
        {
            std::printf(
                "Temperature: %.1f C, "
                "Humidity: %.1f %%\n",
                temperature,
                humidity);

            return 0;
        }

        /*
         * 読み取り失敗。
         */
        std::printf(
            "DHT11 read failed.\n");

        /*
         * DHT11は連続して読みすぎない。
         */
        std::this_thread::sleep_for(
            std::chrono::seconds(
                RETRY_INTERVAL_SEC));
    }

    return 0;
}