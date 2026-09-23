/**
 * @file Dht11Sensor.cpp
 * @brief DHT11温湿度センサクラスの実装
 */

#include "sensor/Dht11Sensor.h"

#include <iostream>

/**
 * @brief コンストラクタ
 * @param gpioPin DHT11を接続するGPIO番号（BCM番号）
 */
Dht11Sensor::Dht11Sensor(int gpioPin)
    : m_gpioPin(gpioPin)
{
}

/**
 * @brief デストラクタ
 */
Dht11Sensor::~Dht11Sensor()
{
}

/**
 * @brief センサを初期化する
 * @return true: 成功 / false: 失敗
 */
bool Dht11Sensor::initialize()
{
    std::cout << "DHT11 initialize. GPIO=" << m_gpioPin << std::endl;

    return true;
}

/**
 * @brief 温度・湿度を取得する
 * @param temperature 取得した温度[℃]
 * @param humidity 取得した湿度[%]
 * @return true: 成功 / false: 失敗
 */
bool Dht11Sensor::read(float& temperature, float& humidity)
{
    /*
     * 現時点では動作確認用の仮データを返す。
     *
     * 後のステップで、この部分を実際のDHT11 GPIO通信処理に変更する。
     */
    temperature = 25.0f;
    humidity = 60.0f;

    return true;
}