#ifndef COMMUNICATION_TASK_H
#define COMMUNICATION_TASK_H

#include "communication/CloudflareClient.h"
#include "ipc/SensorDataQueue.h"

#include <atomic>
#include <thread>

/**
 * @brief Cloudflare通信Threadを管理するクラス
 *
 * SensorDataQueueからセンサデータを取得し、
 * Cloudflare Workerへ送信する。
 */
class CommunicationTask
{
public:
    /**
     * @brief コンストラクタ
     *
     * @param cloudClient Cloudflare通信クライアント
     * @param dataQueue Thread間データ受け渡し用Queue
     */
    CommunicationTask(
        CloudflareClient& cloudClient,
        SensorDataQueue& dataQueue);

    /**
     * @brief デストラクタ
     */
    ~CommunicationTask();

    /**
     * @brief Communication Threadを開始する
     *
     * @return true 開始成功
     * @return false 開始失敗
     */
    bool start();

    /**
     * @brief Communication Threadを停止する
     */
    void stop();

    /**
     * @brief Communication Threadが動作中か確認する
     *
     * @return true 動作中
     * @return false 停止中
     */
    bool isRunning() const;

private:
    /**
     * @brief Communication Threadのメイン処理
     */
    void run();

private:
    /**
     * @brief Cloudflare通信クライアント
     */
    CloudflareClient& m_cloudClient;

    /**
     * @brief Thread間データ受け渡し用Queue
     */
    SensorDataQueue& m_dataQueue;

    /**
     * @brief Thread停止要求
     */
    std::atomic<bool> m_stopRequested;

    /**
     * @brief Communication Thread
     */
    std::thread m_thread;

    /**
     * @brief Threadが動作中か
     */
    std::atomic<bool> m_running;
};

#endif