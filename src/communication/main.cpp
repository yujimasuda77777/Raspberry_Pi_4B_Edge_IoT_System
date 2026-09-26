#include "common/SensorData.h"
#include "communication/CloudflareClient.h"
#include "ipc/SensorDataMessageQueue.h"

#include <chrono>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

/**
 * @brief IPC Message Queue名
 */
const std::string SENSOR_DATA_QUEUE_NAME =
    "/raspberry_pi_edge_iot_sensor_data";

/**
 * @brief Worker URL環境変数名
 */
const std::string WORKER_URL_ENV_NAME =
    "CLOUDFLARE_WORKER_URL";

/**
 * @brief Shared Secret環境変数名
 */
const std::string SHARED_SECRET_ENV_NAME =
    "CLOUDFLARE_SHARED_SECRET";

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
 * @brief 環境変数を取得する
 */
bool getEnvironmentValue(
    const std::string& name,
    std::string& value)
{
    const char* environmentValue =
        std::getenv(name.c_str());

    if (environmentValue == nullptr ||
        environmentValue[0] == '\0')
    {
        return false;
    }

    value = environmentValue;

    return true;
}

/**
 * @brief SensorDataをRetry付きで送信する
 */
bool sendWithRetry(
    CloudflareClient& client,
    const SensorData& data)
{
    constexpr int MAX_RETRY_COUNT = 3;

    for (int retryCount = 0;
         retryCount <= MAX_RETRY_COUNT;
         ++retryCount)
    {
        if (g_shutdownRequested)
        {
            return false;
        }

        const int attemptNumber =
            retryCount + 1;

        std::cout
            << "Sending SensorData. "
            << "data_id="
            << data.data_id
            << " attempt="
            << attemptNumber
            << "/"
            << (MAX_RETRY_COUNT + 1)
            << std::endl;

        if (client.sendSensorData(data))
        {
            std::cout
                << "Sensor data sent successfully. "
                << "data_id="
                << data.data_id
                << std::endl;

            return true;
        }

        /*
         * 4xx等、Retry不要なエラーの場合は
         * ここで終了する。
         */
        if (!client.isRetryableError())
        {
            std::cerr
                << "Sensor data send failed. "
                << "Retry is not required. "
                << "data_id="
                << data.data_id
                << std::endl;

            return false;
        }

        /*
         * MAX_RETRY_COUNT回のRetryを実施済みなら終了。
         */
        if (retryCount >= MAX_RETRY_COUNT)
        {
            break;
        }

        /*
         * 初回失敗後 1秒
         * Retry 1失敗後 2秒
         * Retry 2失敗後 4秒
         */
        const int backoffSeconds =
            1 << retryCount;

        std::cerr
            << "Sensor data send failed. "
            << "Retry after "
            << backoffSeconds
            << " seconds. "
            << "data_id="
            << data.data_id
            << std::endl;

        for (int second = 0;
             second < backoffSeconds;
             ++second)
        {
            if (g_shutdownRequested)
            {
                return false;
            }

            std::this_thread::sleep_for(
                std::chrono::seconds(1));
        }
    }

    std::cerr
        << "Sensor data discarded after "
        << MAX_RETRY_COUNT
        << " retries. "
        << "data_id="
        << data.data_id
        << std::endl;

    return false;
}

/**
 * @brief Communication Processメイン処理
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
        << "Communication Process started."
        << std::endl;

    std::string workerUrl;
    std::string sharedSecret;

    if (!getEnvironmentValue(
            WORKER_URL_ENV_NAME,
            workerUrl))
    {
        std::cerr
            << "Cloudflare Worker URL is not configured."
            << std::endl;

        return 1;
    }

    if (!getEnvironmentValue(
            SHARED_SECRET_ENV_NAME,
            sharedSecret))
    {
        std::cerr
            << "Cloudflare Shared Secret is not configured."
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

    CloudflareClient cloudflareClient(
        workerUrl,
        sharedSecret);

    if (!cloudflareClient.initialize())
    {
        std::cerr
            << "Cloudflare client initialization failed."
            << std::endl;

        messageQueue.close();

        return 1;
    }

    std::cout
        << "Communication Process ready."
        << std::endl;

    while (!g_shutdownRequested)
    {
        SensorData data{};

        if (!messageQueue.receive(data))
        {
            if (g_shutdownRequested)
            {
                break;
            }

            std::cerr
                << "IPC Message Queue receive failed."
                << std::endl;

            continue;
        }

        if (g_shutdownRequested)
        {
            break;
        }

        sendWithRetry(
            cloudflareClient,
            data);
    }

    std::cout
        << "Communication Process stopping."
        << std::endl;

    /*
     * Shutdown時はQueueに残った未送信データを
     * 永続化せず破棄する。
     *
     * Message Queue自体はsystemdによる
     * Processライフサイクルと分離して管理するため、
     * ここではunlinkしない。
     */
    messageQueue.close();

    std::cout
        << "Communication Process stopped."
        << std::endl;

    return 0;
}