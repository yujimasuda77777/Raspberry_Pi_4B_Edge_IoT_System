#include "sensor/Dht11Sensor.h"  // Dht11Sensorクラスの宣言を読み込む

#include <lgpio.h>      // GPIOチップの操作、GPIO入出力、GPIO確保・解放を行う
#include <chrono>       // マイクロ秒・ミリ秒単位の時間を指定する
#include <cstdint>      // 固定幅の整数型を使用する
#include <ctime>        // clock_gettime()と時刻情報を使用する
#include <iostream>     // 標準エラー出力へエラーログを出力する
#include <thread>       // 指定時間だけ処理を待機させる
#include <vector>       // GPIO状態変化とパルス幅を可変長配列に格納する

// GPIOの状態と、その状態に変化した時刻を保持する構造体
struct GpioTransition
{
    // GPIOの電圧レベル（0 = LOW、1 = HIGH）
    int level;

    // GPIOの状態変化が発生した時刻（マイクロ秒）
    std::uint64_t timestamp;
};

/**
 * @brief 現在時刻をマイクロ秒で取得する
 *
 * @return 現在時刻[us]
 */
static std::uint64_t getMonotonicTimeUs()
{
    // 秒とナノ秒を格納する時刻構造体を初期化する
    struct timespec timeSpec{};

    // システム起動後の経過時間を取得する
    clock_gettime(CLOCK_MONOTONIC, &timeSpec);

    // 秒をマイクロ秒へ変換し、ナノ秒から変換した値を加算する
    return static_cast<std::uint64_t>(timeSpec.tv_sec) * 1000000ULL
        + static_cast<std::uint64_t>(timeSpec.tv_nsec) / 1000ULL;
}

/**
 * @brief コンストラクタ
 */
Dht11Sensor::Dht11Sensor(unsigned int gpioPin)
    // GPIO番号、未オープン状態、未確保状態を設定する
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
    // 確保中のGPIOを解放する
    releaseGpio();

    // GPIOチップがオープンされている場合は閉じる
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
    // GPIOチップ0をオープンする
    m_gpioHandle = lgGpiochipOpen(0);

    // GPIOチップのオープンに失敗した場合はエラーを出力する
    if (m_gpioHandle < 0)
    {
        std::cerr
            << "lgGpiochipOpen failed. "
            << "result="
            << m_gpioHandle
            << std::endl;
        return false;
    }

    // GPIOを出力として確保し、HIGHに設定する
    if (!configureOutput(1))
    {
        return false;
    }

    // 信号線をHIGHにした状態で100ms待機する
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // GPIOの確保を解放する
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
    // GPIOチップがオープンされていない場合は失敗とする
    if (m_gpioHandle < 0)
    {
        return false;
    }

    // 既に確保されているGPIOを解放する
    releaseGpio();

    // 指定した初期値でGPIOを出力として確保する
    const int result = lgGpioClaimOutput(
        m_gpioHandle,
        0,
        m_gpioPin,
        initialValue);

    // GPIOの確保に失敗した場合はエラーを出力する
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

    // GPIOを確保済みの状態に設定する
    m_gpioClaimed = true;

    return true;
}

/**
 * @brief GPIOを入力として確保する
 */
bool Dht11Sensor::configureInput()
{
    // GPIOチップがオープンされていない場合は失敗とする
    if (m_gpioHandle < 0)
    {
        return false;
    }

    // 既に確保されているGPIOを解放する
    releaseGpio();

    // GPIOを入力として確保する
    const int result = lgGpioClaimInput(
        m_gpioHandle,
        0,
        m_gpioPin);

    // GPIOの確保に失敗した場合はエラーを出力する
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

    // GPIOを確保済みの状態に設定する
    m_gpioClaimed = true;

    return true;
}

/**
 * @brief GPIOを解放する
 */
void Dht11Sensor::releaseGpio()
{
    // GPIOチップがオープンされていない場合は何もせずに戻る
    if (m_gpioHandle < 0)
    {
        return;
    }

    // GPIOを確保している場合は解放する
    if (m_gpioClaimed)
    {
        lgGpioFree(m_gpioHandle, m_gpioPin);

        // GPIOを未確保の状態に設定する
        m_gpioClaimed = false;
    }
}

/**
 * @brief DHT11から40bitの生データを取得する
 */
