#include "common/SensorData.h"                 // SensorData構造体を使用する
#include "ipc/SensorDataMessageQueue.h"        // センサーデータ送信用Message Queueを使用する
#include "sensor/Dht11Sensor.h"                // DHT11センサー制御クラスを使用する
#include <chrono>                              // 時間間隔を指定する
#include <csignal>                             // シグナルの登録とシグナル番号を使用する
#include <cstdint>                             // 固定幅の整数型を使用する
#include <ctime>                               // Unix時刻を取得する
#include <iostream>                            // 標準出力・標準エラー出力へログを出力する
#include <thread>                              // スレッドを一定時間停止する

// DHT11を接続するBCM GPIO番号
constexpr unsigned int DHT11_GPIO = 14;

// センサーデータの取得周期（秒）
constexpr int SENSOR_INTERVAL_SECONDS = 5;

// センサーデータ送信に使用するMessage Queue名
const std::string SENSOR_DATA_QUEUE_NAME =
    "/raspberry_pi_edge_iot_sensor_data";

// DHT11データの取得を再試行する回数
constexpr int SENSOR_READ_RETRY_COUNT = 3;

// シャットダウン要求の有無を保持するフラグ
volatile std::sig_atomic_t g_shutdownRequested = 0;

/**
 * @brief Signal Handler
 */
void signalHandler(int signalNumber)
{
    // SIGINTまたはSIGTERMを受信したら終了要求を設定する
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
    // 現在のUnix時刻を秒単位で取得して返す
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
    // センサー読み取りを規定回数まで繰り返す
    for (int retryCount = 0;
         retryCount < SENSOR_READ_RETRY_COUNT;
         ++retryCount)
    {
        // センサーから読み取る温度と湿度を初期化する
        float sensorTemperature = 0.0f;
        float sensorHumidity = 0.0f;

        // センサーから温度と湿度を取得する
        if (sensor.read(
                sensorTemperature,
                sensorHumidity))
        {
            // 読み取った値を呼び出し元のdouble型変数へ格納する
            temperature =
                static_cast<double>(sensorTemperature);
            humidity =
                static_cast<double>(sensorHumidity);

            return true;
        }

        // 読み取り失敗回数を出力する
        std::cerr
            << "DHT11 read failed. "
            << "attempt="
            << (retryCount + 1)
            << "/"
            << SENSOR_READ_RETRY_COUNT
            << std::endl;

        // 次の再試行がある場合は500ms待機する
        if (retryCount + 1 <
            SENSOR_READ_RETRY_COUNT)
        {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(500));
        }
    }

    // すべての再試行に失敗した場合はfalseを返す
    return false;
}

/**
 * @brief Sensor Processメイン処理
 */
int main()
{
    // Ctrl+CによるSIGINTを受信するハンドラーを登録する
    std::signal(
        SIGINT,
        signalHandler);

    // SIGTERMを受信するハンドラーを登録する
    std::signal(
        SIGTERM,
        signalHandler);

    std::cout
        << "Sensor Process started."
        << std::endl;

    // GPIO14を使用するDHT11センサーオブジェクトを生成する
    Dht11Sensor sensor(DHT11_GPIO);

    // DHT11センサーを初期化する
    if (!sensor.initialize())
    {
        std::cerr
            << "DHT11 initialization failed."
            << std::endl;

        return 1;
    }

    // センサーデータ送信用Message Queueオブジェクトを生成する
    SensorDataMessageQueue messageQueue(
        SENSOR_DATA_QUEUE_NAME,
        true);

    // Message Queueを初期化する
    if (!messageQueue.initialize())
    {
        std::cerr
            << "IPC Message Queue initialization failed."
            << std::endl;

        return 1;
    }

    // センサーデータの識別番号を1から開始する
    std::uint64_t dataId = 1;

    std::cout
        << "Sensor Process ready."
        << std::endl;

    // シャットダウン要求が設定されるまでセンサー処理を繰り返す
    while (!g_shutdownRequested)
    {
        // 今回取得する温度と湿度を格納する変数を初期化する
        double temperature = 0.0;
        double humidity = 0.0;

        // DHT11から温度と湿度を取得する
        if (readSensorData(
                sensor,
                temperature,
                humidity))
        {
            // 取得したセンサーデータを格納する構造体を初期化する
            SensorData data{};

            // 温度、湿度、取得時刻、識別番号を設定する
            data.temperature = temperature;
            data.humidity = humidity;
            data.timestamp = getCurrentUnixTime();
            data.data_id = dataId++;

            // 取得したセンサーデータをログに出力する
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

            // センサーデータをMessage Queueへ送信する
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
            // センサーから値を取得できなかったことを出力する
            std::cerr
                << "DHT11 sensor error. "
                << "SensorData was not generated."
                << std::endl;
        }

        // 次回の取得時刻まで1秒ずつ待機する
        for (int second = 0;
             second < SENSOR_INTERVAL_SECONDS;
             ++second)
        {
            // 待機中に終了要求があれば待機を打ち切る
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

    // Message Queueを閉じる
    messageQueue.close();

    std::cout
        << "Sensor Process stopped."
        << std::endl;

    return 0;
}