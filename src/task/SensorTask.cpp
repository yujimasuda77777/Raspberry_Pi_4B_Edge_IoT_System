#include "task/SensorTask.h"

#include <chrono>
#include <iostream>
#include <thread>

/**
 * @brief コンストラクタ
 *
 * @param sensor DHT11センサ
 * @param dataQueue Thread間データ受け渡し用Queue
 */
SensorTask::SensorTask(
    Dht11Sensor& sensor,
    SensorDataQueue& dataQueue)
    : m_sensor(sensor),
      m_dataQueue(dataQueue),
      m_stopRequested(false),
      m_running(false)
{
}

/**
 * @brief デストラクタ
 */
SensorTask::~SensorTask()
{
    stop();
}

/**
 * @brief Sensor Threadを開始する
 *
 * @return true 開始成功
 * @return false 開始失敗
 */
bool SensorTask::start()
{
    if (m_running)
    {
        std::cerr
            << "Sensor Thread is already running."
            << std::endl;

        return false;
    }

    m_stopRequested = false;

    m_thread = std::thread(
        &SensorTask::run,
        this);

    return true;
}

/**
 * @brief Sensor Threadを停止する
 */
void SensorTask::stop()
{
    m_stopRequested = true;

    if (m_thread.joinable())
    {
        m_thread.join();
    }

    m_running = false;
}

/**
 * @brief Sensor Threadが動作中か確認する
 *
 * @return true 動作中
 * @return false 停止中
 */
bool SensorTask::isRunning() const
{
    return m_running;
}

/**
 * @brief Sensor Threadのメイン処理
 */
void SensorTask::run()
{
    m_running = true;

    std::cout
        << "Sensor Thread started."
        << std::endl;

    while (!m_stopRequested)
    {
        float temperature = 0.0f;
        float humidity = 0.0f;

        /*
         * DHT11から温度・湿度を取得する。
         */
        if (m_sensor.read(temperature, humidity))
        {
            std::cout
                << "Temperature: "
                << temperature
                << " C, Humidity: "
                << humidity
                << " %"
                << std::endl;

            SensorData data{};

            data.temperature = temperature;
            data.humidity = humidity;

            /*
             * 取得したデータをQueueへ投入する。
             */
            if (!m_dataQueue.push(data))
            {
                std::cerr
                    << "SensorDataQueue is full. "
                    << "Sensor data was discarded."
                    << std::endl;
            }
        }
        else
        {
            std::cerr
                << "DHT11 read failed."
                << std::endl;
        }

        /*
         * 次のセンサ取得まで3秒待つ。
         */
        std::this_thread::sleep_for(
            std::chrono::seconds(3));
    }

    std::cout
        << "Sensor Thread stopped."
        << std::endl;

    m_running = false;
}