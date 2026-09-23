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
     * @param gpioPin DHT11を接続するGPIO番号（BCM番号）
     */
    explicit Dht11Sensor(unsigned int gpioPin);

    /**
     * @brief デストラクタ
     */
    ~Dht11Sensor();

    /**
     * @brief センサを初期化する
     * @return true: 成功 / false: 失敗
     */
    bool initialize();

    /**
     * @brief 温度・湿度を取得する
     * @param temperature 取得した温度[℃]
     * @param humidity 取得した湿度[%]
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
     * @brief GPIOを入力＋両エッジ検出に設定する
     * @return true: 成功 / false: 失敗
     */
    bool configureInput();

    /**
     * @brief DHT11へ開始信号を送信する
     * @return true: 成功 / false: 失敗
     */
    bool sendStartSignal();

    /**
     * @brief DHT11から40bitのデータを取得する
     * @param data 取得した5バイトのデータ
     * @return true: 成功 / false: 失敗
     */
    bool readRawData(std::uint8_t data[5]);

    /**
     * @brief チェックサムを確認する
     * @param data DHT11から取得した5バイトのデータ
     * @return true: 正常 / false: 異常
     */
    bool checkChecksum(const std::uint8_t data[5]) const;

    /**
     * @brief GPIOリクエストを解放する
     */
    void releaseRequest();

private:
    /** DHT11を接続するGPIO番号（BCM番号） */
    unsigned int m_gpioPin;

    /** GPIOデバイス */
    gpiod_chip* m_chip;

    /** GPIOラインリクエスト */
    gpiod_line_request* m_request;
};

#endif // DHT11_SENSOR_H