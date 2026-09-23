/**
 * @file Dht11Sensor.cpp
 * @brief DHT11温湿度センサクラスの実装
 *
 * OSOYOOのDHT11サンプルの考え方を参考に、
 * Raspberry Pi + libgpiod 2.x + C++で実装する。
 *
 * DHT11は非常に短い時間幅で0/1を表現するため、
 * GPIOのエッジイベントをまとめて取得するのではなく、
 * GPIO状態を繰り返し読み取り、HIGH期間の長さから
 * データビットを判定する。
 */

#include "sensor/Dht11Sensor.h"

#include <gpiod.h>

#include <chrono>
#include <cstdint>
#include <iostream>
#include <thread>

/**
 * @brief GPIO関連定数
 */
namespace
{

/**
 * @brief GPIOチップデバイス
 */
constexpr char GPIO_CHIP[] = "/dev/gpiochip0";

/**
 * @brief DHT11開始信号のLOW時間[ms]
 */
constexpr unsigned int START_LOW_TIME_MS = 18;

/**
 * @brief DHT11開始信号後のHIGH時間[us]
 */
constexpr unsigned int START_HIGH_TIME_US = 40;

/**
 * @brief DHT11データビット数
 */
constexpr int DHT11_BIT_COUNT = 40;

/**
 * @brief DHT11データバイト数
 */
constexpr int DHT11_BYTE_COUNT = 5;

/**
 * @brief GPIO読み取りの最大試行回数
 *
 * DHT11通信では、
 *
 *   LOW
 *   HIGH
 *   LOW
 *   HIGH
 *
 * と状態が変化する。
 *
 * 一定時間状態が変化しない場合は、
 * 通信終了または異常と判断する。
 */
constexpr int MAX_TIMING_COUNT = 255;

/**
 * @brief HIGH期間の0/1判定境界
 *
 * DHT11では、
 *
 *   0：約26～28us
 *   1：約70us
 *
 * となる。
 *
 * OSOYOOのサンプルではカウンタ16を
 * 判定境界として使用しているため、
 * 今回も同じ考え方を採用する。
 */
constexpr int BIT_THRESHOLD = 16;

}

/**
 * @brief コンストラクタ
 *
 * @param gpioPin DHT11を接続するGPIO番号（BCM番号）
 */
Dht11Sensor::Dht11Sensor(unsigned int gpioPin)
    : m_gpioPin(gpioPin),
      m_chip(nullptr),
      m_request(nullptr)
{
}

/**
 * @brief デストラクタ
 */
Dht11Sensor::~Dht11Sensor()
{
    releaseRequest();

    if (m_chip != nullptr)
    {
        gpiod_chip_close(m_chip);
        m_chip = nullptr;
    }
}

/**
 * @brief GPIOリクエストを解放する
 */
void Dht11Sensor::releaseRequest()
{
    if (m_request != nullptr)
    {
        gpiod_line_request_release(m_request);
        m_request = nullptr;
    }
}

/**
 * @brief センサを初期化する
 *
 * @return true: 成功
 * @return false: 失敗
 */
bool Dht11Sensor::initialize()
{
    /*
     * GPIOチップをオープンする。
     */
    m_chip = gpiod_chip_open(GPIO_CHIP);

    if (m_chip == nullptr)
    {
        std::cerr
            << "Failed to open GPIO chip: "
            << GPIO_CHIP
            << std::endl;

        return false;
    }

    /*
     * 最初はHIGH出力状態にする。
     */
    if (!configureOutput(1))
    {
        return false;
    }

    std::cout
        << "DHT11 initialized. GPIO="
        << m_gpioPin
        << std::endl;

    return true;
}

/**
 * @brief GPIOを出力モードに設定する
 *
 * @param initialValue 初期出力値
 *
 * @return true: 成功
 * @return false: 失敗
 */
bool Dht11Sensor::configureOutput(int initialValue)
{
    /*
     * 既存のGPIOリクエストを解放する。
     */
    releaseRequest();

    /*
     * GPIO設定を生成する。
     */
    gpiod_line_settings* settings =
        gpiod_line_settings_new();

    if (settings == nullptr)
    {
        std::cerr
            << "Failed to create GPIO line settings."
            << std::endl;

        return false;
    }

    /*
     * 出力モードに設定する。
     */
    gpiod_line_settings_set_direction(
        settings,
        GPIOD_LINE_DIRECTION_OUTPUT);

    /*
     * プッシュプル出力を使用する。
     */
    gpiod_line_settings_set_drive(
        settings,
        GPIOD_LINE_DRIVE_PUSH_PULL);

    /*
     * 初期出力値を設定する。
     */
    gpiod_line_settings_set_output_value(
        settings,
        initialValue
            ? GPIOD_LINE_VALUE_ACTIVE
            : GPIOD_LINE_VALUE_INACTIVE);

    /*
     * GPIOライン設定を生成する。
     */
    gpiod_line_config* lineConfig =
        gpiod_line_config_new();

    if (lineConfig == nullptr)
    {
        gpiod_line_settings_free(settings);

        return false;
    }

    /*
     * GPIO14を設定する。
     */
    int result =
        gpiod_line_config_add_line_settings(
            lineConfig,
            &m_gpioPin,
            1,
            settings);

    gpiod_line_settings_free(settings);

    if (result < 0)
    {
        gpiod_line_config_free(lineConfig);

        return false;
    }

    /*
     * GPIOリクエスト設定を生成する。
     */
    gpiod_request_config* requestConfig =
        gpiod_request_config_new();

    if (requestConfig == nullptr)
    {
        gpiod_line_config_free(lineConfig);

        return false;
    }

    /*
     * GPIO使用者名を設定する。
     */
    gpiod_request_config_set_consumer(
        requestConfig,
        "dht11-sensor");

    /*
     * GPIOを取得する。
     */
    m_request =
        gpiod_chip_request_lines(
            m_chip,
            requestConfig,
            lineConfig);

    /*
     * 設定を解放する。
     */
    gpiod_request_config_free(requestConfig);
    gpiod_line_config_free(lineConfig);

    if (m_request == nullptr)
    {
        std::cerr
            << "Failed to request GPIO output line."
            << std::endl;

        return false;
    }

    return true;
}

