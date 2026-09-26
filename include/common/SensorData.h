#ifndef SENSOR_DATA_H
#define SENSOR_DATA_H

#include <cstdint>

/**
 * @brief センサーデータ
 *
 * Sensor Processで生成し、
 * IPC Message Queueを介して
 * Communication Processへ渡す。
 */
struct SensorData
{
    /**
     * @brief 温度[℃]
     *
     * 小数第1位を有効値とする。
     */
    double temperature;

    /**
     * @brief 湿度[%RH]
     *
     * 小数第1位を有効値とする。
     */
    double humidity;

    /**
     * @brief センサーデータ取得時刻
     *
     * UTC基準のUnix time seconds。
     */
    std::int64_t timestamp;

    /**
     * @brief センサーデータを一意に識別するID
     *
     * Retry時にも同じIDを使用する。
     */
    std::uint64_t data_id;
};

#endif