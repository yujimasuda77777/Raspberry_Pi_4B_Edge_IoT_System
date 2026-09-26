#include "ipc/SensorDataQueue.h"

/**
 * @brief コンストラクタ
 *
 * @param maxSize Queueの最大保持数
 */
SensorDataQueue::SensorDataQueue(std::size_t maxSize)
    : m_maxSize(maxSize)
{
}

/**
 * @brief デストラクタ
 */
SensorDataQueue::~SensorDataQueue()
{
}

/**
 * @brief センサデータをQueueへ投入する
 *
 * @param data 投入するセンサデータ
 *
 * @return true 投入成功
 * @return false Queueが満杯
 */
bool SensorDataQueue::push(const SensorData& data)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    /*
     * Queueが最大サイズに達している場合は、
     * 新しいデータを投入しない。
     */
    if (m_queue.size() >= m_maxSize)
    {
        return false;
    }

    /*
     * Queueへデータを投入する。
     */
    m_queue.push(data);

    /*
     * Queueで待機しているThreadへ、
     * データが入ったことを通知する。
     */
    m_conditionVariable.notify_one();

    return true;
}

/**
 * @brief Queueからセンサデータを取得する
 *
 * Queueが空の場合は、
 * データが投入されるまで待機する。
 *
 * @param data 取得したセンサデータ
 *
 * @return true 取得成功
 * @return false 取得失敗
 */
bool SensorDataQueue::waitAndPop(SensorData& data)
{
    std::unique_lock<std::mutex> lock(m_mutex);

    /*
     * Queueにデータが入るまで待機する。
     *
     * Queueが空の間はCPUを無駄に使用しない。
     */
    m_conditionVariable.wait(
        lock,
        [this]()
        {
            return !m_queue.empty();
        });

    /*
     * Queueの先頭データを取得する。
     */
    data = m_queue.front();

    /*
     * 取得したデータをQueueから削除する。
     */
    m_queue.pop();

    return true;
}

/**
 * @brief 現在のQueue保持数を取得する
 *
 * @return Queueに保持されているデータ数
 */
std::size_t SensorDataQueue::size() const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    return m_queue.size();
}