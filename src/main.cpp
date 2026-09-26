#include "communication/CloudflareClient.h"
#include "ipc/SensorDataQueue.h"
#include "sensor/Dht11Sensor.h"
#include "task/CommunicationTask.h"
#include "task/SensorTask.h"

#include <chrono>
#include <iostream>
#include <string>
#include <thread>
/**
 * @brief アプリケーションのエントリーポイント
 *
 * @return 0 正常終了
 * @return 1 初期化失敗
 */
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
    Dht11Sensor sensor(dht11GpioPin);

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
    CloudflareClient cloudClient(workerUrl);

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
     * SensorDataをThread間で受け渡すQueueを生成する。
     */
    SensorDataQueue dataQueue(10);

    /*
     * Sensor Taskを生成する。
     */
    SensorTask sensorTask(sensor, dataQueue);

    /*
     * Communication Taskを生成する。
     */
    CommunicationTask communicationTask(
        cloudClient,
        dataQueue);

    /*
     * Sensor Threadを開始する。
     */
    if (!sensorTask.start())
    {
        std::cerr
            << "Sensor Thread start failed."
            << std::endl;

        return 1;
    }

    /*
     * Communication Threadを開始する。
     */
    if (!communicationTask.start())
    {
        std::cerr
            << "Communication Thread start failed."
            << std::endl;

        sensorTask.stop();

        return 1;
    }

    std::cout
        << "IoT system started."
        << std::endl;

    /*
     * 今回は既存システムと同様に、
     * メインThreadを待機させる。
     *
     * 実際の終了処理については、
     * Lifecycle / Shutdown設計に合わせて
     * 今後整理する。
     */
    while (true)
    {
        std::this_thread::sleep_for(
            std::chrono::seconds(1));
    }

    return 0;
}