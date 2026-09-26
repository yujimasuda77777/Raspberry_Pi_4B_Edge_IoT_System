#include "communication/CloudflareClient.h"

#include <curl/curl.h>

#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

/**
 * @brief Shared Secretを送信するHTTP Header名
 */
const std::string SHARED_SECRET_HEADER_NAME =
    "X-Edge-IoT-Shared-Secret";

/**
 * @brief HTTPレスポンス受信Callback
 */
static size_t writeCallback(
    char* contents,
    size_t size,
    size_t nmemb,
    void* userData)
{
    const size_t totalSize =
        size * nmemb;

    std::string* response =
        static_cast<std::string*>(userData);

    response->append(
        contents,
        totalSize);

    return totalSize;
}

/**
 * @brief コンストラクタ
 */
CloudflareClient::CloudflareClient(
    const std::string& workerUrl,
    const std::string& sharedSecret)
    : m_workerUrl(workerUrl),
      m_sharedSecret(sharedSecret),
      m_initialized(false),
      m_lastHttpStatusCode(0),
      m_retryableError(false)
{
}

/**
 * @brief デストラクタ
 */
CloudflareClient::~CloudflareClient()
{
}

/**
 * @brief HTTP通信機能を初期化する
 */
bool CloudflareClient::initialize()
{
    if (m_workerUrl.empty())
    {
        std::cerr
            << "Cloudflare Worker URL is empty."
            << std::endl;

        return false;
    }

    if (m_sharedSecret.empty())
    {
        std::cerr
            << "Cloudflare Shared Secret is empty."
            << std::endl;

        return false;
    }

    const CURLcode result =
        curl_global_init(
            CURL_GLOBAL_DEFAULT);

    if (result != CURLE_OK)
    {
        std::cerr
            << "libcurl initialize failed: "
            << curl_easy_strerror(result)
            << std::endl;

        return false;
    }

    m_initialized = true;

    std::cout
        << "Cloudflare client initialized."
        << std::endl;

    return true;
}

/**
 * @brief SensorDataをCloudflare Workerへ送信する
 */
bool CloudflareClient::sendSensorData(
    const SensorData& data)
{
    m_lastHttpStatusCode = 0;
    m_retryableError = false;

    if (!m_initialized)
    {
        std::cerr
            << "Cloudflare client is not initialized."
            << std::endl;

        m_retryableError = true;

        return false;
    }

    std::ostringstream jsonStream;

    jsonStream
        << std::fixed
        << std::setprecision(1)
        << "{"
        << "\"data_id\":"
        << data.data_id
        << ","
        << "\"temperature\":"
        << data.temperature
        << ","
        << "\"humidity\":"
        << data.humidity
        << ","
        << "\"timestamp\":"
        << data.timestamp
        << "}";

    const std::string jsonData =
        jsonStream.str();

    std::cout
        << "HTTP POST. "
        << "data_id="
        << data.data_id
        << std::endl;

    CURL* curl =
        curl_easy_init();

    if (curl == nullptr)
    {
        std::cerr
            << "curl_easy_init failed."
            << std::endl;

        m_retryableError = true;

        return false;
    }

    std::string response;

    struct curl_slist* headers = nullptr;

    headers = curl_slist_append(
        headers,
        "Content-Type: application/json");

    const std::string authenticationHeader =
        SHARED_SECRET_HEADER_NAME
        + ": "
        + m_sharedSecret;

    headers = curl_slist_append(
        headers,
        authenticationHeader.c_str());

    curl_easy_setopt(
        curl,
        CURLOPT_URL,
        m_workerUrl.c_str());

    curl_easy_setopt(
        curl,
        CURLOPT_POST,
        1L);

    curl_easy_setopt(
        curl,
        CURLOPT_POSTFIELDS,
        jsonData.c_str());

    curl_easy_setopt(
        curl,
        CURLOPT_HTTPHEADER,
        headers);

    curl_easy_setopt(
        curl,
        CURLOPT_WRITEFUNCTION,
        writeCallback);

    curl_easy_setopt(
        curl,
        CURLOPT_WRITEDATA,
        &response);

    /*
     * 設計書上のHTTP Timeoutは5秒。
     */
    curl_easy_setopt(
        curl,
        CURLOPT_TIMEOUT,
        5L);

    curl_easy_setopt(
        curl,
        CURLOPT_CONNECTTIMEOUT,
        5L);

    /*
     * DNS/TCP/TLS/Timeout等を含む
     * libcurlの通信エラーはRetry対象。
     */
    const CURLcode result =
        curl_easy_perform(curl);

    bool success = false;

    if (result != CURLE_OK)
    {
        std::cerr
            << "HTTP communication failed: "
            << curl_easy_strerror(result)
            << std::endl;

        m_retryableError = true;
    }
    else
    {
        curl_easy_getinfo(
            curl,
            CURLINFO_RESPONSE_CODE,
            &m_lastHttpStatusCode);

        std::cout
            << "HTTP status="
            << m_lastHttpStatusCode
            << std::endl;

        /*
         * Shared SecretやResponse内容など、
         * 秘密情報をログへ出力しない。
         */

        if (m_lastHttpStatusCode >= 200 &&
            m_lastHttpStatusCode < 300)
        {
            success = true;
            m_retryableError = false;
        }
        else if (
            m_lastHttpStatusCode >= 500 &&
            m_lastHttpStatusCode < 600)
        {
            std::cerr
                << "HTTP 5xx received."
                << std::endl;

            m_retryableError = true;
        }
        else
        {
            std::cerr
                << "HTTP 4xx or other error received."
                << std::endl;

            m_retryableError = false;
        }
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    return success;
}

/**
 * @brief 最後のHTTPステータスコードを取得する
 */
long CloudflareClient::getLastHttpStatusCode() const
{
    return m_lastHttpStatusCode;
}

/**
 * @brief 最後の通信がRetry対象か取得する
 */
bool CloudflareClient::isRetryableError() const
{
    return m_retryableError;
}