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
 */
volatile std::sig_atomic_t g_shutdownRequested = 0;

/**
 * @brief シグナルハンドラ
 *
 * @param signalNumber 受信したシグナル番号
 */
void signalHandler(int signalNumber)
{
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
     * Ctrl+CによるSIGINTを受信する。
     */
    std::signal(SIGINT, signalHandler);

    /*
     * DHT11のBCM GPIO番号
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
            << "ERROR: DHT11 initialize failed."
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
            << "ERROR: Cloudflare client initialize failed."
            << std::endl;

        return 1;
    }

    /*
     * SensorData Queueを生成する。
     *
     * 最大10件保持する。
     */
    SensorDataQueue dataQueue(10);

    /*
     * Sensor Taskを生成する。
     */
    SensorTask sensorTask(
        sensor,
        dataQueue);

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
            << "ERROR: Sensor Thread start failed."
            << std::endl;

        return 1;
    }

    /*
     * Communication Threadを開始する。
     */
    if (!communicationTask.start())
    {
        std::cerr
            << "ERROR: Communication Thread start failed."
            << std::endl;

        /*
         * Sensor Threadを停止する。
         */
        sensorTask.stop();

        return 1;
    }

    std::cout
        << "IoT system started."
        << std::endl;

    /*
     * Ctrl+Cによる終了要求を待つ。
     */
    while (g_shutdownRequested == 0)
    {
        std::this_thread::sleep_for(
            std::chrono::seconds(1));
    }

    std::cout
        << "Shutdown requested."
        << std::endl;

    /*
     * Sensor Threadを停止する。
     */
    sensorTask.stop();

    /*
     * QueueをShutdownする。
     *
     * Communication Threadが待機中なら、
     * notify_all()によって起床する。
     */
    dataQueue.shutdown();

    /*
     * Communication Threadを停止する。
     */
    communicationTask.stop();

    std::cout
        << "IoT system stopped."
        << std::endl;

    return 0;
}