#include "communication/CloudflareClient.h"

#include <curl/curl.h>

#include <iostream>
#include <sstream>
#include <string>

/**
 * @brief HTTPレスポンスを受け取るコールバック関数
 *
 * libcurlがサーバーから受信したデータを
 * std::stringへ格納する。
 *
 * @param contents 受信データ
 * @param size データサイズ
 * @param nmemb データ数
 * @param userData 格納先
 *
 * @return 処理したデータサイズ
 */
static size_t writeCallback(
    char* contents,
    size_t size,
    size_t nmemb,
    void* userData)
{
    const size_t totalSize = size * nmemb;

    std::string* response =
        static_cast<std::string*>(userData);

    response->append(
        contents,
        totalSize);

    return totalSize;
}

/**
 * @brief コンストラクタ
 *
 * @param workerUrl Cloudflare WorkerのURL
 */
CloudflareClient::CloudflareClient(
    const std::string& workerUrl)
    : m_workerUrl(workerUrl),
      m_initialized(false)
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
 *
 * @return true 初期化成功
 * @return false 初期化失敗
 */
bool CloudflareClient::initialize()
{
    CURLcode result =
        curl_global_init(CURL_GLOBAL_DEFAULT);

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
 * @brief 温度・湿度をCloudflare Workerへ送信する
 *
 * @param temperature 温度[℃]
 * @param humidity 湿度[%]
 *
 * @return true 送信成功
 * @return false 送信失敗
 */
bool CloudflareClient::sendSensorData(
    float temperature,
    float humidity)
{
    if (!m_initialized)
    {
        std::cerr
            << "Cloudflare client is not initialized."
            << std::endl;

        return false;
    }

    /*
     * JSONデータを作成する。
     *
     * 今回はJSONライブラリを追加せず、
     * std::ostringstreamで必要最小限のJSONを作成する。
     */
    std::ostringstream jsonStream;

    jsonStream
        << "{"
        << "\"temperature\":"
        << temperature
        << ","
        << "\"humidity\":"
        << humidity
        << "}";

    const std::string jsonData =
        jsonStream.str();

    std::cout
        << "POST data: "
        << jsonData
        << std::endl;

    CURL* curl = curl_easy_init();

    if (curl == nullptr)
    {
        std::cerr
            << "curl_easy_init failed."
            << std::endl;

        return false;
    }

    std::string response;

    /*
     * HTTPヘッダーを設定する。
     */
    struct curl_slist* headers = nullptr;

    headers = curl_slist_append(
        headers,
        "Content-Type: application/json");

    /*
     * libcurlへ各種設定を行う。
     */
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

    /*
     * Workerから返ってきたレスポンスを
     * writeCallback()で受け取る。
     */
    curl_easy_setopt(
        curl,
        CURLOPT_WRITEFUNCTION,
        writeCallback);

    curl_easy_setopt(
        curl,
        CURLOPT_WRITEDATA,
        &response);

    /*
     * 通信タイムアウトを設定する。
     */
    curl_easy_setopt(
        curl,
        CURLOPT_CONNECTTIMEOUT,
        5L);

    curl_easy_setopt(
        curl,
        CURLOPT_TIMEOUT,
        10L);

    /*
     * HTTP通信を実行する。
     */
    CURLcode result =
        curl_easy_perform(curl);

    bool success = false;

    if (result != CURLE_OK)
    {
        std::cerr
            << "HTTP POST failed: "
            << curl_easy_strerror(result)
            << std::endl;
    }
    else
    {
        long httpStatusCode = 0;

        curl_easy_getinfo(
            curl,
            CURLINFO_RESPONSE_CODE,
            &httpStatusCode);

        std::cout
            << "HTTP status: "
            << httpStatusCode
            << std::endl;

        std::cout
            << "Worker response: "
            << response
            << std::endl;

        /*
         * 2xxをHTTP通信成功とする。
         */
        if (httpStatusCode >= 200 &&
            httpStatusCode < 300)
        {
            success = true;
        }
        else
        {
            std::cerr
                << "Worker returned HTTP error."
                << std::endl;
        }
    }

    /*
     * libcurlで確保したリソースを解放する。
     */
    curl_slist_free_all(headers);

    curl_easy_cleanup(curl);

    return success;
}