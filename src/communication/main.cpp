#include "common/SensorData.h"                    // SensorData構造体を使用する
#include "communication/CloudflareClient.h"       // Cloudflare Workerへのデータ送信を行うクラスを使用する
#include "ipc/SensorDataMessageQueue.h"           // IPC Message Queueからセンサーデータを受信するクラスを使用する

#include <chrono>      // Retry待機時間を指定する
#include <csignal>     // SIGINT・SIGTERMの登録とシグナル番号を使用する
#include <cstdlib>     // getenv()で環境変数を取得する
#include <iostream>    // 標準出力・標準エラー出力へログを出力する
#include <string>      // 文字列を格納・操作する
#include <thread>      // Retry時に指定時間だけ処理を待機させる

/**
 * @brief IPC Message Queue名
 */
// Sensor Processと共有するMessage Queueの名前
const std::string SENSOR_DATA_QUEUE_NAME =
    "/raspberry_pi_edge_iot_sensor_data";

/**
 * @brief Worker URL環境変数名
 */
// Cloudflare WorkerのURLを取得する環境変数名
const std::string WORKER_URL_ENV_NAME =
    "CLOUDFLARE_WORKER_URL";

/**
 * @brief Shared Secret環境変数名
 */
// Cloudflare Workerとの認証に使用する秘密情報の環境変数名
const std::string SHARED_SECRET_ENV_NAME =
    "CLOUDFLARE_SHARED_SECRET";

/**
 * @brief Shutdown要求フラグ
 */
// シグナル受信時に終了要求を示すフラグ
volatile std::sig_atomic_t g_shutdownRequested = 0;

/**
 * @brief Signal Handler
 */
void signalHandler(int signalNumber)
{
    // SIGINTまたはSIGTERMを受信した場合は終了要求を設定する
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
    // 指定された名前の環境変数を取得する
    const char* environmentValue =
        std::getenv(name.c_str());

    // 環境変数が未設定または空文字列の場合は失敗とする
    if (environmentValue == nullptr ||
        environmentValue[0] == '\0')
    {
        return false;
    }

    // 取得した環境変数の値を出力引数へ格納する
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
    // 初回送信後に実施する最大Retry回数を設定する
    constexpr int MAX_RETRY_COUNT = 3;

    // 初回送信とRetryを合わせて最大4回送信する
    for (int retryCount = 0;
         retryCount <= MAX_RETRY_COUNT;
         ++retryCount)
    {
        // 終了要求がある場合は送信処理を終了する
        if (g_shutdownRequested)
        {
            return false;
        }

        // ログに出力する送信試行番号を設定する
        const int attemptNumber =
            retryCount + 1;

        // 送信対象のデータIDと試行回数を出力する
        std::cout
            << "Sending SensorData. "
            << "data_id="
            << data.data_id
            << " attempt="
            << attemptNumber
            << "/"
            << (MAX_RETRY_COUNT + 1)
            << std::endl;

        // Cloudflare Workerへセンサーデータを送信する
        if (client.sendSensorData(data))
        {
            // 送信成功時にデータIDを出力する
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
        // エラーがRetry対象でない場合は再送せずに終了する
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
        // 最大Retry回数に達した場合は再送処理を終了する
        if (retryCount >= MAX_RETRY_COUNT)
        {
            break;
        }

        /*
         * 初回失敗後 1秒
         * Retry 1失敗後 2秒
         * Retry 2失敗後 4秒
         */
        // Retry回数に応じて待機時間を1秒、2秒、4秒と設定する
        const int backoffSeconds =
            1 << retryCount;

        // 再送までの待機時間を出力する
        std::cerr
            << "Sensor data send failed. "
            << "Retry after "
            << backoffSeconds
            << " seconds. "
            << "data_id="
            << data.data_id
            << std::endl;

        // 指定した秒数だけ待機する
        for (int second = 0;
             second < backoffSeconds;
             ++second)
        {
            // 待機中に終了要求があれば再送を中止する
            if (g_shutdownRequested)
            {
                return false;
            }

            // 1秒間待機する
            std::this_thread::sleep_for(
                std::chrono::seconds(1));
        }
    }

    // 最大Retry回数を超えて送信に失敗したことを出力する
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
    // Ctrl+CによるSIGINTを受信するハンドラーを登録する
    std::signal(
        SIGINT,
        signalHandler);

    // SIGTERMを受信するハンドラーを登録する
    std::signal(
        SIGTERM,
        signalHandler);

    std::cout
        << "Communication Process started."
        << std::endl;

    // Cloudflare WorkerのURLとShared Secretを格納する変数を用意する
    std::string workerUrl;
    std::string sharedSecret;

    // 環境変数からCloudflare WorkerのURLを取得する
    if (!getEnvironmentValue(
            WORKER_URL_ENV_NAME,
            workerUrl))
    {
        std::cerr
            << "Cloudflare Worker URL is not configured."
            << std::endl;

        return 1;
    }

    // 環境変数からShared Secretを取得する
    if (!getEnvironmentValue(
            SHARED_SECRET_ENV_NAME,
            sharedSecret))
    {
        std::cerr
            << "Cloudflare Shared Secret is not configured."
            << std::endl;

        return 1;
    }

    // センサープロセスと共有するMessage Queueオブジェクトを生成する
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

    // Worker URLとShared Secretを指定してクライアントを生成する
    CloudflareClient cloudflareClient(
        workerUrl,
        sharedSecret);

    // Cloudflareクライアントを初期化する
    if (!cloudflareClient.initialize())
    {
        std::cerr
            << "Cloudflare client initialization failed."
            << std::endl;

        // 初期化失敗時はMessage Queueを閉じる
        messageQueue.close();

        return 1;
    }

    std::cout
        << "Communication Process ready."
        << std::endl;

    // 終了要求が設定されるまで受信・送信処理を繰り返す
    while (!g_shutdownRequested)
    {
        // Queueから受信するセンサーデータを格納する変数を初期化する
        SensorData data{};

        // Message Queueからセンサーデータを受信する
        if (!messageQueue.receive(data))
        {
            // 終了要求が設定されている場合はループを終了する
            if (g_shutdownRequested)
            {
                break;
            }

            // 受信に失敗したことを出力する
            std::cerr
                << "IPC Message Queue receive failed."
                << std::endl;

            // 次の受信処理へ進む
            continue;
        }

        // 受信後に終了要求が設定されていれば送信せずに終了する
        if (g_shutdownRequested)
        {
            break;
        }

        // 受信したセンサーデータをRetry付きでCloudflareへ送信する
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
    // Message Queueを閉じる（Queue自体は削除しない）
    messageQueue.close();

    std::cout
        << "Communication Process stopped."
        << std::endl;

    return 0;
}