/**
 * @brief GPIOを入力モードに設定する
 *
 * @return true: 成功
 * @return false: 失敗
 */
bool Dht11Sensor::configureInput()
{
    /*
     * 現在のGPIOリクエストが存在しない場合は異常。
     */
    if (m_request == nullptr)
    {
        return false;
    }

    /*
     * GPIO設定を生成する。
     */
    gpiod_line_settings* settings =
        gpiod_line_settings_new();

    if (settings == nullptr)
    {
        return false;
    }

    /*
     * 入力モードに設定する。
     */
    gpiod_line_settings_set_direction(
        settings,
        GPIOD_LINE_DIRECTION_INPUT);

    /*
     * 外部プルアップがない構成でも
     * 読み取りできるよう内部プルアップを設定する。
     */
    gpiod_line_settings_set_bias(
        settings,
        GPIOD_LINE_BIAS_PULL_UP);

    /*
     * GPIOライン設定を生成する。
     */
    gpiod_line_config* lineConfig =
        gpiod_line_config_new();

    if (lineConfig == nullptr)
    {
        gpiod_line_settings_free(settings);

        return false;
    }

    /*
     * GPIO14を入力として設定する。
     */
    int result =
        gpiod_line_config_add_line_settings(
            lineConfig,
            &m_gpioPin,
            1,
            settings);

    gpiod_line_settings_free(settings);

    if (result < 0)
    {
        gpiod_line_config_free(lineConfig);

        return false;
    }

    /*
     * 既存のGPIOリクエストを入力設定へ変更する。
     */
    result =
        gpiod_line_request_reconfigure_lines(
            m_request,
            lineConfig);

    gpiod_line_config_free(lineConfig);

    if (result < 0)
    {
        std::cerr
            << "Failed to reconfigure GPIO as input."
            << std::endl;

        return false;
    }

    return true;
}

/**
 * @brief DHT11へ開始信号を送信する
 *
 * @return true: 成功
 * @return false: 失敗
 */
bool Dht11Sensor::sendStartSignal()
{
    /*
     * まずGPIOをHIGH出力にする。
     */
    if (!configureOutput(1))
    {
        return false;
    }

    /*
     * DHT11通信開始。
     *
     * DATA:
     *
     * HIGH
     *   ↓
     * LOW 約18ms
     *   ↓
     * HIGH 約40us
     *   ↓
     * INPUT
     */
    if (gpiod_line_request_set_value(
            m_request,
            m_gpioPin,
            GPIOD_LINE_VALUE_INACTIVE) < 0)
    {
        std::cerr
            << "Failed to set GPIO LOW."
            << std::endl;

        return false;
    }

    /*
     * LOWを約18ms保持する。
     */
    std::this_thread::sleep_for(
        std::chrono::milliseconds(
            START_LOW_TIME_MS));

    /*
     * HIGHへ戻す。
     */
    if (gpiod_line_request_set_value(
            m_request,
            m_gpioPin,
            GPIOD_LINE_VALUE_ACTIVE) < 0)
    {
        std::cerr
            << "Failed to set GPIO HIGH."
            << std::endl;

        return false;
    }

    /*
     * DHT11が応答するまで約40us待つ。
     */
    std::this_thread::sleep_for(
        std::chrono::microseconds(
            START_HIGH_TIME_US));

    /*
     * 入力へ切り替える。
     */
    if (!configureInput())
    {
        return false;
    }

    return true;
}

/**
 * @brief GPIOを読み取り、DHT11の40bitデータを取得する
 *
 * @param data 取得した5バイトのデータ
 *
 * @return true: 成功
 * @return false: 失敗
 */
