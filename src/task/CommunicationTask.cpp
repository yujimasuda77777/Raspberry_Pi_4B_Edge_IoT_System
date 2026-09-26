#include "task/CommunicationTask.h"

#include <iostream>

/**
 * @brief コンストラクタ
 *
 * @param cloudClient Cloudflare通信クライアント
 * @param dataQueue Thread間データ受け渡し用Queue
 */
CommunicationTask::CommunicationTask(
    CloudflareClient& cloudClient,
    SensorDataQueue& dataQueue)
    : m_cloudClient(cloudClient),
      m_dataQueue(dataQueue),
      m_stopRequested(false),
      m_running(false)
{
}

/**
 * @brief デストラクタ
 */
CommunicationTask::~CommunicationTask()
{
    stop();
}

/**
 * @brief Communication Threadを開始する
 *
 * @return true 開始成功
 * @return false 開始失敗
 */
bool CommunicationTask::start()
{
    /*
     * 既にThreadが動作している場合は、
     * 二重起動しない。
     */
    if (m_running)
    {
        return false;
    }

    /*
     * 停止要求を解除する。
     */
    m_stopRequested = false;

    /*
     * Communication Threadを起動する。
     */
    m_thread = std::thread(
        &CommunicationTask::run,
        this);

    return true;
}

/**
 * @brief Communication Threadを停止する
 */
void CommunicationTask::stop()
{
    /*
     * 停止要求を設定する。
     */
    m_stopRequested = true;

    /*
     * Threadが起動している場合は、
     * Threadの終了を待つ。
     */
    if (m_thread.joinable())
    {
        m_thread.join();
    }

    m_running = false;
}

/**
 * @brief Communication Threadが動作中か確認する
 *
 * @return true 動作中
 * @return false 停止中
 */
bool CommunicationTask::isRunning() const
{
    return m_running;
}

/**
 * @brief Communication Threadのメイン処理
 */
void CommunicationTask::run()
{
    /*
     * Threadが動作中であることを記録する。
     */
    m_running = true;

    std::cout
        << "Communication Thread started."
        << std::endl;

    /*
     * Communication Threadのメインループ。
     */
    while (!m_stopRequested)
    {
        SensorData data{};

        /*
         * Queueからセンサデータを取得する。
         *
         * Queueが空の場合は、
         * データが投入されるか、
         * Shutdownされるまで待機する。
         */
        if (!m_dataQueue.waitAndPop(data))
        {
            /*
             * QueueからShutdown通知を受け取った場合は、
             * Communication Threadを終了する。
             */
            break;
        }

        /*
         * Queueから取得したデータを
         * Cloudflare Workerへ送信する。
         */
        if (m_cloudClient.sendSensorData(
                data.temperature,
                data.humidity))
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

    std::cout
        << "Communication Thread stopped."
        << std::endl;

    /*
     * Thread停止を記録する。
     */
    m_running = false;
}