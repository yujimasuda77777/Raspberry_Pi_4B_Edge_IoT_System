#ifndef DHT11_SENSOR_H
#define DHT11_SENSOR_H

#include <cstdint>

/**
 * @brief DHT11温湿度センサを制御するクラス
 */
class Dht11Sensor
{
public:
    /**
     * @brief コンストラクタ
     *
     * @param gpioPin BCM GPIO番号
     */
    explicit Dht11Sensor(unsigned int gpioPin);

    /**
     * @brief デストラクタ
     */
    ~Dht11Sensor();

    /**
     * @brief DHT11を初期化する
     *
     * @return true  初期化成功
     * @return false 初期化失敗
     */
    bool initialize();

    /**
     * @brief DHT11から温度・湿度を取得する
     *
     * @param temperature 取得した温度
     * @param humidity    取得した湿度
     *
     * @return true  読み取り成功
     * @return false 読み取り失敗
     */
    bool read(float& temperature, float& humidity);

private:
    /**
     * @brief GPIOを出力モードで確保する
     *
     * @param initialValue 初期出力値
     *
     * @return true  成功
     * @return false 失敗
     */
    bool configureOutput(int initialValue);

    /**
     * @brief DHT11へ開始信号を送信する
     *
     * @return true  成功
     * @return false 失敗
     */
    bool sendStartSignal();

    /**
     * @brief GPIOを解放する
     */
    void releaseGpio();

    /**
     * @brief DHT11のパルスを取得して40ビットのデータを作成する
     *
     * @param data 取得した5バイトのデータ
     *
     * @return true  成功
     * @return false 失敗
     */
    bool readRawData(std::uint8_t data[5]);

    /**
     * @brief チェックサムを確認する
     *
     * @param data DHT11から取得した5バイトのデータ
     *
     * @return true  正常
     * @return false 異常
     */
    bool checkChecksum(const std::uint8_t data[5]) const;

private:
    unsigned int m_gpioPin;

    /**
     * @brief lgpio GPIOチップハンドル
     */
    int m_gpioHandle;

    /**
     * @brief GPIOを確保しているか
     */
    bool m_gpioClaimed;
};

#endif