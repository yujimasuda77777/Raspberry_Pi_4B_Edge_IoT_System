/**
 * @file Dht11Sensor.cpp
 * @brief DHT11温湿度センサの実装
 */

#include "sensor/Dht11Sensor.h"

#include <gpiod.h>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <thread>

/*
 * Raspberry PiのGPIOチップ
 */
namespace
{
constexpr const char* GPIO_CHIP_NAME = "/dev/gpiochip0";

/*
 * DHT11通信仕様
 */
constexpr int START_LOW_TIME_MS = 18;
constexpr int START_HIGH_TIME_US = 40;

/*
 * DHT11の通信を監視する最大時間
 *
 * DHT11の応答そのものは数ms程度だが、
 * Linux上での処理遅延を考慮して余裕を持たせる。
 */
constexpr int READ_TIMEOUT_MS = 250;

/*
 * DHT11は40bit送信する。
 */
constexpr int DHT11_DATA_BYTES = 5;
constexpr int DHT11_DATA_BITS = 40;

/*
 * HIGHパルス幅による0/1判定値
 *
 * DHT11:
 *   0 = 約26～28us
 *   1 = 約70us
 *
 * この値より長ければ1と判定する。
 */
constexpr long BIT_THRESHOLD_US = 50;

/*
 * DHT11応答開始時のLOW/HIGHを含め、
 * GPIO状態変化をある程度多めに保持する。
 */
constexpr int MAX_TRANSITIONS = 100;
}

/**
 * @brief コンストラクタ
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
 * @brief DHT11を初期化する
 */
bool Dht11Sensor::initialize()
{
    /*
     * GPIOチップをオープンする。
     */
    m_chip = gpiod_chip_open(GPIO_CHIP_NAME);

    if (m_chip == nullptr)
    {
        std::printf("Failed to open GPIO chip: %s\n",
                    std::strerror(errno));
        return false;
    }

    /*
     * 最初は出力LOWにしておく。
     */
    if (!configureOutput(1))
    {
        return false;
    }

    std::printf("DHT11 initialized. GPIO=%u\n", m_gpioPin);

    return true;
}

/**
 * @brief GPIOを出力モードに設定する
 */
bool Dht11Sensor::configureOutput(int initialValue)
{
    /*
     * 既存の要求を解放する。
     */
    releaseRequest();

    /*
     * GPIOライン設定を作成する。
     */
    gpiod_line_settings* settings = gpiod_line_settings_new();

    if (settings == nullptr)
    {
        std::printf("Failed to create GPIO settings.\n");
        return false;
    }

    /*
     * 出力モードを設定する。
     */
    if (gpiod_line_settings_set_direction(
            settings,
            GPIOD_LINE_DIRECTION_OUTPUT) < 0)
    {
        std::printf("Failed to set GPIO direction.\n");
        gpiod_line_settings_free(settings);
        return false;
    }

    /*
     * 初期出力値を設定する。
     */
    gpiod_line_settings_set_output_value(
        settings,
        initialValue
            ? GPIOD_LINE_VALUE_ACTIVE
            : GPIOD_LINE_VALUE_INACTIVE);

    /*
     * GPIOライン設定を作成する。
     */
    gpiod_line_config* lineConfig = gpiod_line_config_new();

    if (lineConfig == nullptr)
    {
        std::printf("Failed to create GPIO line config.\n");
        gpiod_line_settings_free(settings);
        return false;
    }

    unsigned int offset = m_gpioPin;

    if (gpiod_line_config_add_line_settings(
            lineConfig,
            &offset,
            1,
            settings) < 0)
    {
        std::printf("Failed to add GPIO line settings.\n");

        gpiod_line_config_free(lineConfig);
        gpiod_line_settings_free(settings);

        return false;
    }

    /*
     * GPIO要求を作成する。
     */
    m_request = gpiod_chip_request_lines(
        m_chip,
        nullptr,
        lineConfig);

    gpiod_line_config_free(lineConfig);
    gpiod_line_settings_free(settings);

    if (m_request == nullptr)
    {
        std::printf("Failed to request GPIO output line: %s\n",
                    std::strerror(errno));
        return false;
    }

    return true;
}

/**
 * @brief GPIOを入力モードに設定する
 */
