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
 * @return 1: 初期化異常
 */
int main()
{
    /*
     * DHT11は既存のPython動作確認と同じ
     * BCM GPIO14を使用する。
     */
    constexpr int DHT11_GPIO = 14;

    /*
     * DHT11読み取り間隔。
     *
     * DHT11は連続して読み取らず、
     * 約1秒以上の間隔を空ける。
     */
    constexpr int READ_INTERVAL_MS = 3000;

    Dht11Sensor sensor(DHT11_GPIO);

    /*
     * センサ初期化。
     *
     * GPIOを取得できない場合は、
     * リトライしても同じGPIOを取得できないため、
     * ここでは終了する。
     */
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
     * DHT11読み取りを無限に繰り返す。
     *
     * 成功するまでリトライする。
     */
    int retryCount = 0;

    while (true)
    {
        ++retryCount;

        std::cout
            << "DHT11 read attempt "
            << retryCount
            << std::endl;

        /*
         * DHT11から温度・湿度を取得する。
         */
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

            /*
             * 現時点では1回成功したら終了。
             *
             * 今後、IoTシステムとして連続監視する場合は
             * ここを終了せず、そのまま次の読み取りへ進める。
             */
            return 0;
        }

        /*
         * 読み取り失敗。
         */
        std::cerr
            << "DHT11 read failed."
            << std::endl;

        /*
         * 次回読み取りまで1秒待つ。
         */
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