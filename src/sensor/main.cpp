#include "common/SensorData.h"
#include "ipc/SensorDataMessageQueue.h"
#include "sensor/Dht11Sensor.h"

#include <chrono>
#include <csignal>
#include <cstdint>
#include <ctime>
#include <iostream>
#include <thread>

/**
 * @brief DHT11接続GPIO
 */
constexpr unsigned int DHT11_GPIO = 14;

/**
 * @brief センサー取得周期
 */
constexpr int SENSOR_INTERVAL_SECONDS = 5;

/**
 * @brief IPC Message Queue名
 */
const std::string SENSOR_DATA_QUEUE_NAME =
    "/raspberry_pi_edge_iot_sensor_data";

/**
 * @brief DHT11取得Retry回数
 *
 * 一時的な読み取り失敗に対して再取得する。
 */
constexpr int SENSOR_READ_RETRY_COUNT = 3;

/**
 * @brief Shutdown要求フラグ
 */
volatile std::sig_atomic_t g_shutdownRequested = 0;

/**
 * @brief Signal Handler
 */
void signalHandler(int signalNumber)
{
    if (signalNumber == SIGINT ||
        signalNumber == SIGTERM)
    {
        g_shutdownRequested = 1;
    }
}

/**
 * @brief UTC Unix time secondsを取得する
 */
std::int64_t getCurrentUnixTime()
{
    return static_cast<std::int64_t>(
        std::time(nullptr));
}

/**
 * @brief DHT11データを取得する
 */
bool readSensorData(
    Dht11Sensor& sensor,
    double& temperature,
    double& humidity)
{
    for (int retryCount = 0;
         retryCount < SENSOR_READ_RETRY_COUNT;
         ++retryCount)
    {
        float sensorTemperature = 0.0f;
        float sensorHumidity = 0.0f;

        if (sensor.read(
                sensorTemperature,
                sensorHumidity))
        {
            temperature =
                static_cast<double>(sensorTemperature);

            humidity =
                static_cast<double>(sensorHumidity);

            return true;
        }

        std::cerr
            << "DHT11 read failed. "
            << "attempt="
            << (retryCount + 1)
            << "/"
            << SENSOR_READ_RETRY_COUNT
            << std::endl;

        if (retryCount + 1 <
            SENSOR_READ_RETRY_COUNT)
        {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(500));
        }
    }

    return false;
}

/**
 * @brief Sensor Processメイン処理
 */
int main()
{
    std::signal(
        SIGINT,
        signalHandler);

    std::signal(
        SIGTERM,
        signalHandler);

    std::cout
        << "Sensor Process started."
        << std::endl;

    Dht11Sensor sensor(DHT11_GPIO);

    if (!sensor.initialize())
    {
        std::cerr
            << "DHT11 initialization failed."
            << std::endl;

        return 1;
    }

    SensorDataMessageQueue messageQueue(
        SENSOR_DATA_QUEUE_NAME,
        true);

    if (!messageQueue.initialize())
    {
        std::cerr
            << "IPC Message Queue initialization failed."
            << std::endl;

        return 1;
    }

    std::uint64_t dataId = 1;

    std::cout
        << "Sensor Process ready."
        << std::endl;

    while (!g_shutdownRequested)
    {
        double temperature = 0.0;
        double humidity = 0.0;

        if (readSensorData(
                sensor,
                temperature,
                humidity))
        {
            SensorData data{};

            data.temperature = temperature;
            data.humidity = humidity;
            data.timestamp = getCurrentUnixTime();
            data.data_id = dataId++;

            std::cout
                << "SensorData acquired. "
                << "data_id="
                << data.data_id
                << " temperature="
                << data.temperature
                << " humidity="
                << data.humidity
                << " timestamp="
                << data.timestamp
                << std::endl;

            if (!messageQueue.send(data))
            {
                std::cerr
                    << "SensorData IPC send failed. "
                    << "data_id="
                    << data.data_id
                    << std::endl;
            }
        }
        else
        {
            std::cerr
                << "DHT11 sensor error. "
                << "SensorData was not generated."
                << std::endl;
        }

        for (int second = 0;
             second < SENSOR_INTERVAL_SECONDS;
             ++second)
        {
            if (g_shutdownRequested)
            {
                break;
            }

            std::this_thread::sleep_for(
                std::chrono::seconds(1));
        }
    }

    std::cout
        << "Sensor Process stopping."
        << std::endl;

    messageQueue.close();

    std::cout
        << "Sensor Process stopped."
        << std::endl;

    return 0;
}