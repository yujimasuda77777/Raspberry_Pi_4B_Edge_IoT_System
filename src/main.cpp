#include "sensor/Dht11Sensor.h"

#include <chrono>
#include <iostream>
#include <thread>

/**
 * @brief アプリケーションエントリポイント
 */
int main()
{
    /*
     * DHT11はRaspberry Piの
     * BCM GPIO14へ接続されている。
     */
    Dht11Sensor sensor(14);

    /*
     * センサ初期化
     */
    if (!sensor.initialize())
    {
        std::cerr
            << "DHT11 initialize failed."
            << std::endl;

        return 1;
    }

    /*
     * DHT11の読み取りを繰り返す。
     */
    while (true)
    {
        float temperature = 0.0f;
        float humidity = 0.0f;

        if (sensor.read(temperature, humidity))
        {
            std::cout
                << "Temperature: "
                << temperature
                << " C, Humidity: "
                << humidity
                << " %"
                << std::endl;
        }
        else
        {
            std::cerr
                << "DHT11 read failed. "
                << "Retry after 3 seconds."
                << std::endl;
        }

        /*
         * DHT11は連続して読み取らず、
         * 3秒間隔を空ける。
         */
        std::this_thread::sleep_for(
            std::chrono::seconds(3));
    }

    return 0;
}