#include "ipc/SensorDataMessageQueue.h"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <sys/stat.h>

/**
 * @brief Message Queue最大メッセージ数
 *
 * 設計書の確定値は100件。
 */
constexpr long QUEUE_MAX_MESSAGES = 100;

/**
 * @brief Message Queueのアクセス権
 */
constexpr mode_t QUEUE_PERMISSIONS = 0660;

/**
 * @brief コンストラクタ
 */
SensorDataMessageQueue::SensorDataMessageQueue(
    const std::string& queueName,
    bool createQueue)
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
    close();
}

/**
 * @brief Message Queueを初期化する
 */
bool SensorDataMessageQueue::initialize()
{
    return openQueue();
}

/**
 * @brief Message Queueを作成またはオープンする
 */
bool SensorDataMessageQueue::openQueue()
{
    struct mq_attr attributes{};

    attributes.mq_flags = 0;
    attributes.mq_maxmsg = QUEUE_MAX_MESSAGES;
    attributes.mq_msgsize = sizeof(SensorData);
    attributes.mq_curmsgs = 0;

    int flags = O_RDWR;

    if (m_createQueue)
    {
        flags |= O_CREAT;
    }

    m_queueDescriptor = mq_open(
        m_queueName.c_str(),
        flags,
        QUEUE_PERMISSIONS,
        &attributes);

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
    mqd_t nonBlockingDescriptor = mq_open(
        m_queueName.c_str(),
        O_WRONLY | O_NONBLOCK);

    if (nonBlockingDescriptor == static_cast<mqd_t>(-1))
    {
        std::cerr
            << "Message Queue send open failed. "
            << "error="
            << std::strerror(errno)
            << std::endl;

        return false;
    }

    const int result = mq_send(
        nonBlockingDescriptor,
        reinterpret_cast<const char*>(&data),
        sizeof(SensorData),
        0);

    const int savedErrno = errno;

    mq_close(nonBlockingDescriptor);

    if (result == -1)
    {
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
    if (!m_opened)
    {
        std::cerr
            << "Message Queue is not opened."
            << std::endl;

        return false;
    }

    const ssize_t receivedSize = mq_receive(
        m_queueDescriptor,
        reinterpret_cast<char*>(&data),
        sizeof(SensorData),
        nullptr);

    if (receivedSize == -1)
    {
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
    if (!m_opened)
    {
        return;
    }

    mq_close(m_queueDescriptor);

    m_queueDescriptor = static_cast<mqd_t>(-1);
    m_opened = false;
}

/**
 * @brief Message Queueを削除する
 */
bool SensorDataMessageQueue::unlinkQueue()
{
    if (mq_unlink(m_queueName.c_str()) == -1)
    {
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