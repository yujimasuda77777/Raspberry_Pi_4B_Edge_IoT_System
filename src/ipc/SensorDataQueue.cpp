#include "ipc/SensorDataQueue.h"

SensorDataQueue::SenSorDataQueue(std::size_t maxsize)
    : m_maxSize(maxSize)
{

}

SensorDataQueue::~SensorDataQueue()
{
}

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

    data = m_queue.front();

    m_queue.pop();

    return true;

}


std::size_t SensorDataQueue::size() const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    return m_queue.size();
}
