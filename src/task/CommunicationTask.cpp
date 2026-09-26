#include "task/CommunicationTask.h"

#include <chrono>
#include <iostream>
#include <thread>

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
    if (m_running)
    {
        std::cerr
            << "Communication Thread is already running."
            << std::endl;

        return false;
    }

    m_stopRequested = false;

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
    m_stopRequested = true;

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
    m_running = true;

    std::cout
        << "Communication Thread started."
        << std::endl;

    while (!m_stopRequested)
    {
        SensorData data{};

        /*
         * Queueからセンサデータを取得する。
         *
         * Queueが空の場合は、
         * データ投入またはShutdownまで待機する。
         */
        if (!m_dataQueue.waitAndPop(data))
        {
            break;
        }

        /*
         * Cloudflareへの送信を最大3回試行する。
         */
        constexpr int maxRetryCount = 3;

        bool sendSuccess = false;

        for (int retryCount = 1;
             retryCount <= maxRetryCount;
             ++retryCount)
        {
            if (m_stopRequested)
            {
                break;
            }

            std::cout
                << "Sending sensor data. Attempt "
                << retryCount
                << "/"
                << maxRetryCount
                << "."
                << std::endl;

            if (m_cloudClient.sendSensorData(
                    data.temperature,
                    data.humidity))
            {
                sendSuccess = true;
                break;
            }

            /*
             * 最終試行の場合は、
             * これ以上待機しない。
             */
            if (retryCount == maxRetryCount)
            {
                break;
            }

            /*
             * 指数バックオフで待機する。
             *
             * 1回目失敗 → 2秒
             * 2回目失敗 → 4秒
             */
            const int retryDelaySeconds =
                1 << retryCount;

            std::cerr
                << "Sensor data send failed. "
                << "Retrying after "
                << retryDelaySeconds
                << " seconds."
                << std::endl;

            std::this_thread::sleep_for(
                std::chrono::seconds(
                    retryDelaySeconds));
        }

        if (sendSuccess)
        {
            std::cout
                << "Sensor data sent successfully."
                << std::endl;
        }
        else
        {
            std::cerr
                << "Sensor data send failed after "
                << maxRetryCount
                << " attempts."
                << std::endl;
        }
    }

    std::cout
        << "Communication Thread stopped."
        << std::endl;

    m_running = false;
}