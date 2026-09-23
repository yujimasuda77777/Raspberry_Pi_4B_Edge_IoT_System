#include "sensor/Dht11Sensor.h"

#include <lgpio.h>

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

/**
 * @brief GPIOエッジ情報
 */
struct GpioEdge
{
    std::uint8_t level;
    std::uint32_t timestamp;
};

/**
 * @brief DHT11読み取り用コンテキスト
 */
struct AlertContext
{
    std::mutex mutex;
    std::condition_variable condition;

    /**
     * @brief HIGH期間の時間[us]
     */
    std::vector<std::uint32_t> highPulses;

    /**
     * @brief 現在HIGH期間を計測中か
     */
    bool highActive;

    /**
     * @brief HIGHになった時刻
     */
    std::uint32_t highStart;

    /**
     * @brief データ収集中か
     */
    bool collecting;

    AlertContext()
        : highActive(false),
          highStart(0),
          collecting(true)
    {
    }
};

/**
 * @brief lgpio GPIOエッジ通知コールバック
 *
 * GPIOの状態変化が発生するたびにlgpioから呼び出される。
 *
 * @param e       GPIOイベント元
 * @param evt     GPIOイベント情報
 * @param userdata ユーザーデータ
 */
static void gpioAlertCallback(
    int e,
    lgGpioAlert_p evt,
    void* userdata)
{
    (void)e;

    if (evt == nullptr || userdata == nullptr)
    {
        return;
    }

    AlertContext* context =
        static_cast<AlertContext*>(userdata);

    std::lock_guard<std::mutex> lock(context->mutex);

    if (!context->collecting)
    {
        return;
    }

    const std::uint8_t level =
        evt->report.level;

    const std::uint32_t timestamp =
        evt->report.timestamp;

    /*
     * HIGHになった時刻を保存する。
     */
    if (level == 1)
    {
        context->highActive = true;
        context->highStart = timestamp;
    }
    /*
     * LOWになった場合、
     * HIGHになっていた時間を計算する。
     */
    else if (level == 0)
    {
        if (context->highActive)
        {
            /*
             * timestampは32bitなので、
             * unsigned演算によりタイマラップにも対応する。
             */
            const std::uint32_t pulseWidth =
                timestamp - context->highStart;

            context->highPulses.push_back(pulseWidth);

            context->highActive = false;

            /*
             * DHT11のデータは40ビット。
             *
             * 40個のHIGHパルスを取得できれば、
             * データ解析に必要なパルスが揃ったと判断する。
             */
            if (context->highPulses.size() >= 40)
            {
                context->collecting = false;
                context->condition.notify_one();
            }
        }
    }
}


/**
 * @brief コンストラクタ
 */
Dht11Sensor::Dht11Sensor(unsigned int gpioPin)
    : m_gpioPin(gpioPin),
      m_gpioHandle(-1),
      m_gpioClaimed(false)
{
}


/**
 * @brief デストラクタ
 */
Dht11Sensor::~Dht11Sensor()
{
    releaseGpio();

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
     * GPIOチップ0をオープンする。
     */
    m_gpioHandle = lgGpiochipOpen(0);

    if (m_gpioHandle < 0)
    {
        std::cerr
            << "lgGpiochipOpen failed. "
            << "GPIO=" << m_gpioPin
            << std::endl;

        return false;
    }

    /*
     * DHT11の信号線は通常HIGHで待機する。
     *
     * Python版では読み取り開始前に
     * HIGH状態で100ms待っているため、
     * ここでも同じように待つ。
     */
    if (!configureOutput(1))
    {
        return false;
    }

    std::this_thread::sleep_for(
        std::chrono::milliseconds(100));

    releaseGpio();

    std::cout
        << "DHT11 initialized. GPIO="
        << m_gpioPin
        << std::endl;

    return true;
}


/**
 * @brief GPIOを出力モードで確保する
 */
bool Dht11Sensor::configureOutput(int initialValue)
{
    if (m_gpioHandle < 0)
    {
        return false;
    }

    /*
     * 既にGPIOを確保している場合は、
     * いったん解放する。
     */
    releaseGpio();

    const int result =
        lgGpioClaimOutput(
            m_gpioHandle,
            0,
            m_gpioPin,
            initialValue);

    if (result < 0)
    {
        std::cerr
            << "lgGpioClaimOutput failed. "
            << "GPIO=" << m_gpioPin
            << " result=" << result
            << std::endl;

        return false;
    }

    m_gpioClaimed = true;

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

    if (m_gpioClaimed)
    {
        lgGpioFree(
            m_gpioHandle,
            m_gpioPin);

        m_gpioClaimed = false;
    }
}


/**
 * @brief DHT11開始信号を送信する
 */
bool Dht11Sensor::sendStartSignal()
{
    /*
     * DHT11読み取り開始時、
     * ホスト側がLOWを18ms以上出力する。
     */
    if (!configureOutput(0))
    {
        return false;
    }

    std::this_thread::sleep_for(
        std::chrono::milliseconds(18));

    return true;
}


/**
 * @brief DHT11から40ビットのデータを取得する
 */
