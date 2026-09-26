#ifndef CLOUDFLARE_CLIENT_H
#define CLOUDFLARE_CLIENT_H

#include <string>

/**
 * @brief Cloudflare WorkerとのHTTP通信を担当するクラス
 *
 * Raspberry PiからCloudflare Workerへ
 * 温度・湿度データをHTTP POSTで送信する。
 */
class CloudflareClient
{
public:
    /**
     * @brief コンストラクタ
     *
     * @param workerUrl Cloudflare WorkerのURL
     */
    explicit CloudflareClient(const std::string& workerUrl);

    /**
     * @brief デストラクタ
     */
    ~CloudflareClient();

    /**
     * @brief HTTP通信機能を初期化する
     *
     * @return true 初期化成功
     * @return false 初期化失敗
     */
    bool initialize();

    /**
     * @brief 温度・湿度をCloudflare Workerへ送信する
     *
     * @param temperature 温度[℃]
     * @param humidity 湿度[%]
     *
     * @return true 送信成功
     * @return false 送信失敗
     */
    bool sendSensorData(float temperature, float humidity);

private:
    /**
     * @brief Cloudflare WorkerのURL
     */
    std::string m_workerUrl;

    /**
     * @brief HTTP通信機能が初期化済みか
     */
    bool m_initialized;
};

#endif