#ifndef SENSOR_DATA_H
#define SENSOR_DATA_H

/**
 * @brief センサから取得した温湿度データ
 *
 * Sensor ThreadでDHT11から取得したデータを保持し、
 * Communication Threadへ受け渡すために使用する。
 */
struct SensorData
{
    /**
     * @brief 温度[℃]
     */
    float temperature;

    /**
     * @brief 湿度[%]
     */
    float humidity;
};

#endif