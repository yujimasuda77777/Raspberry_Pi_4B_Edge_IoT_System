#ifndef SENSOR_DATA_QUEUE_H
#define SENSOR_DATA_QUEUE_H

#include "common/SensorData.h"

#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <queue>


class SensorDataQueue
{

public:
    explicit SensorDataQueue(std::sizet maxSize)
        : m_maxSize(maxSize)
    {

    }

    ~SensorDataQueue() = default;

    bool push(const common::SensorData& data);


    bool waitAndPop(common::SensorData& data);

    std::size_t sise() const;

private:

    std:queue<common::SensorData> m_queue;

    mutable std::mutex m_mutex;

    std::condtion_variable m_confitionVariable;

    std::size_t m_maxsize;

};

#endif