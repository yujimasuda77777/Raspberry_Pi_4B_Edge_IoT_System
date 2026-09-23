/**
 * @file Dht11Sensor.cpp
 * @brief DHT11温湿度センサクラスの実装
 */

#include "sensor/Dht11Sensor.h"

#include <gpiod.h>

#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <thread>

/**
 * @brief GPIOデバイス
 */
namespace
{
constexpr char GPIO_CHIP[] = "/dev/gpiochip0";

/**
 * @brief DHT11のスタート信号LOW時間[ms]
 */
constexpr unsigned int START_LOW_TIME_MS = 20;

/**
 * @brief GPIOイベント待ちタイムアウト[ms]
 */
constexpr std::int64_t EDGE_TIMEOUT_NS = 5'000'000;

/**
 * @brief DHT11の40bitデータ数
 */
constexpr std::size_t DHT11_BIT_COUNT = 40;

/**
 * @brief DHT11のデータバイト数
 */
constexpr std::size_t DHT11_BYTE_COUNT = 5;

/**
 * @brief 0/1判定用のパルス幅境界[us]
 *
 * DHT11では、
 *   0：約26～28us
 *   1：約70us
 *
 * となるため、中間値として50usを使用する。
 */
constexpr std::uint64_t BIT_THRESHOLD_NS = 50'000;

/**
 * @brief DHT11の応答エッジ数
 *
 * DHT11の応答開始から40bit分の通信を取得する。
 */
constexpr std::size_t MAX_EDGE_EVENTS = 100;
}

/**
 * @brief コンストラクタ
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
 * @return true: 成功 / false: 失敗
 */
bool Dht11Sensor::initialize()
{
    m_chip = gpiod_chip_open(GPIO_CHIP);

    if (m_chip == nullptr)
    {
        std::cerr << "Failed to open GPIO chip: "
                  << GPIO_CHIP
                  << std::endl;

        return false;
    }

    if (!configureOutput(1))
    {
        return false;
    }

    std::cout << "DHT11 initialized. GPIO="
              << m_gpioPin
              << std::endl;

    return true;
}

/**
 * @brief GPIOを出力モードに設定する
 * @param initialValue 初期出力値
 * @return true: 成功 / false: 失敗
 */
bool Dht11Sensor::configureOutput(int initialValue)
{
    releaseRequest();

    gpiod_line_settings* settings = gpiod_line_settings_new();

    if (settings == nullptr)
    {
        std::cerr << "Failed to create GPIO line settings."
                  << std::endl;

        return false;
    }

    gpiod_line_settings_set_direction(
        settings,
        GPIOD_LINE_DIRECTION_OUTPUT);

    gpiod_line_settings_set_drive(
        settings,
        GPIOD_LINE_DRIVE_PUSH_PULL);

    gpiod_line_settings_set_output_value(
        settings,
        initialValue
            ? GPIOD_LINE_VALUE_ACTIVE
            : GPIOD_LINE_VALUE_INACTIVE);

    gpiod_line_config* lineConfig = gpiod_line_config_new();

    if (lineConfig == nullptr)
    {
        gpiod_line_settings_free(settings);

        std::cerr << "Failed to create GPIO line config."
                  << std::endl;

        return false;
    }

    int result = gpiod_line_config_add_line_settings(
        lineConfig,
        &m_gpioPin,
        1,
        settings);

    gpiod_line_settings_free(settings);

    if (result < 0)
    {
        gpiod_line_config_free(lineConfig);

        std::cerr << "Failed to add GPIO line settings."
                  << std::endl;

        return false;
    }

    gpiod_request_config* requestConfig =
        gpiod_request_config_new();

    if (requestConfig == nullptr)
    {
        gpiod_line_config_free(lineConfig);

        std::cerr << "Failed to create GPIO request config."
                  << std::endl;

        return false;
    }

    gpiod_request_config_set_consumer(
        requestConfig,
        "dht11-sensor");

    m_request = gpiod_chip_request_lines(
        m_chip,
        requestConfig,
        lineConfig);

    gpiod_request_config_free(requestConfig);
    gpiod_line_config_free(lineConfig);

    if (m_request == nullptr)
    {
        std::cerr << "Failed to request GPIO output line."
                  << std::endl;

        return false;
    }

    return true;
}