bool Dht11Sensor::readRawData(
    std::uint8_t data[5])
{
    /*
     * データ領域を初期化する。
     */
    for (int i = 0;
         i < DHT11_BYTE_COUNT;
         ++i)
    {
        data[i] = 0;
    }

    /*
     * DHT11へ開始信号を送る。
     */
    if (!sendStartSignal())
    {
        return false;
    }

    /*
     * DHT11の通信を状態変化として
     * 直接読み取る。
     *
     * OSOYOOのサンプルと同じく、
     * 状態が変化するまでGPIOを読み続け、
     * その回数からHIGH期間を判定する。
     */
    int lastState = 1;

    int bitIndex = 0;

    /*
     * 最大85回程度の状態変化を監視する。
     *
     * DHT11通信は、
     *
     *   応答信号
     *   +
     *   40bit × 2状態
     *
     * となるため、85回程度の変化が発生する。
     */
    for (int timingIndex = 0;
         timingIndex < 85;
         ++timingIndex)
    {
        int counter = 0;

        /*
         * 現在のGPIO状態が変化するまで待つ。
         */
        while (true)
        {
            int currentState =
                gpiod_line_request_get_value(
                    m_request,
                    m_gpioPin);

            if (currentState < 0)
            {
                std::cerr
                    << "Failed to read GPIO."
                    << std::endl;

                return false;
            }

            if (currentState != lastState)
            {
                break;
            }

            ++counter;

            if (counter >= MAX_TIMING_COUNT)
            {
                std::cerr
                    << "DHT11 timing timeout."
                    << std::endl;

                return false;
            }

            /*
             * OSOYOO方式を参考に、
             * 短時間待ってから再度GPIOを読む。
             */
            std::this_thread::sleep_for(
                std::chrono::microseconds(1));
        }

        /*
         * 状態変化後の状態を取得する。
         */
        int currentState =
            gpiod_line_request_get_value(
                m_request,
                m_gpioPin);

        if (currentState < 0)
        {
            std::cerr
                << "Failed to read GPIO."
                << std::endl;

            return false;
        }

        lastState = currentState;

        /*
         * 最初の4回程度の状態変化は
         * DHT11の応答信号なので無視する。
         *
         * その後、偶数番目の状態変化時に
         * 各データビットを取り込む。
         */
        if (timingIndex >= 4 &&
            (timingIndex % 2 == 0))
        {
            if (bitIndex >= DHT11_BIT_COUNT)
            {
                break;
            }

            /*
             * HIGH期間が長ければ1、
             * 短ければ0と判定する。
             */
            data[bitIndex / 8] <<= 1;

            if (counter > BIT_THRESHOLD)
            {
                data[bitIndex / 8] |= 1;
            }

            ++bitIndex;
        }
    }

    /*
     * 40bit取得できなかった場合は異常。
     */
    if (bitIndex != DHT11_BIT_COUNT)
    {
        std::cerr
            << "Invalid DHT11 bit count: "
            << bitIndex
            << std::endl;

        return false;
    }

    return true;
}

/**
 * @brief DHT11データのチェックサムを確認する
 *
 * @param data DHT11から取得した5バイトのデータ
 *
 * @return true: 正常
 * @return false: 異常
 */
bool Dht11Sensor::checkChecksum(
    const std::uint8_t data[5]) const
{
    const std::uint8_t checksum =
        static_cast<std::uint8_t>(
            (data[0] +
             data[1] +
             data[2] +
             data[3]) & 0xFF);

    return checksum == data[4];
}

/**
 * @brief 温度・湿度を取得する
 *
 * @param temperature 取得した温度[℃]
 * @param humidity 取得した湿度[%]
 *
 * @return true: 成功
 * @return false: 失敗
 */
bool Dht11Sensor::read(
    float& temperature,
    float& humidity)
{
    std::uint8_t data[DHT11_BYTE_COUNT] = {};

    /*
     * DHT11から生データを取得する。
     */
    if (!readRawData(data))
    {
        return false;
    }

    /*
     * チェックサムを確認する。
     */
    if (!checkChecksum(data))
    {
        std::cerr
            << "DHT11 checksum error."
            << std::endl;

        std::cerr
            << "Raw data: "
            << static_cast<int>(data[0])
            << ", "
            << static_cast<int>(data[1])
            << ", "
            << static_cast<int>(data[2])
            << ", "
            << static_cast<int>(data[3])
            << ", "
            << static_cast<int>(data[4])
            << std::endl;

        return false;
    }

    /*
     * DHT11データ形式
     *
     * data[0] : 湿度整数部
     * data[1] : 湿度小数部
     * data[2] : 温度整数部
     * data[3] : 温度小数部
     * data[4] : チェックサム
     */
    humidity =
        static_cast<float>(data[0]) +
        static_cast<float>(data[1]) / 10.0f;

    temperature =
        static_cast<float>(data[2]) +
        static_cast<float>(data[3]) / 10.0f;

    /*
     * DHT11の一般的な温度範囲を確認する。
     */
    if (temperature < -40.0f ||
        temperature > 80.0f)
    {
        std::cerr
            << "Invalid temperature: "
            << temperature
            << std::endl;

        return false;
    }

    /*
     * 湿度範囲を確認する。
     */
    if (humidity < 0.0f ||
        humidity > 100.0f)
    {
        std::cerr
            << "Invalid humidity: "
            << humidity
            << std::endl;

        return false;
    }

    return true;
}