bool Dht11Sensor::readRawData(std::uint8_t data[5])
{
    if (data == nullptr)
    {
        return false;
    }

    /*
     * DHT11開始信号を送信する。
     *
     * GPIOはLOW状態で18ms保持される。
     */
    if (!sendStartSignal())
    {
        return false;
    }

    /*
     * Python版では、
     *
     * GPIO LOW
     * ↓
     * direction = INPUT
     *
     * としてセンサへ信号線を解放している。
     *
     * ここでもLOW出力を解放する。
     */
    releaseGpio();

    AlertContext context;

    /*
     * GPIOを入力＋エッジ監視として確保する。
     *
     * DHT11からの応答を
     * LOW/HIGH両方のエッジで監視する。
     */
    const int result =
        lgGpioClaimAlert(
            m_gpioHandle,
            0,
            LG_BOTH_EDGES,
            m_gpioPin,
            -1);

    if (result < 0)
    {
        std::cerr
            << "lgGpioClaimAlert failed. "
            << "GPIO=" << m_gpioPin
            << " result=" << result
            << std::endl;

        return false;
    }

    m_gpioClaimed = true;

    /*
     * エッジ通知コールバックを登録する。
     */
    const int callbackResult =
        lgGpioSetAlertsFunc(
            m_gpioHandle,
            m_gpioPin,
            gpioAlertCallback,
            &context);

    if (callbackResult < 0)
    {
        std::cerr
            << "lgGpioSetAlertsFunc failed. "
            << "result=" << callbackResult
            << std::endl;

        releaseGpio();

        return false;
    }

    /*
     * DHT11から40個のHIGHパルスを
     * 取得するまで待つ。
     *
     * DHT11の通信全体は数ms程度なので、
     * 20msをタイムアウトとする。
     */
    {
        std::unique_lock<std::mutex> lock(
            context.mutex);

        const bool completed =
            context.condition.wait_for(
                lock,
                std::chrono::milliseconds(20),
                [&context]()
                {
                    return
                        context.highPulses.size() >= 40;
                });

        if (!completed)
        {
            context.collecting = false;

            releaseGpio();

            std::cerr
                << "DHT11 timing timeout."
                << std::endl;

            return false;
        }
    }

    /*
     * GPIO監視を終了する。
     */
    releaseGpio();

    /*
     * 最後の40個のHIGHパルスを
     * DHT11の40ビットとして解析する。
     *
     * 先頭にはDHT11の応答パルスが
     * 含まれる可能性があるため、
     * 「最後の40個」を使用する。
     */
    std::vector<std::uint32_t> pulses;

    {
        std::lock_guard<std::mutex> lock(
            context.mutex);

        if (context.highPulses.size() < 40)
        {
            return false;
        }

        const std::size_t startIndex =
            context.highPulses.size() - 40;

        pulses.assign(
            context.highPulses.begin() + startIndex,
            context.highPulses.end());
    }

    /*
     * 40ビットを5バイトへ変換する。
     */
    for (int i = 0; i < 5; ++i)
    {
        data[i] = 0;
    }

    for (int bitIndex = 0; bitIndex < 40; ++bitIndex)
    {
        /*
         * DHT11のビット判定。
         *
         * 約26～28us → 0
         * 約70us     → 1
         *
         * 中間値として40usを使用する。
         */
        const bool bitValue =
            pulses[bitIndex] > 40;

        const int byteIndex =
            bitIndex / 8;

        const int bitPosition =
            7 - (bitIndex % 8);

        if (bitValue)
        {
            data[byteIndex] |=
                static_cast<std::uint8_t>(
                    1U << bitPosition);
        }
    }

    return true;
}


/**
 * @brief DHT11チェックサムを確認する
 */
bool Dht11Sensor::checkChecksum(
    const std::uint8_t data[5]) const
{
    const std::uint8_t checksum =
        static_cast<std::uint8_t>(
            data[0]
            + data[1]
            + data[2]
            + data[3]);

    return checksum == data[4];
}


/**
 * @brief DHT11から温度・湿度を取得する
 */
bool Dht11Sensor::read(
    float& temperature,
    float& humidity)
{
    std::uint8_t data[5] = {};

    if (!readRawData(data))
    {
        return false;
    }

    /*
     * DHT11データフォーマット
     *
     * data[0] : 湿度整数部
     * data[1] : 湿度小数部
     * data[2] : 温度整数部
     * data[3] : 温度小数部
     * data[4] : チェックサム
     */
    if (!checkChecksum(data))
    {
        std::cerr
            << "DHT11 checksum error."
            << std::endl;

        return false;
    }

    humidity =
        static_cast<float>(data[0])
        + static_cast<float>(data[1]) / 10.0f;

    temperature =
        static_cast<float>(data[2])
        + static_cast<float>(data[3]) / 10.0f;

    /*
     * DHT11の仕様上、
     * 温度・湿度が異常な値になっていないか確認する。
     */
    if (temperature < -40.0f ||
        temperature > 80.0f)
    {
        return false;
    }

    if (humidity < 0.0f ||
        humidity > 100.0f)
    {
        return false;
    }

    return true;
}