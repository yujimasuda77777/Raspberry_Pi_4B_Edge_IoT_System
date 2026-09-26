#include "communication/CloudflareClient.h"

#include "ipc/SensorDataQueue.h"

#include "sensor/Dht11Sensor.h"

#include "task/CommunicationTask.h"

#include "task/SensorTask.h"

#include <chrono>

#include <csignal>

#include <iostream>

#include <string>

#include <thread>

/**
 * @brief アプリケーション終了要求
 *
 * Ctrl+CによるSIGINTを受け取った場合、
 * このフラグをtrueにしてメインループを終了させる。
 */
volatile std::sig_atomic_t g_shutdownRequested = 0;

/**
 * @brief シグナルハンドラ
 *
 * @param signalNumber 受信したシグナル番号
 */
void signalHandler(int signalNumber)
{
    /*
     * SIGINTを受信した場合は、
     * アプリケーション終了を要求する。
     */
    if (signalNumber == SIGINT)
    {
        g_shutdownRequested = 1;
    }
}

/**
 * @brief アプリケーションのエントリーポイント
 *
 * @return 0 正常終了
 * @return 1 初期化失敗
 */
int main()
{
    /*
     * SIGINT（Ctrl+C）を受信した場合に、
     * signalHandler()を呼び出すよう設定する。
     */
    std::signal(SIGINT, signalHandler);

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

        /*
         * Communication Threadの起動に失敗した場合は、
         * 既に起動しているSensor Threadを停止する。
         */
        sensorTask.stop();

        return 1;
    }

    std::cout
        << "IoT system started."
        << std::endl;

    /*
     * メインThreadは、
     * Ctrl+Cによる終了要求が発生するまで待機する。
     */
    while (g_shutdownRequested == 0)
    {
        std::this_thread::sleep_for(
            std::chrono::seconds(1));
    }

    /*
     * Shutdown開始を表示する。
     */
    std::cout
        << "Shutdown requested."
        << std::endl;

    /*
     * Sensor Threadを停止する。
     *
     * これ以降、新しいセンサデータが
     * Queueへ投入されないようにする。
     */
    sensorTask.stop();

    /*
     * QueueへShutdownを通知する。
     *
     * Communication ThreadがQueue待機中の場合は、
     * condition_variableによって起床する。
     */
    dataQueue.shutdown();

    /*
     * Communication Threadを停止する。
     *
     * QueueからShutdown通知を受け取ることで、
     * waitAndPop()から抜けてThreadが終了する。
     */
    communicationTask.stop();

    /*
     * 全Threadの終了が完了した。
     */
    std::cout
        << "IoT system stopped."
        << std::endl;

    return 0;
}