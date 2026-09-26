#include "sensor/Dht11Sensor.h"

#include <lgpio.h>

#include <chrono>
#include <cstdint>
#include <ctime>
#include <iostream>
#include <thread>
#include <vector>

/**
 * @brief GPIO状態変化情報
 */
struct GpioTransition
{
    /**
     * @brief GPIOレベル
     *
     * 0 = LOW
     * 1 = HIGH
     */
    int level;

    /**
     * @brief 状態変化が発生した時刻[us]
     */
    std::uint64_t timestamp;
};

/**
 * @brief 現在時刻をマイクロ秒で取得する
 *
 * Python版のtime.monotonic()に相当する。
 *
 * @return 現在時刻[us]
 */
static std::uint64_t getMonotonicTimeUs()
{
    struct timespec timeSpec{};

    clock_gettime(CLOCK_MONOTONIC, &timeSpec);

    return static_cast<std::uint64_t>(timeSpec.tv_sec) * 1000000ULL
        + static_cast<std::uint64_t>(timeSpec.tv_nsec) / 1000ULL;
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
     *
     * Raspberry Pi 4Bでは
     * BCM GPIO14はgpiochip0に存在する。
     */
    m_gpioHandle = lgGpiochipOpen(0);

    if (m_gpioHandle < 0)
    {
        std::cerr
            << "lgGpiochipOpen failed. "
            << "result="
            << m_gpioHandle
            << std::endl;

        return false;
    }

    /*
     * DHT11の信号線は通常HIGHで待機する。
     */
    if (!configureOutput(1))
    {
        return false;
    }

    /*
     * Python版でも読み取り前に
     * HIGH状態を100ms保持している。
     *
     * ここでも同じように100ms待つ。
     */
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    releaseGpio();

    std::cout
        << "DHT11 initialized. GPIO="
        << m_gpioPin
        << std::endl;

    return true;
}

/**
 * @brief GPIOを出力として確保する
 */
bool Dht11Sensor::configureOutput(int initialValue)
{
    if (m_gpioHandle < 0)
    {
        return false;
    }

    /*
     * 既にGPIOを確保している場合は、
     * 一度解放する。
     */
    releaseGpio();

    const int result = lgGpioClaimOutput(
        m_gpioHandle,
        0,
        m_gpioPin,
        initialValue);

    if (result < 0)
    {
        std::cerr
            << "lgGpioClaimOutput failed. "
            << "GPIO="
            << m_gpioPin
            << " result="
            << result
            << std::endl;

        return false;
    }

    m_gpioClaimed = true;

    return true;
}

/**
 * @brief GPIOを入力として確保する
 */
