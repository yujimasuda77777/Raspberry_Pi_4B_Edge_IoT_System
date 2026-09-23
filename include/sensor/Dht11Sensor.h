#ifndef DHT11_SENSOR_H
#define DHT11_SENSOR_H

/**
 * @file Dht11Sensor.h
 * @brief DHT11温湿度センサクラスの定義
 */

#include <cstdint>

/**
 * @class Dht11Sensor
 * @brief DHT11温湿度センサを制御するクラス
 */
class Dht11Sensor
{
public:
    /**
     * @brief コンストラクタ
     * @param gpioPin BCM GPIO番号
     */
    explicit Dht11Sensor(unsigned int gpioPin);

    /**
     * @brief デストラクタ
     */
    ~Dht11Sensor();

    /**
     * @brief DHT11を初期化する
     * @return true: 成功 / false: 失敗
     */
    bool initialize();

    /**
     * @brief DHT11から温湿度を取得する
     * @param temperature 温度
     * @param humidity 湿度
     * @return true: 成功 / false: 失敗
     */
    bool read(float& temperature, float& humidity);

private:
    /**
     * @brief GPIOを出力として確保する
     * @param initialValue 初期値
     * @return true: 成功 / false: 失敗
     */
    bool claimOutput(int initialValue);

    /**
     * @brief GPIOを入力＋エッジ検出として確保する
     * @return true: 成功 / false: 失敗
     */
    bool claimAlert();

    /**
     * @brief GPIOを解放する
     */
    void releaseGpio();

    /**
     * @brief DHT11開始信号を送信する
     * @return true: 成功 / false: 失敗
     */
    bool sendStartSignal();

    /**
     * @brief DHT11の40bitデータを取得する
     * @param data 取得データ
     * @return true: 成功 / false: 失敗
     */
    bool readRawData(std::uint8_t data[5]);

    /**
     * @brief チェックサムを確認する
     * @param data DHT11データ
     * @return true: 正常 / false: 異常
     */
    bool checkChecksum(const std::uint8_t data[5]) const;

private:
    /**
     * @brief BCM GPIO番号
     */
    unsigned int m_gpioPin;

    /**
     * @brief lgpio GPIOチップハンドル
     */
    int m_gpioHandle;

    /**
     * @brief GPIO確保状態
     */
    bool m_gpioClaimed;

    /**
     * @brief エッジ検出を有効にしているか
     */
    bool m_alertClaimed;

    /**
     * @brief エッジ通知を受け取るための内部データ
     */
    struct AlertContext;

    AlertContext* m_alertContext;
};

#endif // DHT11_SENSOR_H