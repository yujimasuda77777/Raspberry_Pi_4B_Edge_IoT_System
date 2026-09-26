#ifndef SENSOR_TASK_H
#define SENSOR_TASK_H

#include "ipc/SensorDataQueue.h"
#include "sensor/Dht11Sensor.h"

#include <atomic>
#include <thread>

/**
 * @brief センサ取得Threadを管理するクラス
 *
 * DHT11から温度・湿度を取得し、
 * SensorDataQueueへデータを投入する。
 */
class SensorTask
{
public:
    /**
     * @brief コンストラクタ
     *
     * @param sensor DHT11センサ
     * @param dataQueue Thread間データ受け渡し用Queue
     */
    SensorTask(Dht11Sensor& sensor, SensorDataQueue& dataQueue);

    /**
     * @brief デストラクタ
     */
    ~SensorTask();

    /**
     * @brief Sensor Threadを開始する
     *
     * @return true 開始成功
     * @return false 開始失敗
     */
    bool start();

    /**
     * @brief Sensor Threadを停止する
     */
    void stop();

    /**
     * @brief Sensor Threadが動作中か確認する
     *
     * @return true 動作中
     * @return false 停止中
     */
    bool isRunning() const;

private:
    /**
     * @brief Sensor Threadのメイン処理
     */
    void run();

private:
    /**
     * @brief DHT11センサ
     */
    Dht11Sensor& m_sensor;

    /**
     * @brief Thread間データ受け渡し用Queue
     */
    SensorDataQueue& m_dataQueue;

    /**
     * @brief Thread停止要求
     */
    std::atomic<bool> m_stopRequested;

    /**
     * @brief Sensor Thread
     */
    std::thread m_thread;

    /**
     * @brief Threadが動作中か
     */
    std::atomic<bool> m_running;
};

#endif