bool Dht11Sensor::configureInput()
{
    if (m_gpioHandle < 0)
    {
        return false;
    }

    /*
     * 出力として確保されていた場合は
     * 一度解放する。
     */
    releaseGpio();

    const int result = lgGpioClaimInput(
        m_gpioHandle,
        0,
        m_gpioPin);

    if (result < 0)
    {
        std::cerr
            << "lgGpioClaimInput failed. "
            << "GPIO="
            << m_gpioPin
            << " result="
            << result
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
        lgGpioFree(m_gpioHandle, m_gpioPin);

        m_gpioClaimed = false;
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

    /*
     * 5バイトを初期化する。
     */
    for (int i = 0; i < 5; ++i)
    {
        data[i] = 0;
    }

    /*
     * --------------------------------------------------
     * 1. GPIOを出力LOWにする
     * --------------------------------------------------
     */
    if (!configureOutput(0))
    {
        return false;
    }

    /*
     * --------------------------------------------------
     * 2. LOWを18ms保持する
     *
     * Python版の
     *
     * dhtpin.value = False
     * time.sleep(self._trig_wait / 1000000)
     *
     * に相当する。
     * --------------------------------------------------
     */
    std::this_thread::sleep_for(std::chrono::milliseconds(18));

    /*
     * --------------------------------------------------
     * 3. GPIOを解放する
     *
     * Python版では、
     *
     * dhtpin.direction = Direction.INPUT
     *
     * によってセンサ側へ信号線を渡している。
     *
     * ここではlgpioのGPIO確保を解放した後、
     * 入力として再確保する。
     * --------------------------------------------------
     */
    releaseGpio();

    if (!configureInput())
    {
        return false;
    }

    /*
     * --------------------------------------------------
     * 4. GPIO状態を高速ポーリングする
     *
     * ここが今回の重要部分。
     *
     * Python版は、
     *
     * while time.monotonic() - timestamp < 0.25:
     *     if dhtval != dhtpin.value:
     *         ...
     *
     * としている。
     *
     * C++でもsleep_for(1us)などは入れず、
     * GPIO状態を連続して読み取る。
     * --------------------------------------------------
     */

    std::vector<GpioTransition> transitions;

    transitions.reserve(100);

    /*
     * 読み取り開始時刻。
     */
    const std::uint64_t startTime = getMonotonicTimeUs();

    /*
     * Python版と同じく、
     * 最大250ms監視する。
     */
    constexpr std::uint64_t TIMEOUT_US = 250000ULL;

    /*
     * 現在認識しているGPIO状態。
     *
     * DHT11は待機時HIGHなので、
     * 最初はHIGHとする。
     */
    int previousLevel = 1;

    while (true)
    {
        const std::uint64_t currentTime = getMonotonicTimeUs();

        /*
         * 250msを超えたらタイムアウト。
         */
        if ((currentTime - startTime) >= TIMEOUT_US)
        {
            break;
        }

        /*
         * GPIO状態を取得する。
         */
        const int level = lgGpioRead(m_gpioHandle, m_gpioPin);

        if (level < 0)
        {
            std::cerr
                << "lgGpioRead failed. "
                << "result="
                << level
                << std::endl;

            releaseGpio();

            return false;
        }

        /*
         * GPIO状態が変化した場合だけ、
         * 変化した時刻を記録する。
         */
        if (level != previousLevel)
        {
            GpioTransition transition{};

            transition.level = level;
            transition.timestamp = currentTime;

            transitions.push_back(transition);

            previousLevel = level;

            /*
             * DHT11の通信は、
             *
             * 応答
             * + 40bit
             *
             * なので80～90程度のエッジが
             * 取得できれば十分。
             *
             * 余計な待ち時間を減らすため、
             * 100個を上限とする。
             */
            if (transitions.size() >= 100)
            {
                break;
            }
        }
    }

    releaseGpio();

    /*
     * エッジが少なすぎる場合は失敗。
     */
    if (transitions.size() < 2)
    {
        std::cerr
            << "DHT11 timing timeout."
            << std::endl;

        return false;
    }

    /*
     * --------------------------------------------------
     * 5. HIGHパルス幅を取り出す
     *
     * DHT11のデータは、
     *
     * LOW 約50us
     * HIGH 約26～28us → 0
     * HIGH 約70us     → 1
     *
     * という形式。
     *
     * HIGHになった時刻から
     * 次にLOWになった時刻までを測定する。
     * --------------------------------------------------
     */
    std::vector<std::uint64_t> highPulses;

    highPulses.reserve(50);

    bool highActive = false;
    std::uint64_t highStart = 0;

    for (const auto& transition : transitions)
    {
        /*
         * LOW → HIGH
         */
        if (transition.level == 1)
        {
            highActive = true;
            highStart = transition.timestamp;
        }
        /*
         * HIGH → LOW
         */
        else if (transition.level == 0)
        {
            if (highActive)
            {
                const std::uint64_t pulseWidth =
                    transition.timestamp - highStart;

                highPulses.push_back(pulseWidth);

                highActive = false;
            }
        }
    }

    /*
     * DHT11の40bit分のHIGHパルスが
     * 必要。
     */
    if (highPulses.size() < 40)
    {
        std::cerr
            << "DHT11 timing timeout. "
            << "HIGH pulses="
            << highPulses.size()
            << std::endl;

        return false;
    }

    /*
     * --------------------------------------------------
     * 6. 最後の40個を使用する
     *
     * Python版でも取得したtransitionの
     * 後半を使用する実装になっている。
     *
     * DHT11の応答開始部分を取りこぼした場合にも、
     * データ部分を利用しやすくする。
     * --------------------------------------------------
     */
    const std::size_t startIndex = highPulses.size() - 40;

    /*
     * --------------------------------------------------
     * 7. 40bitを5バイトへ変換する
     * --------------------------------------------------
     */
    for (int bitIndex = 0; bitIndex < 40; ++bitIndex)
    {
        const std::uint64_t pulseWidth =
            highPulses[
                startIndex +
                static_cast<std::size_t>(bitIndex)];

        /*
         * 0:
         *   約26～28us
         *
         * 1:
         *   約70us
         *
         * 40usを境界値とする。
         */
        const bool bitValue = pulseWidth > 40;

        const int byteIndex = bitIndex / 8;

        const int bitPosition = 7 - (bitIndex % 8);

        if (bitValue)
        {
            data[byteIndex] |= static_cast<std::uint8_t>(
                1U << bitPosition);
        }
    }

    return true;
}

/**
 * @brief DHT11チェックサムを確認する
 */
bool Dht11Sensor::checkChecksum(const std::uint8_t data[5]) const
{
    const std::uint8_t checksum = static_cast<std::uint8_t>(
        data[0]
        + data[1]
        + data[2]
        + data[3]);

    return checksum == data[4];
}

/**
 * @brief DHT11から温度・湿度を取得する
 */
bool Dht11Sensor::read(float& temperature, float& humidity)
{
    std::uint8_t data[5] = {};

    /*
     * DHT11から40bitを取得する。
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
        std::cerr
            << "DHT11 checksum error."
            << std::endl;

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
    humidity =
        static_cast<float>(data[0])
        +
        static_cast<float>(data[1]) / 10.0f;

    temperature =
        static_cast<float>(data[2])
        +
        static_cast<float>(data[3]) / 10.0f;

    /*
     * 値の範囲確認。
     */
    if (temperature < -40.0f ||
        temperature > 80.0f)
    {
        std::cerr
            << "DHT11 temperature range error."
            << std::endl;

        return false;
    }

    if (humidity < 0.0f ||
        humidity > 100.0f)
    {
        std::cerr
            << "DHT11 humidity range error."
            << std::endl;

        return false;
    }

    return true;
}