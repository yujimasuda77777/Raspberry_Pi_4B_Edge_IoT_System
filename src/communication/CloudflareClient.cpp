#include "communication/CloudflareClient.h"  // CloudflareClientクラスの宣言とメンバ変数を使用する

#include <curl/curl.h>  // HTTP通信、リクエスト設定、レスポンス受信を行うlibcurlを使用する

#include <iomanip>   // JSON出力時の小数点以下の桁数を指定する
#include <iostream>  // 標準出力・標準エラー出力へログを出力する
#include <sstream>   // 数値と文字列を連結してJSON文字列を組み立てる
#include <string>    // URL、認証情報、JSON、レスポンスを文字列として扱う

// Shared Secretを設定するHTTP Header名
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
    // 今回受信したデータの総バイト数を計算する
    const size_t totalSize =
        size * nmemb;

    // 呼び出し側から渡されたレスポンス格納先を取得する
    std::string* response =
        static_cast<std::string*>(userData);

    // 受信データをレスポンス文字列の末尾に追加する
    response->append(
        contents,
        totalSize);

    // 処理したバイト数をlibcurlへ返す
    return totalSize;
}

/**
 * @brief コンストラクタ
 */
CloudflareClient::CloudflareClient(
    const std::string& workerUrl,
    const std::string& sharedSecret)
    // 通信先URL、認証情報、通信状態を初期化する
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
    // 送信先URLが空の場合は初期化を中止する
    if (m_workerUrl.empty())
    {
        std::cerr
            << "Cloudflare Worker URL is empty."
            << std::endl;

        return false;
    }

    // Shared Secretが空の場合は初期化を中止する
    if (m_sharedSecret.empty())
    {
        std::cerr
            << "Cloudflare Shared Secret is empty."
            << std::endl;

        return false;
    }

    // libcurl全体を初期化する
    const CURLcode result =
        curl_global_init(
            CURL_GLOBAL_DEFAULT);

    // 初期化に失敗した場合はエラー内容を出力する
    if (result != CURLE_OK)
    {
        std::cerr
            << "libcurl initialize failed: "
            << curl_easy_strerror(result)
            << std::endl;

        return false;
    }

    // libcurlの初期化成功を記録する
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
    // 前回のHTTPステータスとRetry判定結果をリセットする
    m_lastHttpStatusCode = 0;
    m_retryableError = false;

    // libcurlが初期化されていない場合は送信しない
    if (!m_initialized)
    {
        std::cerr
            << "Cloudflare client is not initialized."
            << std::endl;

        // 初期化されていない状態をRetry対象として記録する
        m_retryableError = true;

        return false;
    }

    // JSON文字列を組み立てるストリームを生成する
    std::ostringstream jsonStream;

    // SensorDataの各メンバをJSON形式の文字列に出力する
    // 温度と湿度は小数点以下1桁で出力する
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

    // 組み立てたJSONを文字列として取得する
    const std::string jsonData =
        jsonStream.str();

    // 送信対象のデータIDをログに出力する
    std::cout
        << "HTTP POST. "
        << "data_id="
        << data.data_id
        << std::endl;

    // HTTPリクエストに使用するCURLオブジェクトを生成する
    CURL* curl =
        curl_easy_init();

    // CURLオブジェクトの生成に失敗した場合は送信を中止する
    if (curl == nullptr)
    {
        std::cerr
            << "curl_easy_init failed."
            << std::endl;

        m_retryableError = true;

        return false;
    }

    // HTTPレスポンス本文の格納先を用意する
    std::string response;

    // HTTP Headerリストを初期化する
    struct curl_slist* headers = nullptr;

    // リクエスト本文がJSON形式であることをHeaderに設定する
    headers = curl_slist_append(
        headers,
        "Content-Type: application/json");

    // Shared Secretを認証用Headerの文字列に連結する
    const std::string authenticationHeader =
        SHARED_SECRET_HEADER_NAME
        + ": "
        + m_sharedSecret;

    // 認証用Headerをリストに追加する
    headers = curl_slist_append(
        headers,
        authenticationHeader.c_str());

    // HTTPリクエストの送信先URLを設定する
    curl_easy_setopt(
        curl,
        CURLOPT_URL,
        m_workerUrl.c_str());

    // HTTPメソッドをPOSTに設定する
    curl_easy_setopt(
        curl,
        CURLOPT_POST,
        1L);

    // POSTリクエストの本文にJSON文字列を設定する
    curl_easy_setopt(
        curl,
        CURLOPT_POSTFIELDS,
        jsonData.c_str());

    // HTTPリクエストに使用するHeaderリストを設定する
    curl_easy_setopt(
        curl,
        CURLOPT_HTTPHEADER,
        headers);

    // レスポンス受信時に実行するCallback関数を設定する
    curl_easy_setopt(
        curl,
        CURLOPT_WRITEFUNCTION,
        writeCallback);

    // Callback関数にレスポンス格納先のアドレスを渡す
    curl_easy_setopt(
        curl,
        CURLOPT_WRITEDATA,
        &response);

    // HTTP通信全体のタイムアウトを5秒に設定する
    curl_easy_setopt(
        curl,
        CURLOPT_TIMEOUT,
        5L);

    // 接続確立までのタイムアウトを5秒に設定する
    curl_easy_setopt(
        curl,
        CURLOPT_CONNECTTIMEOUT,
        5L);

    // HTTPリクエストを実行する
    const CURLcode result =
        curl_easy_perform(curl);

    // 送信結果を失敗として初期化する
    bool success = false;

    // libcurlの通信処理が失敗した場合はエラーを出力する
    if (result != CURLE_OK)
    {
        std::cerr
            << "HTTP communication failed: "
            << curl_easy_strerror(result)
            << std::endl;

        // 通信エラーをRetry対象として記録する
        m_retryableError = true;
    }
    else
    {
        // HTTPレスポンスのステータスコードを取得する
        curl_easy_getinfo(
            curl,
            CURLINFO_RESPONSE_CODE,
            &m_lastHttpStatusCode);

        std::cout
            << "HTTP status="
            << m_lastHttpStatusCode
            << std::endl;

        // HTTPステータスコードが2xxの場合は送信成功とする
        if (m_lastHttpStatusCode >= 200 &&
            m_lastHttpStatusCode < 300)
        {
            success = true;
            m_retryableError = false;
        }
        // HTTPステータスコードが5xxの場合はRetry対象とする
        else if (
            m_lastHttpStatusCode >= 500 &&
            m_lastHttpStatusCode < 600)
        {
            std::cerr
                << "HTTP 5xx received."
                << std::endl;

            m_retryableError = true;
        }
        // 2xx・5xx以外の場合はRetry対象外とする
        else
        {
            std::cerr
                << "HTTP 4xx or other error received."
                << std::endl;

            m_retryableError = false;
        }
    }

    // HTTP Headerリストを解放する
    curl_slist_free_all(headers);

    // CURLオブジェクトを解放する
    curl_easy_cleanup(curl);

    // HTTP送信処理の成否を返す
    return success;
}

/**
 * @brief 最後のHTTPステータスコードを取得する
 */
long CloudflareClient::getLastHttpStatusCode() const
{
    // 保存されているHTTPステータスコードを返す
    return m_lastHttpStatusCode;
}

/**
 * @brief 最後の通信がRetry対象か取得する
 */
bool CloudflareClient::isRetryableError() const
{
    // 保存されているRetry判定結果を返す
    return m_retryableError;
}