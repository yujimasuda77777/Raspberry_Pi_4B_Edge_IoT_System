/**
 * @file main.cpp
 * @brief Raspberry Pi 4B Edge IoT System メイン処理
 */

#include "sensor/Dht11Sensor.h"

#include <chrono>
#include <cstdio>
#include <thread>

/**
 * @brief DHT11を接続しているBCM GPIO番号
 */
namespace
{
constexpr unsigned int DHT11_GPIO = 14;

/**
 * @brief DHT11読み取り失敗時の再試行間隔
 */
constexpr int RETRY_INTERVAL_SEC = 3;
}

/**
 * @brief アプリケーションエントリポイント
 * @return 正常終了時0
 */
int main()
{
    /*
     * DHT11センサオブジェクトを生成する。
     */
    Dht11Sensor sensor(DHT11_GPIO);

    /*
     * DHT11を初期化する。
     */
    if (!sensor.initialize())
    {
        std::printf("DHT11 initialize failed.\n");
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
         * DHT11読み取り。
         */
        if (sensor.read(temperature, humidity))
        {
            /*
             * 読み取り成功。
             */
            std::printf(
                "Temperature: %.1f C, "
                "Humidity: %.1f %%\n",
                temperature,
                humidity);

            /*
             * 成功したら終了。
             */
            return 0;
        }

        /*
         * 読み取り失敗。
         */
        std::printf(
            "DHT11 read failed. "
            "Retry after %d seconds.\n",
            RETRY_INTERVAL_SEC);

        /*
         * DHT11の読み取り間隔を空ける。
         */
        std::this_thread::sleep_for(
            std::chrono::seconds(RETRY_INTERVAL_SEC));
    }

    return 0;
}