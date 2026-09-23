#include "sensor/Dht11Sensor.h"
#include "communication/CloudflareClient.h"

#include <chrono>
#include <iostream>
#include <thread>

int main()
{
    /*
     * DHT11を接続しているGPIO番号
     *
     * Raspberry PiのBCM GPIO14を使用する。
     */
    const unsigned int dht11GpioPin = 14;

    /*
     * Cloudflare WorkerのURL
     */
    const std::string workerUrl =
        "https://raspi-iot.yujimasuda77777.workers.dev/";

    /*
     * DHT11センサを生成する。
     */
    Dht11Sensor sensor(
        dht11GpioPin);

    /*
     * DHT11を初期化する。
     */
    if (!sensor.initialize())
    {
        std::cerr
            << "DHT11 initialize failed."
            << std::endl;

        return 1;
    }

    /*
     * Cloudflare通信クライアントを生成する。
     */
    CloudflareClient cloudClient(
        workerUrl);

    /*
     * Cloudflare通信を初期化する。
     */
    if (!cloudClient.initialize())
    {
        std::cerr
            << "Cloudflare client initialize failed."
            << std::endl;

        return 1;
    }

    std::cout
        << "IoT system started."
        << std::endl;

    /*
     * センサ取得 → Cloudflare送信を繰り返す。
     */
    while (true)
    {
        float temperature = 0.0f;
        float humidity = 0.0f;

        /*
         * DHT11から温度・湿度を取得する。
         */
        if (sensor.read(
                temperature,
                humidity))
        {
            std::cout
                << "Temperature: "
                << temperature
                << " C, Humidity: "
                << humidity
                << " %"
                << std::endl;

            /*
             * 取得した実測値を
             * Cloudflare Workerへ送信する。
             */
            if (cloudClient.sendSensorData(
                    temperature,
                    humidity))
            {
                std::cout
                    << "Sensor data sent successfully."
                    << std::endl;
            }
            else
            {
                std::cerr
                    << "Sensor data send failed."
                    << std::endl;
            }
        }
        else
        {
            /*
             * DHT11取得に失敗した場合は、
             * Cloudflareへ送信しない。
             */
            std::cerr
                << "DHT11 read failed."
                << std::endl;
        }

        /*
         * 次の取得まで3秒待つ。
         */
        std::this_thread::sleep_for(
            std::chrono::seconds(3));
    }

    return 0;
}