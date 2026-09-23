/**
 * @file Dht11Sensor.cpp
 * @brief DHT11温湿度センサの実装
 */

#include "sensor/Dht11Sensor.h"

#include <lgpio.h>

#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <thread>
#include <vector>

namespace
{
constexpr int GPIO_CHIP = 0;

constexpr int START_LOW_MS = 18;
constexpr int START_HIGH_US = 40;

constexpr int DATA_BITS = 40;
constexpr int DATA_BYTES = 5;

/*
 * DHT11のHIGHパルス幅
 *
 * 0 : 約26～28us
 * 1 : 約70us
 *
 * 中間値として50usを使用。
 */
constexpr int BIT_THRESHOLD_US = 50;

/*
 * DHT11の応答は数ms程度。
 *
 * Linux上でのスケジューリング遅延を考慮して
 * 十分大きく設定する。
 */
constexpr int READ_TIMEOUT_MS = 100;

/*
 * DHT11通信で必要なエッジ数。
 *
 * 応答:
 *   LOW
 *   HIGH
 *
 * その後40bit。
 *
 * 40bit × 2エッジ = 80エッジ
 *
 * 応答部分を含めて余裕を持たせる。
 */
constexpr int MAX_EDGES = 100;
}

/**
 * @brief エッジ情報を保持するコンテキスト
 */
struct Dht11Sensor::AlertContext
{
    struct Edge
    {
        int level;
        std::uint32_t tick;
    };

    std::mutex mutex;
    std::condition_variable condition;

    std::vector<Edge> edges;

    bool collecting = false;
};

/**
 * @brief lgpioエッジコールバック
 *
 * lgpioからGPIO状態変化が通知されるたびに呼び出される。
 */
static void gpioAlertCallback(
    int e,
    lgGpioAlert_p evt,
    void* userdata)
{
    if (e != 0 || evt == nullptr || userdata == nullptr)
    {
        return;
    }

    auto* context =
        static_cast<Dht11Sensor::AlertContext*>(userdata);

    std::lock_guard<std::mutex> lock(
        context->mutex);

    if (!context->collecting)
    {
        return;
    }

    if (context->edges.size() >= MAX_EDGES)
    {
        return;
    }

    /*
     * DHT11では両エッジを取得する。
     *
     * evt->report.level:
     *
     * 0 = LOW
     * 1 = HIGH
     */
    AlertContext::Edge edge;

    edge.level = evt->report.level;
    edge.tick = evt->report.tick;

    context->edges.push_back(edge);

    context->condition.notify_one();
}

/**
 * @brief コンストラクタ
 */
Dht11Sensor::Dht11Sensor(unsigned int gpioPin)
    : m_gpioPin(gpioPin),
      m_gpioHandle(-1),
      m_gpioClaimed(false),
      m_alertClaimed(false),
      m_alertContext(new AlertContext())
{
}

/**
 * @brief デストラクタ
 */
Dht11Sensor::~Dht11Sensor()
{
    releaseGpio();

    delete m_alertContext;
    m_alertContext = nullptr;

    if (m_gpioHandle >= 0)
    {
        lgGpiochipClose(m_gpioHandle);
        m_gpioHandle = -1;
    }
}

/**
 * @brief DHT11を初期化する
 */
bool Dht11Sensor::initialize()
{
    /*
     * gpiochip0をオープンする。
     */
    m_gpioHandle =
        lgGpiochipOpen(GPIO_CHIP);

    if (m_gpioHandle < 0)
    {
        std::printf(
            "lgGpiochipOpen failed: %d\n",
            m_gpioHandle);

        return false;
    }

    /*
     * DHT11はアイドル時HIGH。
     */
    if (!claimOutput(1))
    {
        return false;
    }

    std::printf(
        "DHT11 initialized. GPIO=%u\n",
        m_gpioPin);

    return true;
}

/**
 * @brief GPIOを出力として確保する
 */
bool Dht11Sensor::claimOutput(int initialValue)
{
    releaseGpio();

    const int result =
        lgGpioClaimOutput(
            m_gpioHandle,
            0,
            static_cast<int>(m_gpioPin),
            initialValue);

    if (result < 0)
    {
        std::printf(
            "lgGpioClaimOutput failed: %d\n",
            result);

        return false;
    }

    m_gpioClaimed = true;

    return true;
}

