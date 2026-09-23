#ifndef DHT11_SENSOR_H
#define DHT11_SENSOR_H

/**
 * @file Dht11Sensor.h
 * @brief DHT11温湿度センサクラスの定義
 */

#include <cstdint>

struct gpiod_chip;
struct gpiod_line_request;

/**
 * @class Dht11Sensor
 * @brief DHT11温湿度センサを制御するクラス
 */
class Dht11Sensor
{
public:
    /**
     * @brief コンストラクタ
     * @param gpioPin 使用するBCM GPIO番号
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
     * @param temperature 温度格納先
     * @param humidity 湿度格納先
     * @return true: 成功 / false: 失敗
     */
    bool read(float& temperature, float& humidity);

private:
    /**
     * @brief GPIOを出力モードに設定する
     * @param initialValue 初期出力値
     * @return true: 成功 / false: 失敗
     */
    bool configureOutput(int initialValue);

    /**
     * @brief GPIOを入力モードに設定する
     * @return true: 成功 / false: 失敗
     */
    bool configureInput();

    /**
     * @brief DHT11へ開始信号を送信する
     * @return true: 成功 / false: 失敗
     */
    bool sendStartSignal();

    /**
     * @brief DHT11から40bitの生データを取得する
     * @param data 5バイトのデータ格納先
     * @return true: 成功 / false: 失敗
     */
    bool readRawData(std::uint8_t data[5]);

    /**
     * @brief チェックサムを確認する
     * @param data DHT11から取得した5バイトデータ
     * @return true: 正常 / false: 異常
     */
    bool checkChecksum(const std::uint8_t data[5]) const;

    /**
     * @brief GPIO要求を解放する
     */
    void releaseRequest();

private:
    /**
     * @brief BCM GPIO番号
     */
    unsigned int m_gpioPin;

    /**
     * @brief GPIOチップ
     */
    gpiod_chip* m_chip;

    /**
     * @brief GPIOライン要求
     */
    gpiod_line_request* m_request;
};

#endif // DHT11_SENSOR_H