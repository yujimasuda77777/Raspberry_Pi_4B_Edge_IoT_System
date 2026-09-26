#ifndef CLOUDFLARE_CLIENT_H
#define CLOUDFLARE_CLIENT_H

#include "common/SensorData.h"

#include <string>

/**
 * @brief Cloudflare WorkerとのHTTP通信を担当するクラス
 */
class CloudflareClient
{
public:
    /**
     * @brief コンストラクタ
     *
     * @param workerUrl Cloudflare Worker URL
     * @param sharedSecret Worker認証用Shared Secret
     */
    CloudflareClient(
        const std::string& workerUrl,
        const std::string& sharedSecret);

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
     * @brief SensorDataをCloudflare Workerへ送信する
     *
     * @param data 送信するSensorData
     *
     * @return true 送信成功
     * @return false 送信失敗
     */
    bool sendSensorData(const SensorData& data);

    /**
     * @brief 最後のHTTPステータスコードを取得する
     */
    long getLastHttpStatusCode() const;

    /**
     * @brief 最後の通信がRetry対象か取得する
     */
    bool isRetryableError() const;

private:
    /**
     * @brief Cloudflare Worker URL
     */
    std::string m_workerUrl;

    /**
     * @brief Shared Secret
     */
    std::string m_sharedSecret;

    /**
     * @brief 初期化済みか
     */
    bool m_initialized;

    /**
     * @brief 最後のHTTPステータスコード
     */
    long m_lastHttpStatusCode;

    /**
     * @brief 最後の通信結果がRetry対象か
     */
    bool m_retryableError;
};

#endif