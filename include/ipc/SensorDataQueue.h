#ifndef SENSOR_DATA_QUEUE_H
#define SENSOR_DATA_QUEUE_H

#include "common/SensorData.h"

#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <queue>

/**
 * @brief センサデータをThread間で受け渡すQueue
 *
 * Sensor ThreadがSensorDataを投入し、
 * Communication ThreadがSensorDataを取得する。
 *
 * Shutdown要求が発生した場合は、
 * 待機中のThreadを起床させて終了できるようにする。
 */
class SensorDataQueue
{
public:
    /**
     * @brief コンストラクタ
     *
     * @param maxSize Queueの最大保持数
     */
    explicit SensorDataQueue(std::size_t maxSize);

    /**
     * @brief デストラクタ
     */
    ~SensorDataQueue();

    /**
     * @brief センサデータをQueueへ投入する
     *
     * @param data 投入するセンサデータ
     *
     * @return true 投入成功
     * @return false Queueが満杯
     */
    bool push(const SensorData& data);

    /**
     * @brief Queueからセンサデータを取得する
     *
     * Queueが空の場合は、
     * データが投入されるかShutdownされるまで待機する。
     *
     * @param data 取得したセンサデータ
     *
     * @return true 取得成功
     * @return false Shutdown要求により取得終了
     */
    bool waitAndPop(SensorData& data);

    /**
     * @brief QueueのShutdownを要求する
     *
     * 待機中のThreadを起床させる。
     */
    void shutdown();

    /**
     * @brief 現在のQueue保持数を取得する
     *
     * @return Queueに保持されているデータ数
     */
    std::size_t size() const;

private:
    /**
     * @brief Queue本体
     */
    std::queue<SensorData> m_queue;

    /**
     * @brief Queueへのアクセスを保護するmutex
     */
    mutable std::mutex m_mutex;

    /**
     * @brief Queueの状態変化を通知するcondition_variable
     */
    std::condition_variable m_conditionVariable;

    /**
     * @brief Queueの最大保持数
     */
    std::size_t m_maxSize;

    /**
     * @brief Shutdown要求状態
     *
     * trueの場合、待機中のThreadを終了させる。
     */
    bool m_shutdown;
};

#endif