bool Dht11Sensor::readRawData(std::uint8_t data[5])
{
    // データ格納先が無効な場合は失敗とする
    if (data == nullptr)
    {
        return false;
    }

    // 受信データを格納する5バイトを0で初期化する
    for (int i = 0; i < 5; ++i)
    {
        data[i] = 0;
    }

    // GPIOを出力LOWに設定する
    if (!configureOutput(0))
    {
        return false;
    }

    // LOW状態を18ms保持してセンサーへ通信開始を通知する
    std::this_thread::sleep_for(std::chrono::milliseconds(18));

    // GPIOの確保を解放してから入力として確保し直す
    releaseGpio();

    if (!configureInput())
    {
        return false;
    }

    // GPIO状態変化の記録先を用意する
    std::vector<GpioTransition> transitions;

    // 状態変化情報を格納する領域をあらかじめ確保する
    transitions.reserve(100);

    // GPIO状態の監視開始時刻を取得する
    const std::uint64_t startTime = getMonotonicTimeUs();

    // GPIO状態の監視タイムアウトを250msに設定する
    constexpr std::uint64_t TIMEOUT_US = 250000ULL;

    // 監視開始時点のGPIO状態をHIGHとして設定する
    int previousLevel = 1;

    // GPIO状態の変化を監視する
    while (true)
    {
        // 現在時刻を取得する
        const std::uint64_t currentTime = getMonotonicTimeUs();

        // 監視開始から250ms経過した場合はループを終了する
        if ((currentTime - startTime) >= TIMEOUT_US)
        {
            break;
        }

        // GPIOの現在のレベルを読み取る
        const int level = lgGpioRead(m_gpioHandle, m_gpioPin);

        // GPIOの読み取りに失敗した場合はエラーを出力する
        if (level < 0)
        {
            std::cerr
                << "lgGpioRead failed. "
                << "result="
                << level
                << std::endl;

            // GPIOを解放してから失敗を返す
            releaseGpio();

            return false;
        }

        // 前回読み取った状態と異なる場合は状態変化を記録する
        if (level != previousLevel)
        {
            // 状態変化後のレベルと発生時刻を記録する
            GpioTransition transition{};

            transition.level = level;
            transition.timestamp = currentTime;

            // 状態変化情報を配列へ追加する
            transitions.push_back(transition);

            // 次回比較用のGPIO状態を更新する
            previousLevel = level;

            // 状態変化の記録数が100件に達した場合は監視を終了する
            if (transitions.size() >= 100)
            {
                break;
            }
        }
    }

    // GPIO状態の監視終了後にGPIOを解放する
    releaseGpio();

    // 記録された状態変化が2件未満の場合は失敗とする
    if (transitions.size() < 2)
    {
        std::cerr
            << "DHT11 timing timeout."
            << std::endl;

        return false;
    }

    // HIGHパルス幅を格納する配列を用意する
    std::vector<std::uint64_t> highPulses;

    // 40bit分のパルス幅を格納できる領域を確保する
    highPulses.reserve(50);

    // HIGHパルスの計測中かどうかを管理する
    bool highActive = false;

    // HIGHになった時刻を保持する
    std::uint64_t highStart = 0;

    // 記録したGPIO状態変化を順番に処理する
    for (const auto& transition : transitions)
    {
        // GPIOがHIGHになった場合はパルス開始時刻を記録する
        if (transition.level == 1)
        {
            highActive = true;
            highStart = transition.timestamp;
        }
        // GPIOがLOWになった場合はHIGHパルス幅を計算する
        else if (transition.level == 0)
        {
            // HIGHパルスの計測中であればパルス幅を記録する
            if (highActive)
            {
                const std::uint64_t pulseWidth =
                    transition.timestamp - highStart;

                highPulses.push_back(pulseWidth);

                // HIGHパルスの計測状態を解除する
                highActive = false;
            }
        }
    }

    // 40bit分のHIGHパルスが取得できていない場合は失敗とする
    if (highPulses.size() < 40)
    {
        std::cerr
            << "DHT11 timing timeout. "
            << "HIGH pulses="
            << highPulses.size()
            << std::endl;

        return false;
    }

    // 取得したパルス列の末尾40個をデータ変換対象にする
    const std::size_t startIndex = highPulses.size() - 40;

    // 40個のHIGHパルスを順番に処理してデータへ変換する
    for (int bitIndex = 0; bitIndex < 40; ++bitIndex)
    {
        // 対象となるHIGHパルスの幅を取得する
        const std::uint64_t pulseWidth =
            highPulses[
                startIndex +
                static_cast<std::size_t>(bitIndex)];

        // HIGHパルス幅が40usを超える場合は1、それ以外は0とする
        const bool bitValue = pulseWidth > 40;

        // 対象ビットを格納するバイト番号を計算する
        const int byteIndex = bitIndex / 8;