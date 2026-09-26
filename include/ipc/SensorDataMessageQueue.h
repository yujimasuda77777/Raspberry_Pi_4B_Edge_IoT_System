#ifndef SENSOR_DATA_MESSAGE_QUEUE_H
#define SENSOR_DATA_MESSAGE_QUEUE_H

#include "common/SensorData.h"

#include <cstddef>
#include <mqueue.h>
#include <string>

/**
 * @brief SensorData用Linux Message Queueを管理するクラス
 *
 * Sensor ProcessとCommunication Processの
 * Process間通信にPOSIX Message Queueを使用する。
 */
class SensorDataMessageQueue
{
public:
    /**
     * @brief コンストラクタ
     *
     * @param queueName Message Queue名
     * @param createQueue Queueが存在しない場合に作成するか
     */
    SensorDataMessageQueue(
        const std::string& queueName,
        bool createQueue);

    /**
     * @brief デストラクタ
     */
    ~SensorDataMessageQueue();

    /**
     * @brief Message Queueを初期化する
     *
     * @return true 初期化成功
     * @return false 初期化失敗
     */
    bool initialize();

    /**
     * @brief SensorDataをQueueへ送信する
     *
     * Queue Fullの場合は待機せず失敗する。
     *
     * @param data 送信するSensorData
     *
     * @return true 送信成功
     * @return false 送信失敗
     */
    bool send(const SensorData& data);

    /**
     * @brief QueueからSensorDataを受信する
     *
     * データが到着するまで待機する。
     *
     * @param data 受信したSensorData
     *
     * @return true 受信成功
     * @return false 受信失敗
     */
    bool receive(SensorData& data);

    /**
     * @brief Message Queueを閉じる
     */
    void close();

    /**
     * @brief Message Queueを削除する
     *
     * Process終了時のリソース解放に使用する。
     */
    bool unlinkQueue();

private:
    /**
     * @brief Message Queueを作成またはオープンする
     */
    bool openQueue();

    /**
     * @brief Message Queue名
     */
    std::string m_queueName;

    /**
     * @brief Queueを作成するか
     */
    bool m_createQueue;

    /**
     * @brief Message Queue descriptor
     */
    mqd_t m_queueDescriptor;

    /**
     * @brief Queueがオープン済みか
     */
    bool m_opened;
};

#endif