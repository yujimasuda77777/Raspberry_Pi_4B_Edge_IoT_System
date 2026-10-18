#include "ipc/SensorDataMessageQueue.h"  // SensorDataMessageQueueクラスの宣言を読み込む

#include <cerrno>       // errnoおよびEAGAIN、EINTR、ENOENTなどのエラー番号を使用する
#include <cstring>      // strerror()でエラー番号に対応する文字列を取得する
#include <fcntl.h>      // O_RDWR、O_WRONLY、O_CREAT、O_NONBLOCKなどのフラグを使用する
#include <iostream>     // std::cout、std::cerrでログを出力する
#include <sys/stat.h>   // mode_t型を使用し、Message Queueのアクセス権を指定する

// Message Queueに格納できる最大メッセージ数
constexpr long QUEUE_MAX_MESSAGES = 100;

// Message Queueのアクセス権（所有者・グループに読み書きを許可）
constexpr mode_t QUEUE_PERMISSIONS = 0660;

/**
 * @brief コンストラクタ
 */
SensorDataMessageQueue::SensorDataMessageQueue(
    const std::string& queueName,
    bool createQueue)
    // Queue名、作成有無、未オープン状態をメンバ変数に設定する
    : m_queueName(queueName),
      m_createQueue(createQueue),
      m_queueDescriptor(static_cast<mqd_t>(-1)),
      m_opened(false)
{
}

/**
 * @brief デストラクタ
 */
SensorDataMessageQueue::~SensorDataMessageQueue()
{
    // オブジェクト破棄時にQueueを閉じる
    close();
}

/**
 * @brief Message Queueを初期化する
 */
bool SensorDataMessageQueue::initialize()
{
    // Queueの作成またはオープンを実行する
    return openQueue();
}

/**
 * @brief Message Queueを作成またはオープンする
 */
bool SensorDataMessageQueue::openQueue()
{
    // Queueの最大メッセージ数と1件あたりのサイズを設定する
    struct mq_attr attributes{};

    attributes.mq_flags = 0;
    attributes.mq_maxmsg = QUEUE_MAX_MESSAGES;
    attributes.mq_msgsize = sizeof(SensorData);
    attributes.mq_curmsgs = 0;

    // 読み書き用のアクセスフラグを設定する
    int flags = O_RDWR;

    // 指定されている場合はQueue作成フラグを追加する
    if (m_createQueue)
    {
        flags |= O_CREAT;
    }

    // Queueを指定した属性とアクセス権でオープンする
    m_queueDescriptor = mq_open(
        m_queueName.c_str(),
        flags,
        QUEUE_PERMISSIONS,
        &attributes);

    // オープンに失敗した場合はエラーを出力する
    if (m_queueDescriptor == static_cast<mqd_t>(-1))
    {
        std::cerr
            << "Message Queue open failed. "
            << "name="
            << m_queueName
            << " error="
            << std::strerror(errno)
            << std::endl;

        return false;
    }

    // オープン済み状態に設定する
    m_opened = true;

    std::cout
        << "IPC Message Queue initialized. "
        << "name="
        << m_queueName
        << std::endl;

    return true;
}

/**
 * @brief SensorDataをQueueへ送信する
 */
bool SensorDataMessageQueue::send(const SensorData& data)
{
    // Queueがオープンされていない場合は送信しない
    if (!m_opened)
    {
        std::cerr
            << "Message Queue is not opened."
            << std::endl;

        return false;
    }

    /*
     * Queue Full時にSensor Processを待たせないため、
     * O_NONBLOCKを付けたdescriptorを別途使用する。
     */
    // 送信用の非ブロッキングdescriptorを取得する
    mqd_t nonBlockingDescriptor = mq_open(
        m_queueName.c_str(),
        O_WRONLY | O_NONBLOCK);

    // descriptorの取得に失敗した場合はエラーを出力する
    if (nonBlockingDescriptor == static_cast<mqd_t>(-1))
    {
        std::cerr
            << "Message Queue send open failed. "
            << "error="
            << std::strerror(errno)
            << std::endl;

        return false;
    }

    // SensorDataのメモリ領域をメッセージとしてQueueへ送信する
    const int result = mq_send(
        nonBlockingDescriptor,
        reinterpret_cast<const char*>(&data),
        sizeof(SensorData),
        0);

    // 後続処理でerrnoが変化する前にエラー番号を保存する
    const int savedErrno = errno;

    // 送信処理で使用したdescriptorを閉じる
    mq_close(nonBlockingDescriptor);

    // 送信に失敗した場合はエラーの種類に応じてログを出力する
    if (result == -1)
    {
        // Queueが満杯の場合は送信データを破棄したことを出力する
        if (savedErrno == EAGAIN)
        {
            std::cerr
                << "IPC Message Queue is full. "
                << "SensorData discarded."
                << std::endl;
        }
        else
        {
            std::cerr
                << "Message Queue send failed. "
                << "error="
                << std::strerror(savedErrno)
                << std::endl;
        }

        return false;
    }

    return true;
}

/**
 * @brief QueueからSensorDataを受信する
 */
bool SensorDataMessageQueue::receive(SensorData& data)
{
    // Queueがオープンされていない場合は受信しない
    if (!m_opened)
    {
        std::cerr
            << "Message Queue is not opened."
            << std::endl;

        return false;
    }

    // Queueからメッセージを受信し、SensorDataのメモリ領域に格納する
    const ssize_t receivedSize = mq_receive(
        m_queueDescriptor,
        reinterpret_cast<char*>(&data),
        sizeof(SensorData),
        nullptr);

    // 受信に失敗した場合はエラー番号を確認する
    if (receivedSize == -1)
    {
        // システムコールが割り込まれた場合はエラー出力せずに戻る
        if (errno == EINTR)
        {
            return false;
        }

        std::cerr
            << "Message Queue receive failed. "
            << "error="
            << std::strerror(errno)
            << std::endl;

        return false;
    }

    // 受信したデータサイズがSensorDataのサイズと一致するか確認する
    if (receivedSize != sizeof(SensorData))
    {
        std::cerr
            << "Invalid SensorData size received. "
            << "size="
            << receivedSize
            << std::endl;

        return false;
    }

    return true;
}

/**
 * @brief Message Queueを閉じる
 */
void SensorDataMessageQueue::close()
{
    // 未オープンの場合は何もせずに戻る
    if (!m_opened)
    {
        return;
    }

    // オープン中のQueue descriptorを閉じる
    mq_close(m_queueDescriptor);

    // descriptorを未オープン状態の値に戻す
    m_queueDescriptor = static_cast<mqd_t>(-1);
    m_opened = false;
}

/**
 * @brief Message Queueを削除する
 */
bool SensorDataMessageQueue::unlinkQueue()
{
    // Queue名を指定してMessage Queueを削除する
    if (mq_unlink(m_queueName.c_str()) == -1)
    {
        // 既に削除されている場合は成功として扱う
        if (errno == ENOENT)
        {
            return true;
        }

        std::cerr
            << "Message Queue unlink failed. "
            << "error="
            << std::strerror(errno)
            << std::endl;

        return false;
    }

    return true;
}