bool Dht11Sensor::configureInput()
{
    /*
     * 現在のGPIO要求を解放する。
     */
    releaseRequest();

    /*
     * GPIOライン設定を作成する。
     */
    gpiod_line_settings* settings = gpiod_line_settings_new();

    if (settings == nullptr)
    {
        std::printf("Failed to create GPIO settings.\n");
        return false;
    }

    /*
     * 入力モードを設定する。
     */
    if (gpiod_line_settings_set_direction(
            settings,
            GPIOD_LINE_DIRECTION_INPUT) < 0)
    {
        std::printf("Failed to set GPIO input direction.\n");
        gpiod_line_settings_free(settings);
        return false;
    }

    /*
     * DHT11のデータラインはアイドルHIGH。
     *
     * DHT11モジュール側にプルアップ抵抗があることを
     * 想定するため、ここでは内部プルアップを必須としない。
     */
    gpiod_line_settings_set_bias(
        settings,
        GPIOD_LINE_BIAS_PULL_UP);

    /*
     * GPIOライン設定を作成する。
     */
    gpiod_line_config* lineConfig = gpiod_line_config_new();

    if (lineConfig == nullptr)
    {
        std::printf("Failed to create GPIO line config.\n");
        gpiod_line_settings_free(settings);
        return false;
    }

    unsigned int offset = m_gpioPin;

    if (gpiod_line_config_add_line_settings(
            lineConfig,
            &offset,
            1,
            settings) < 0)
    {
        std::printf("Failed to add GPIO input settings.\n");

        gpiod_line_config_free(lineConfig);
        gpiod_line_settings_free(settings);

        return false;
    }

    /*
     * GPIO要求を作成する。
     */
    m_request = gpiod_chip_request_lines(
        m_chip,
        nullptr,
        lineConfig);

    gpiod_line_config_free(lineConfig);
    gpiod_line_settings_free(settings);

    if (m_request == nullptr)
    {
        std::printf("Failed to request GPIO input line: %s\n",
                    std::strerror(errno));
        return false;
    }

    return true;
}

/**
 * @brief DHT11へ開始信号を送信する
 */
bool Dht11Sensor::sendStartSignal()
{
    /*
     * DHT11の開始信号
     *
     * 1. LOWを18ms以上
     * 2. HIGHに戻す
     * 3. その後すぐ入力へ切り替える
     */

    /*
     * LOWを18ms出力する。
     */
    if (gpiod_line_request_set_value(
            m_request,
            m_gpioPin,
            GPIOD_LINE_VALUE_INACTIVE) < 0)
    {
        std::printf("Failed to set GPIO LOW.\n");
        return false;
    }

    std::this_thread::sleep_for(
        std::chrono::milliseconds(START_LOW_TIME_MS));

    /*
     * HIGHに戻す。
     */
    if (gpiod_line_request_set_value(
            m_request,
            m_gpioPin,
            GPIOD_LINE_VALUE_ACTIVE) < 0)
    {
        std::printf("Failed to set GPIO HIGH.\n");
        return false;
    }

    /*
     * 約40us待つ。
     */
    std::this_thread::sleep_for(
        std::chrono::microseconds(START_HIGH_TIME_US));

    return true;
}

/**
 * @brief GPIO要求を解放する
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
 * @brief DHT11から40bitの生データを取得する
 */
