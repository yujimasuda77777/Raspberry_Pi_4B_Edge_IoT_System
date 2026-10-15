```cpp
#include "communication/CloudflareClient.h"

#include <curl/curl.h>

#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

/**
 * @brief Shared Secretを送信するHTTP Header名
 *
 * 認証情報をHTTPリクエストのHeaderに設定するために使用する。
 * 実際の秘密情報は、メンバー変数m_sharedSecretに保持されている。
 */
const std::string SHARED_SECRET_HEADER_NAME =
    "X-Edge-IoT-Shared-Secret";

/**
 * @brief HTTPレスポンス受信Callback
 *
 * libcurlがレスポンスデータを受信したときに呼び出す関数。
 *
 * contents : 受信したデータの先頭アドレス
 * size     : 1要素あたりのバイト数
 * nmemb    : 要素数
 * userData : 呼び出し側から渡されたデータへのポインタ
 *
 * 戻り値として処理したバイト数を返す。
 */
static size_t writeCallback(
    char* contents,
    size_t size,
    size_t nmemb,
    void* userData)
{
    // 今回受信したデータの総バイト数を求める。
    const size_t totalSize =
        size * nmemb;

    // void*型をstd::string*型に変換する。
    // 呼び出し側から渡されたレスポンス格納先を取得する。
    std::string* response =
        static_cast<std::string*>(userData);

    // 受信データを文字列の末尾に追加する。
    // 複数回に分かれて受信した場合も、順番に連結できる。
    response->append(
        contents,
        totalSize);

    // 処理したバイト数をlibcurlへ返す。
    return totalSize;
}

/**
 * @brief コンストラクタ
 *
 * 通信先URLと認証情報を保持し、各メンバー変数を初期化する。
 */
CloudflareClient::CloudflareClient(
    const std::string& workerUrl,
    const std::string& sharedSecret)
    // 初期化リストでメンバー変数の初期値を設定する。
    // const std::string&は、文字列を参照で受け取る形式。
    : m_workerUrl(workerUrl),
      m_sharedSecret(sharedSecret),
      m_initialized(false),         // libcurlの初期化状態
      m_lastHttpStatusCode(0),     // 最後に取得したHTTPステータス
      m_retryableError(false)      // Retry対象エラーの有無
{
}

/**
 * @brief デストラクタ
 *
 * オブジェクトが破棄されるときに呼び出される。
 * 現在の実装では、特別な処理は行わない。
 */
CloudflareClient::~CloudflareClient()
{
}

/**
 * @brief HTTP通信機能を初期化する
 *
 * URLと認証情報を確認した後、libcurlを初期化する。
 * 初期化に成功した場合はtrue、失敗した場合はfalseを返す。
 */
bool CloudflareClient::initialize()
{
    // 送信先URLが空の場合は通信できないため、初期化を中止する。
    if (m_workerUrl.empty())
    {
        std::cerr
            << "Cloudflare Worker URL is empty."
            << std::endl;

        return false;
    }

    // 認証情報が空の場合も、初期化を中止する。
    if (m_sharedSecret.empty())
    {
        std::cerr
            << "Cloudflare Shared Secret is empty."
            << std::endl;

        return false;
    }

    // libcurl全体の初期化を行う。
    // CURLcodeは、libcurlの処理結果を表す型。
    const CURLcode result =
        curl_global_init(
            CURL_GLOBAL_DEFAULT);

    // CURLE_OK以外なら初期化に失敗している。
    if (result != CURLE_OK)
    {
        // curl_easy_strerror()でエラーコードを文字列に変換する。
        std::cerr
            << "libcurl initialize failed: "
            << curl_easy_strerror(result)
            << std::endl;

        return false;
    }

    // 初期化成功を記録する。
    // sendSensorData()で、このフラグを確認する。
    m_initialized = true;

    std::cout
        << "Cloudflare client initialized."
        << std::endl;

    return true;
}

/**
 * @brief SensorDataをCloudflare Workerへ送信する
 *
 * dataは送信対象のSensorDataへのconst参照。
 * constにより、この関数内で元データを書き換えない。
 *
 * 戻り値:
 *   true  : HTTPステータスコードが2xx
 *   false : 通信失敗、または2xx以外
 *
 * Retryの要否はm_retryableErrorにも記録する。
 */
bool CloudflareClient::sendSensorData(
    const SensorData& data)
{
    // 今回の送信結果を記録するため、前回の結果をリセットする。
    m_lastHttpStatusCode = 0;
    m_retryableError = false;

    // initialize()が成功していなければ、通信を開始しない。
    if (!m_initialized)
    {
        std::cerr
            << "Cloudflare client is not initialized."
            << std::endl;

        // 初期化不足による送信失敗をRetry対象として記録する。
        m_retryableError = true;

        return false;
    }

    // JSON文字列を組み立てるためのストリームを生成する。
    // ostringstreamは、数値や文字列を順番に書き込めるオブジェクト。
    std::ostringstream jsonStream;

    // operator<<で各値を順番に出力し、JSON文字列を組み立てる。
    // fixedとsetprecision(1)により、小数点以下を1桁で出力する。
    // 例: 25.678 → 25.7
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

    // str()でストリームの内容をstd::stringとして取り出す。
    // この文字列をHTTPリクエストの本文として送信する。
    const std::string jsonData =
        jsonStream.str();

    // どのデータを送信するか、識別用のログを出力する。
    std::cout
        << "HTTP POST. "
        << "data_id="
        << data.data_id
        << std::endl;

    // 今回のHTTPリクエストに使用するCURLオブジェクトを生成する。
    CURL* curl =
        curl_easy_init();

    // オブジェクトを生成できなければ送信できない。
    if (curl == nullptr)
    {
        std::cerr
            << "curl_easy_init failed."
            << std::endl;

        m_retryableError = true;

        return false;
    }

    // HTTPレスポンス本文を格納する文字列。
    // writeCallback()から、この文字列にデータを追加する。
    std::string response;

    // HTTP Headerを管理するリスト。
    // nullptrは、まだHeaderが登録されていない状態を表す。
    struct curl_slist* headers = nullptr;

    // リクエスト本文がJSON形式であることをHeaderで通知する。
    headers = curl_slist_append(
        headers,
        "Content-Type: application/json");

    // Header名と秘密情報を連結して、認証用Headerを作成する。
    // 実際の秘密情報をログへ出力しないこと。
    const std::string authenticationHeader =
        SHARED_SECRET_HEADER_NAME
        + ": "
        + m_sharedSecret;

    // 認証用Headerをリストへ追加する。
    headers = curl_slist_append(
        headers,
        authenticationHeader.c_str());

    // 接続先URLを設定する。
    // c_str()は、std::stringの内容をC形式の文字列として参照する。
    curl_easy_setopt(
        curl,
        CURLOPT_URL,
        m_workerUrl.c_str());

    // HTTP POST方式でリクエストを送信するよう設定する。
    // 1Lはlong型の値1。libcurlのオプション指定に使用する。
    curl_easy_setopt(
        curl,
        CURLOPT_POST,
        1L);

    // POST本文にJSON文字列を設定する。
    curl_easy_setopt(
        curl,
        CURLOPT_POSTFIELDS,
        jsonData.c_str());

    // 作成したHTTP Headerのリストを設定する。
    curl_easy_setopt(
        curl,
        CURLOPT_HTTPHEADER,
        headers);

    // レスポンスを受信したときに呼び出すCallbackを登録する。
    curl_easy_setopt(
        curl,
        CURLOPT_WRITEFUNCTION,
        writeCallback);

    // Callbackへ渡すデータとしてresponseのアドレスを登録する。
    // void*型で渡すが、Callback側でstd::string*型に変換する。
    curl_easy_setopt(
        curl,
        CURLOPT_WRITEDATA,
        &response);

    /*
     * 設計書上のHTTP Timeoutは5秒。
     * 通信処理全体に対するタイムアウト時間を秒単位で設定する。
     */
    curl_easy_setopt(
        curl,
        CURLOPT_TIMEOUT,
        5L);

    // 接続確立までのタイムアウト時間を5秒に設定する。
    // 通信全体のTimeoutとは別の設定。
    curl_easy_setopt(
        curl,
        CURLOPT_CONNECTTIMEOUT,
        5L);

    /*
     * DNS/TCP/TLS/Timeout等を含む
     * libcurlの通信エラーはRetry対象。
     *
     * curl_easy_perform()で、設定したHTTPリクエストを実行する。
     * CURLcodeのresultには、通信処理の結果が格納される。
     */
    const CURLcode result =
        curl_easy_perform(curl);

    // 送信成功かどうかを管理する変数。
    // 最初は失敗としておき、成功条件を満たした場合だけtrueにする。
    bool success = false;

    // libcurl自体が通信エラーを返した場合。
    // 例: DNS解決失敗、接続失敗、タイムアウトなど。
    if (result != CURLE_OK)
    {
        std::cerr
            << "HTTP communication failed: "
            << curl_easy_strerror(result)
            << std::endl;

        // 通信エラーはRetry対象として記録する。
        m_retryableError = true;
    }
    else
    {
        // 通信処理が完了したため、HTTPステータスコードを取得する。
        // 通信成功とHTTP処理成功は別なので、後続でコードを判定する。
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

        // HTTPステータスコードが200～299なら成功と判定する。
        if (m_lastHttpStatusCode >= 200 &&
            m_lastHttpStatusCode < 300)
        {
            success = true;
            m_retryableError = false;
        }
        // HTTP 500番台はサーバー側のエラーとしてRetry対象にする。
        else if (
            m_lastHttpStatusCode >= 500 &&
            m_lastHttpStatusCode < 600)
        {
            std::cerr
                << "HTTP 5xx received."
                << std::endl;

            m_retryableError = true;
        }
        // HTTP 400番台など、上記以外のステータスはRetry対象外とする。
        else
        {
            std::cerr
                << "HTTP 4xx or other error received."
                << std::endl;

            m_retryableError = false;
        }
    }

    // HTTP Headerリストを解放する。
    // curl_slist_append()で確保したリストを後片付けする。
    curl_slist_free_all(headers);

    // CURLオブジェクトを解放する。
    // curl_easy_init()で生成したオブジェクトに対応する後片付け。
    curl_easy_cleanup(curl);

    // 呼び出し元へ送信処理の成否を返す。
    return success;
}

/**
 * @brief 最後のHTTPステータスコードを取得する
 *
 * constは、このメンバー関数がメンバー変数を書き換えないことを示す。
 * 戻り値のlongは、HTTPステータスコードを格納する整数型。
 */
long CloudflareClient::getLastHttpStatusCode() const
{
    return m_lastHttpStatusCode;
}

/**
 * @brief 最後の通信がRetry対象か取得する
 *
 * true  : Retry対象として記録されている
 * false : Retry対象ではない
 *
 * この関数自体は再送信を行わない。
 * Retryするかどうかは、この戻り値を利用する呼び出し側が判断する。
 */
bool CloudflareClient::isRetryableError() const
{
    return m_retryableError;
}
```