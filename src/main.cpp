#include "cloud/CloudflareClient.h"

#include <chrono>
#include <iostream>
#include <thread>

int main()
{
    /*
     * Cloudflare WorkerのURL
     */
    const std::string workerUrl =
        "https://raspi-iot.yujimasuda77777.workers.dev/";

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

    /*
     * 今回はDHT11をまだ接続しない。
     *
     * Cloudflare通信確認用として
     * 固定値を使用する。
     */
    const float temperature = 26.0f;
    const float humidity = 62.0f;

    /*
     * Cloudflare Workerへデータを送信する。
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

        return 1;
    }

    return 0;
}