bool Dht11Sensor::readRawData(std::uint8_t data[5])
{
    if (data == nullptr)
    {
        return false;
    }

    std::memset(data, 0, DHT11_DATA_BYTES);

    /*
     * 開始信号を送信する。
     */
    if (!sendStartSignal())
    {
        return false;
    }

    /*
     * GPIOを入力へ切り替える。
     */
    if (!configureInput())
    {
        return false;
    }

    /*
     * DHT11の応答を待ちながら、
     * GPIOの状態変化を高速ポーリングする。
     *
     * Python版のAdafruit_DHTが行っている
     * bitbang方式を参考にしている。
     */
    struct Transition
    {
        int value;
        std::chrono::steady_clock::time_point timestamp;
    };

    Transition transitions[MAX_TRANSITIONS];
    int transitionCount = 0;

    int previousValue = -1;

    const auto startTime =
        std::chrono::steady_clock::now();

    while (true)
    {
        const auto now =
            std::chrono::steady_clock::now();

        const auto elapsed =
            std::chrono::duration_cast<
                std::chrono::milliseconds>(
                    now - startTime)
                .count();

        /*
         * 最大250ms監視する。
         */
        if (elapsed >= READ_TIMEOUT_MS)
        {
            break;
        }

        /*
         * GPIO状態を取得する。
         */
        const int value =
            gpiod_line_request_get_value(
                m_request,
                m_gpioPin);

        if (value < 0)
        {
            std::printf("Failed to read GPIO: %s\n",
                        std::strerror(errno));
            return false;
        }

        /*
         * 最初の状態を記録する。
         */
        if (previousValue < 0)
        {
            previousValue = value;
            continue;
        }

        /*
         * GPIO状態が変化した場合だけ記録する。
         *
         * ここではsleepを入れない。
         *
         * DHT11は数十us単位のパルスを使用するため、
         * 1us sleepなどを入れるとLinux上では実際には
         * 大幅に遅延する可能性がある。
         */
        if (value != previousValue)
        {
            if (transitionCount < MAX_TRANSITIONS)
            {
                transitions[transitionCount].value = value;
                transitions[transitionCount].timestamp = now;

                ++transitionCount;
            }

            previousValue = value;

            /*
             * DHT11は40bit送信する。
             *
             * データ取得に必要な変化数が揃ったら
             * それ以上待つ必要はない。
             */
            if (transitionCount >= 83)
            {
                break;
            }
        }
    }

    /*
     * 40bitを取得するには、
     * DHT11のデータ部分だけで少なくとも80回程度の
     * HIGH/LOW変化が必要。
     */
    if (transitionCount < 80)
    {
        std::printf(
            "DHT11 transition count too small: %d\n",
            transitionCount);

        return false;
    }

    /*
     * DHT11の通信は、
     *
     * 応答LOW
     * 応答HIGH
     * 40bitデータ
     *
     * という構成。
     *
     * 各HIGH期間の長さを測定し、
     * 約50usを境に0/1を判定する。
     */

    int bitIndex = 0;

    /*
     * transition[0]～transition[n]の
     * 隣接する時刻差を調べる。
     */
    for (int i = 1;
         i < transitionCount && bitIndex < DHT11_DATA_BITS;
         ++i)
    {
        /*
         * transition[i-1] → transition[i] の時間。
         */
        const auto pulseWidth =
            std::chrono::duration_cast<
                std::chrono::microseconds>(
                    transitions[i].timestamp -
                    transitions[i - 1].timestamp)
                .count();

        /*
         * HIGH期間だけをデータビットとして扱う。
         *
         * transition[i-1].value == 1
         * なら、直前の状態がHIGHだったため、
         * 今回の遷移までがHIGHパルス幅になる。
         */
        if (transitions[i - 1].value == 1)
        {
            /*
             * 50us以上なら1。
             * 50us未満なら0。
             */
            const int bit =
                (pulseWidth >= BIT_THRESHOLD_US)
                    ? 1
                    : 0;

            const int byteIndex = bitIndex / 8;

            data[byteIndex] <<= 1;

            if (bit != 0)
            {
                data[byteIndex] |= 1;
            }

            ++bitIndex;
        }
    }

    /*
     * 40bit取得できなかった場合。
     */
    if (bitIndex != DHT11_DATA_BITS)
    {
        std::printf(
            "DHT11 bit count invalid: %d\n",
            bitIndex);

        return false;
    }

    return true;
}

/**
 * @brief チェックサムを確認する
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
 * @brief DHT11から温湿度を取得する
 */
bool Dht11Sensor::read(
    float& temperature,
    float& humidity)
{
    std::uint8_t data[DHT11_DATA_BYTES];

    /*
     * 生データ取得。
     */
    if (!readRawData(data))
    {
        return false;
    }

    /*
     * チェックサム確認。
     */
    if (!checkChecksum(data))
    {
        std::printf(
            "DHT11 checksum error: "
            "%02X %02X %02X %02X %02X\n",
            data[0],
            data[1],
            data[2],
            data[3],
            data[4]);

        return false;
    }

    /*
     * DHT11は整数部と小数部を別々に返す。
     *
     * DHT11の場合、通常は小数部は0。
     */
    humidity =
        static_cast<float>(data[0]) +
        static_cast<float>(data[1]) / 10.0F;

    temperature =
        static_cast<float>(data[2]) +
        static_cast<float>(data[3]) / 10.0F;

    /*
     * DHT11の値として明らかにおかしい場合は
     * エラーとする。
     */
    if (humidity < 0.0F || humidity > 100.0F)
    {
        std::printf(
            "DHT11 humidity out of range: %.1f\n",
            humidity);

        return false;
    }

    if (temperature < -40.0F ||
        temperature > 80.0F)
    {
        std::printf(
            "DHT11 temperature out of range: %.1f\n",
            temperature);

        return false;
    }

    return true;
}