/**
 * @brief GPIOを入力＋エッジ検出として確保する
 */
bool Dht11Sensor::claimAlert()
{
    releaseGpio();

    /*
     * GPIOを両エッジ検出として確保する。
     *
     * これにより、
     *
     * LOW → HIGH
     * HIGH → LOW
     *
     * の両方をlgpioから通知してもらう。
     */
    const int result =
        lgGpioClaimAlert(
            m_gpioHandle,
            0,
            LG_BOTH_EDGES,
            static_cast<int>(m_gpioPin),
            -1);

    if (result < 0)
    {
        std::printf(
            "lgGpioClaimAlert failed: %d\n",
            result);

        return false;
    }

    m_gpioClaimed = true;

    /*
     * コールバックを登録する。
     */
    const int callbackResult =
        lgGpioSetAlertsFunc(
            m_gpioHandle,
            static_cast<int>(m_gpioPin),
            gpioAlertCallback,
            m_alertContext);

    if (callbackResult < 0)
    {
        std::printf(
            "lgGpioSetAlertsFunc failed: %d\n",
            callbackResult);

        releaseGpio();

        return false;
    }

    m_alertClaimed = true;

    return true;
}

/**
 * @brief GPIOを解放する
 */
void Dht11Sensor::releaseGpio()
{
    if (m_gpioHandle < 0)
    {
        return;
    }

    if (m_alertClaimed)
    {
        /*
         * コールバックを停止する。
         */
        lgGpioSetAlertsFunc(
            m_gpioHandle,
            static_cast<int>(m_gpioPin),
            nullptr,
            nullptr);

        m_alertClaimed = false;
    }

    if (m_gpioClaimed)
    {
        lgGpioFree(
            m_gpioHandle,
            static_cast<int>(m_gpioPin));

        m_gpioClaimed = false;
    }
}

/**
 * @brief DHT11開始信号を送信する
 */
bool Dht11Sensor::sendStartSignal()
{
    /*
     * DHT11開始:
     *
     * HIGH
     * ↓
     * LOW 18ms
     * ↓
     * HIGH
     * ↓
     * 入力
     */

    if (lgGpioWrite(
            m_gpioHandle,
            static_cast<int>(m_gpioPin),
            0) < 0)
    {
        std::printf(
            "Failed to set GPIO LOW.\n");

        return false;
    }

    /*
     * LOWを18ms維持。
     */
    std::this_thread::sleep_for(
        std::chrono::milliseconds(
            START_LOW_MS));

    /*
     * HIGHへ戻す。
     */
    if (lgGpioWrite(
            m_gpioHandle,
            static_cast<int>(m_gpioPin),
            1) < 0)
    {
        std::printf(
            "Failed to set GPIO HIGH.\n");

        return false;
    }

    /*
     * 約40us待つ。
     */
    std::this_thread::sleep_for(
        std::chrono::microseconds(
            START_HIGH_US));

    return true;
}

/**
 * @brief DHT11から40bitを取得する
 */