/**
 * @brief GPIOを入力＋両エッジ検出に設定する
 * @return true: 成功 / false: 失敗
 */
bool Dht11Sensor::configureInput()
{
    if (m_request == nullptr)
    {
        return false;
    }

    gpiod_line_settings* settings = gpiod_line_settings_new();

    if (settings == nullptr)
    {
        return false;
    }

    gpiod_line_settings_set_direction(
        settings,
        GPIOD_LINE_DIRECTION_INPUT);

    gpiod_line_settings_set_edge_detection(
        settings,
        GPIOD_LINE_EDGE_BOTH);

    gpiod_line_settings_set_event_clock(
        settings,
        GPIOD_LINE_CLOCK_MONOTONIC);

    gpiod_line_config* lineConfig = gpiod_line_config_new();

    if (lineConfig == nullptr)
    {
        gpiod_line_settings_free(settings);
        return false;
    }

    int result = gpiod_line_config_add_line_settings(
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

    result = gpiod_line_request_reconfigure_lines(
        m_request,
        lineConfig);

    gpiod_line_config_free(lineConfig);

    if (result < 0)
    {
        std::cerr << "Failed to reconfigure GPIO as input."
                  << std::endl;

        return false;
    }

    return true;
}

/**
 * @brief DHT11へ開始信号を送信する
 * @return true: 成功 / false: 失敗
 */
bool Dht11Sensor::sendStartSignal()
{
    if (!configureOutput(1))
    {
        return false;
    }

    /*
     * DHT11の開始信号：
     *
     * 1. ホスト側がLOWにする
     * 2. 18ms以上保持する
     * 3. HIGHへ戻す
     * 4. DHT11が応答する
     */
    if (gpiod_line_request_set_value(
            m_request,
            m_gpioPin,
            GPIOD_LINE_VALUE_INACTIVE) < 0)
    {
        return false;
    }

    std::this_thread::sleep_for(
        std::chrono::milliseconds(START_LOW_TIME_MS));

    if (gpiod_line_request_set_value(
            m_request,
            m_gpioPin,
            GPIOD_LINE_VALUE_ACTIVE) < 0)
    {
        return false;
    }

    /*
     * HIGHへ戻した直後に入力へ切り替える。
     */
    if (!configureInput())
    {
        return false;
    }

    return true;
}

/**
 * @brief DHT11から40bitのデータを取得する
 * @param data 取得した5バイトのデータ
 * @return true: 成功 / false: 失敗
 */
bool Dht11Sensor::readRawData(std::uint8_t data[5])
{
    std::memset(
        data,
        0,
        DHT11_BYTE_COUNT);

    if (!sendStartSignal())
    {
        return false;
    }

    gpiod_edge_event_buffer* buffer =
        gpiod_edge_event_buffer_new(MAX_EDGE_EVENTS);

    if (buffer == nullptr)
    {
        std::cerr << "Failed to create edge event buffer."
                  << std::endl;

        return false;
    }

    /*
     * DHT11の応答開始からデータ取得完了まで待つ。
     */
    int waitResult = gpiod_line_request_wait_edge_events(
        m_request,
        EDGE_TIMEOUT_NS);

    if (waitResult <= 0)
    {
        gpiod_edge_event_buffer_free(buffer);

        std::cerr << "DHT11 edge event timeout."
                  << std::endl;

        return false;
    }

    int eventCount = gpiod_line_request_read_edge_events(
        m_request,
        buffer,
        MAX_EDGE_EVENTS);

    if (eventCount < 0)
    {
        gpiod_edge_event_buffer_free(buffer);

        std::cerr << "Failed to read DHT11 edge events."
                  << std::endl;

        return false;
    }

    /*
     * DHT11の通信では、
     *
     *   50us LOW
     *   + HIGH幅
     *
     * のHIGH幅で0/1を判定する。
     *
     * 40bit分のHIGHパルスを探す。
     */
    std::uint64_t previousTimestamp = 0;
    bool havePreviousTimestamp = false;

    std::size_t bitIndex = 0;

    for (int i = 0; i < eventCount; ++i)
    {
        gpiod_edge_event* event =
            gpiod_edge_event_buffer_get_event(
                buffer,
                i);

        if (event == nullptr)
        {
            continue;
        }

        const auto eventType =
            gpiod_edge_event_get_event_type(event);

        const std::uint64_t timestamp =
            gpiod_edge_event_get_timestamp_ns(event);

        /*
         * HIGHになった時刻を記録する。
         */
        if (eventType == GPIOD_EDGE_EVENT_RISING_EDGE)
        {
            previousTimestamp = timestamp;
            havePreviousTimestamp = true;
        }
        /*
         * LOWになった時点でHIGH時間を計算する。
         */
        else if (eventType == GPIOD_EDGE_EVENT_FALLING_EDGE &&
                 havePreviousTimestamp)
        {
            const std::uint64_t highTime =
                timestamp - previousTimestamp;

            /*
             * 最初の80us程度の応答HIGHなどを除外し、
             * DHT11のデータビットとして扱う。
             *
             * 40bitを超えたら終了。
             */
            if (highTime > 10'000 &&
                highTime < 100'000)
            {
                const bool bitValue =
                    highTime >= BIT_THRESHOLD_NS;

                const std::size_t byteIndex =
                    bitIndex / 8;

                data[byteIndex] <<= 1;

                if (bitValue)
                {
                    data[byteIndex] |= 1;
                }

                ++bitIndex;

                if (bitIndex >= DHT11_BIT_COUNT)
                {
                    break;
                }
            }

            havePreviousTimestamp = false;
        }
    }

    gpiod_edge_event_buffer_free(buffer);

    if (bitIndex != DHT11_BIT_COUNT)
    {
        std::cerr << "Invalid DHT11 bit count: "
                  << bitIndex
                  << std::endl;

        return false;
    }

    return true;
}

/**
 * @brief チェックサムを確認する
 * @param data DHT11から取得した5バイトのデータ
 * @return true: 正常 / false: 異常
 */
bool Dht11Sensor::checkChecksum(
    const std::uint8_t data[5]) const
{
    const std::uint8_t checksum =
        static_cast<std::uint8_t>(
            data[0] +
            data[1] +
            data[2] +
            data[3]);

    return checksum == data[4];
}

/**
 * @brief 温度・湿度を取得する
 * @param temperature 取得した温度[℃]
 * @param humidity 取得した湿度[%]
 * @return true: 成功 / false: 失敗
 */
bool Dht11Sensor::read(
    float& temperature,
    float& humidity)
{
    std::uint8_t data[DHT11_BYTE_COUNT] = {};

    if (!readRawData(data))
    {
        return false;
    }

    if (!checkChecksum(data))
    {
        std::cerr << "DHT11 checksum error."
                  << std::endl;

        return false;
    }

    /*
     * DHT11は整数部＋小数部で温湿度を表す。
     *
     * data[0] : 湿度整数部
     * data[1] : 湿度小数部
     * data[2] : 温度整数部
     * data[3] : 温度小数部
     */
    humidity =
        static_cast<float>(data[0]) +
        static_cast<float>(data[1]) / 10.0f;

    temperature =
        static_cast<float>(data[2]) +
        static_cast<float>(data[3]) / 10.0f;

    /*
     * 今回はDHT11なので符号ビットは通常使用しない。
     */
    if (temperature < -40.0f ||
        temperature > 80.0f)
    {
        std::cerr << "Invalid temperature: "
                  << temperature
                  << std::endl;

        return false;
    }

    if (humidity < 0.0f ||
        humidity > 100.0f)
    {
        std::cerr << "Invalid humidity: "
                  << humidity
                  << std::endl;

        return false;
    }

    return true;
}