bool Dht11Sensor::readRawData(
    std::uint8_t data[DATA_BYTES])
{
    if (data == nullptr)
    {
        return false;
    }

    std::memset(
        data,
        0,
        DATA_BYTES);

    /*
     * まず出力としてGPIOを確保。
     */
    if (!claimOutput(1))
    {
        return false;
    }

    /*
     * DHT11開始信号。
     */
    if (!sendStartSignal())
    {
        return false;
    }

    /*
     * 開始信号を送信した直後に、
     * エッジ検出へ切り替える。
     */
    {
        std::lock_guard<std::mutex> lock(
            m_alertContext->mutex);

        m_alertContext->edges.clear();
        m_alertContext->collecting = true;
    }

    /*
     * 入力＋エッジ検出に切り替える。
     */
    if (!claimAlert())
    {
        std::lock_guard<std::mutex> lock(
            m_alertContext->mutex);

        m_alertContext->collecting = false;

        return false;
    }

    /*
     * DHT11のエッジが蓄積されるまで待つ。
     */
    {
        std::unique_lock<std::mutex> lock(
            m_alertContext->mutex);

        const bool completed =
            m_alertContext->condition.wait_for(
                lock,
                std::chrono::milliseconds(
                    READ_TIMEOUT_MS),
                [this]()
                {
                    return
                        m_alertContext->edges.size()
                        >= 83;
                });

        if (!completed)
        {
            m_alertContext->collecting = false;

            std::printf(
                "DHT11 edge timeout. edges=%zu\n",
                m_alertContext->edges.size());

            return false;
        }

        /*
         * コールバックからの取得を停止。
         */
        m_alertContext->collecting = false;
    }

    /*
     * GPIOを解放。
     */
    releaseGpio();

    /*
     * エッジデータを解析する。
     */
    std::vector<AlertContext::Edge> edges;

    {
        std::lock_guard<std::mutex> lock(
            m_alertContext->mutex);

        edges = m_alertContext->edges;
    }

    if (edges.size() < 83)
    {
        std::printf(
            "DHT11 edge count too small: %zu\n",
            edges.size());

        return false;
    }

    /*
     * DHT11の通信は、
     *
     * 応答:
     *   LOW 約80us
     *   HIGH 約80us
     *
     * データ:
     *   LOW 約50us
     *   HIGH 約26us → 0
     *   HIGH 約70us → 1
     *
     * となる。
     *
     * HIGH → LOWのエッジ間の時間を
     * 40個取り出す。
     */
    int bitIndex = 0;

    for (std::size_t i = 1;
         i < edges.size() &&
         bitIndex < DATA_BITS;
         ++i)
    {
        /*
         * LOW → HIGHになったエッジ。
         *
         * この次のHIGH → LOWまでが
         * データビットのHIGH期間。
         */
        if (edges[i - 1].level == 0 &&
            edges[i].level == 1)
        {
            if (i + 1 >= edges.size())
            {
                break;
            }

            /*
             * HIGH期間を計算。
             *
             * lgpioのtickはマイクロ秒単位。
             */
            const std::uint32_t highStart =
                edges[i].tick;

            const std::uint32_t highEnd =
                edges[i + 1].tick;

            /*
             * tickは32bitで循環するため、
             * unsigned演算で差を取る。
             */
            const std::uint32_t pulseWidth =
                highEnd - highStart;

            /*
             * 0 / 1判定。
             */
            const int bit =
                (pulseWidth >= BIT_THRESHOLD_US)
                    ? 1
                    : 0;

            const int byteIndex =
                bitIndex / 8;

            data[byteIndex] <<= 1;

            if (bit != 0)
            {
                data[byteIndex] |= 1;
            }

            ++bitIndex;
        }
    }

    if (bitIndex != DATA_BITS)
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
    const std::uint8_t data[DATA_BYTES]) const
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
    std::uint8_t data[DATA_BYTES];

    /*
     * DHT11データ取得。
     */
    if (!readRawData(data))
    {
        return false;
    }

    /*
     * デバッグ用に生データを表示。
     */
    std::printf(
        "DHT11 raw: "
        "%02X %02X %02X %02X %02X\n",
        data[0],
        data[1],
        data[2],
        data[3],
        data[4]);

    /*
     * チェックサム。
     */
    if (!checkChecksum(data))
    {
        std::printf(
            "DHT11 checksum error.\n");

        return false;
    }

    /*
     * 湿度。
     */
    humidity =
        static_cast<float>(data[0]) +
        static_cast<float>(data[1]) / 10.0F;

    /*
     * 温度。
     */
    temperature =
        static_cast<float>(data[2]) +
        static_cast<float>(data[3]) / 10.0F;

    /*
     * DHT11の範囲チェック。
     */
    if (humidity < 0.0F ||
        humidity > 100.0F)
    {
        std::printf(
            "Humidity out of range: %.1f\n",
            humidity);

        return false;
    }

    if (temperature < -40.0F ||
        temperature > 80.0F)
    {
        std::printf(
            "Temperature out of range: %.1f\n",
            temperature);

        return false;
    }

    return true;
}