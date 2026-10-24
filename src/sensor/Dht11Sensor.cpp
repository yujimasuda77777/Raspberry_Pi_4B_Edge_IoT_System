#include "sensor/Dht11Sensor.h"  // Dht11Sensorクラスの宣言を読み込む

#include <lgpio.h>      // GPIOチップの操作、GPIO入出力、GPIO確保・解放を行う
#include <chrono>       // マイクロ秒・ミリ秒単位の時間を指定する
#include <cstdint>      // 固定幅の整数型を使用する
#include <ctime>        // clock_gettime()と時刻情報を使用する
#include <iostream>     // 標準エラー出力へエラーログを出力する
#include <thread>       // 指定時間だけ処理を待機させる
#include <vector>       // GPIO状態変化とパルス幅を可変長配列に格納する


/**
 * @brief GPIO状態変化情報
 */
struct GpioTransition
{
    // GPIOの状態（0：LOW、1：HIGH）
    int level;

    // 状態変化が発生した時刻（マイクロ秒）
    std::uint64_t timestamp;
};

/**
 * @brief 単調増加時計から現在時刻をマイクロ秒単位で取得する
 *
 * @return 現在時刻（マイクロ秒）
 */
static std::uint64_t getMonotonicTimeUs()
{
    // 秒とナノ秒で時刻を取得するための構造体
    struct timespec timeSpec{};

    // 単調増加時計から現在時刻を取得する
    clock_gettime(CLOCK_MONOTONIC, &timeSpec);

    // 秒とナノ秒をマイクロ秒に換算して合算する
    return static_cast<std::uint64_t>(timeSpec.tv_sec) * 1000000ULL
        + static_cast<std::uint64_t>(timeSpec.tv_nsec) / 1000ULL;
}

/**
 * @brief コンストラクタ
 */
Dht11Sensor::Dht11Sensor(unsigned int gpioPin)
    // GPIO番号、GPIOチップのハンドル、GPIOの確保状態を初期化する
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

    // GPIOチップのハンドルが有効な場合は閉じる
    if (m_gpioHandle >= 0)
    {
        lgGpiochipClose(m_gpioHandle);

        // 解放済みを示す値に更新する
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

    // GPIOチップをオープンできなかった場合はエラーを出力する
    if (m_gpioHandle < 0)
    {
        std::cerr
            << "lgGpiochipOpen failed. "
            << "result="
            << m_gpioHandle
            << std::endl;

        return false;
    }

    // GPIOを出力に設定し、HIGHを出力する
    if (!configureOutput(1))
    {
        return false;
    }

    // 信号線をHIGHにした状態で100ミリ秒待機する
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // GPIOの確保を解除する
    releaseGpio();

    // 初期化結果と使用するGPIO番号を出力する
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
    // GPIOチップのハンドルが無効な場合は処理を終了する
    if (m_gpioHandle < 0)
    {
        return false;
    }

    // 既に確保しているGPIOを解放する
    releaseGpio();

    // GPIOを出力として確保し、初期出力値を設定する
    const int result = lgGpioClaimOutput(
        m_gpioHandle,
        0,
        m_gpioPin,
        initialValue);

    // GPIOの確保に失敗した場合はエラー情報を出力する
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

    // GPIOを確保済みの状態にする
    m_gpioClaimed = true;

    return true;
}

/**
 * @brief GPIOを入力として確保する
 */
bool Dht11Sensor::configureInput()
{
    // GPIOチップのハンドルが無効な場合は処理を終了する
    if (m_gpioHandle < 0)
    {
        return false;
    }

    // 既に確保しているGPIOを解放する
    releaseGpio();

    // GPIOを入力として確保する
    const int result = lgGpioClaimInput(
        m_gpioHandle,
        0,
        m_gpioPin);

    // GPIOの確保に失敗した場合はエラー情報を出力する
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

    // GPIOを確保済みの状態にする
    m_gpioClaimed = true;

    return true;
}

/**
 * @brief GPIOを解放する
 */
void Dht11Sensor::releaseGpio()
{
    // GPIOチップのハンドルが無効な場合は処理を終了する
    if (m_gpioHandle < 0)
    {
        return;
    }

    // GPIOを確保している場合は解放する
    if (m_gpioClaimed)
    {
        lgGpioFree(m_gpioHandle, m_gpioPin);

        // GPIOの確保状態を未確保に更新する
        m_gpioClaimed = false;
    }
}

/**
 * @brief DHT11から40bitの生データを取得する
 */
bool Dht11Sensor::readRawData(std::uint8_t data[5])
{
    // データ格納先が指定されていない場合は処理を終了する
    if (data == nullptr)
    {
        return false;
    }

    // データ格納用の5バイトを0で初期化する
    for (int i = 0; i < 5; ++i)
    {
        data[i] = 0;
    }

    // GPIOを出力LOWに設定する
    if (!configureOutput(0))
    {
        return false;
    }

    // LOWを18ミリ秒保持してDHT11に応答を要求する
    std::this_thread::sleep_for(std::chrono::milliseconds(18));

    // GPIOの出力設定を解除する
    releaseGpio();

    // GPIOを入力に設定してDHT11からの信号を読み取る
    if (!configureInput())
    {
        return false;
    }

    // GPIOの状態変化を時刻とともに記録する配列を用意する
    std::vector<GpioTransition> transitions;

    // 状態変化を最大100個記録できる容量を確保する
    transitions.reserve(100);

    // GPIO状態の監視を開始する時刻を取得する
    const std::uint64_t startTime = getMonotonicTimeUs();

    // GPIOの監視時間を250ミリ秒に設定する
    constexpr std::uint64_t TIMEOUT_US = 250000ULL;

    // GPIOの初期状態をHIGHとして設定する
    int previousLevel = 1;

    // タイムアウトまたは記録数の上限に達するまでGPIOを監視する
    while (true)
    {
        // 現在時刻を取得する
        const std::uint64_t currentTime = getMonotonicTimeUs();

        // 監視開始から250ミリ秒経過した場合はループを終了する
        if ((currentTime - startTime) >= TIMEOUT_US)
        {
            break;
        }

        // GPIOの現在の状態を読み取る
        const int level = lgGpioRead(m_gpioHandle, m_gpioPin);

        // GPIOの読み取りに失敗した場合はエラーを出力する
        if (level < 0)
        {
            std::cerr
                << "lgGpioRead failed. "
                << "result="
                << level
                << std::endl;

            // GPIOを解放して読み取り処理を終了する
            releaseGpio();

            return false;
        }

        // 前回の読み取りからGPIO状態が変化した場合に記録する
        if (level != previousLevel)
        {
            // 状態変化情報を生成する
            GpioTransition transition{};

            // 変化後のGPIOレベルを記録する
            transition.level = level;

            // 状態変化を検出した時刻を記録する
            transition.timestamp = currentTime;

            // 状態変化情報を配列の末尾に追加する
            transitions.push_back(transition);

            // 次回の比較に使用するGPIOレベルを更新する
            previousLevel = level;

            // 状態変化の記録数が100個に達した場合は監視を終了する
            if (transitions.size() >= 100)
            {
                break;
            }
        }
    }

    // GPIOの監視が終了したため、GPIOを解放する
    releaseGpio();

    // 状態変化の記録が2個未満の場合は読み取り失敗とする
    if (transitions.size() < 2)
    {
        std::cerr
            << "DHT11 timing timeout."
            << std::endl;

        return false;
    }

    // GPIO状態の変化履歴からHIGHパルス幅を保存する配列を用意する
    std::vector<std::uint64_t> highPulses;

    // 40bit分のパルス幅を記録できる容量を確保する
    highPulses.reserve(50);

    // HIGHパルスの計測中かどうかを管理する
    bool highActive = false;

    // HIGHになった時刻を保持する
    std::uint64_t highStart = 0;

    // 記録したGPIO状態変化を先頭から順に処理する
    for (const auto& transition : transitions)
    {
        // GPIOがHIGHになった場合はパルスの開始時刻を記録する
        if (transition.level == 1)
        {
            highActive = true;
            highStart = transition.timestamp;
        }
        // GPIOがLOWになった場合はHIGHパルスの終了時刻として扱う
        else if (transition.level == 0)
        {
            // HIGHパルスの開始時刻が記録されている場合に幅を計算する
            if (highActive)
            {
                // HIGH開始からLOWへの変化までの経過時間を計算する
                const std::uint64_t pulseWidth =
                    transition.timestamp - highStart;

                // 計算したパルス幅を配列に追加する
                highPulses.push_back(pulseWidth);

                // HIGHパルスの計測状態を解除する
                highActive = false;
            }
        }
    }

    // HIGHパルスが40個未満の場合はデータ不足として処理を終了する
    if (highPulses.size() < 40)
    {
        std::cerr
            << "DHT11 timing timeout. "
            << "HIGH pulses="
            << highPulses.size()
            << std::endl;

        return false;
    }

    // HIGHパルス配列の末尾40個を取り出すための開始位置を求める
    const std::size_t startIndex = highPulses.size() - 40;

    // 40個のHIGHパルスを順に処理して5バイトのデータへ変換する
    for (int bitIndex = 0; bitIndex < 40; ++bitIndex)
    {
        // 今回処理するビットに対応するHIGHパルス幅を取得する
        const std::uint64_t pulseWidth =
            highPulses[
                startIndex +
                static_cast<std::size_t>(bitIndex)];

        // HIGHパルス幅が40マイクロ秒を超えていれば1、そうでなければ0と判定する
        const bool bitValue = pulseWidth > 40;

        // ビット番号から格納先のバイト番号を求める
        const int byteIndex = bitIndex / 8;

        // バイト内の格納位置を上位ビットから順に求める
        const int bitPosition = 7 - (bitIndex % 8);

        // 判定結果が1の場合は対応するビットを立てる
        if (bitValue)
        {
            data[byteIndex] |= static_cast<std::uint8_t>(
                1U << bitPosition);
        }
    }

    // 40bitのデータ取得が完了したことを通知する
    return true;
}

/**
 * @brief DHT11チェックサムを確認する
 */
bool Dht11Sensor::checkChecksum(const std::uint8_t data[5]) const
{
    // 先頭4バイトを加算して8ビットのチェックサム値を計算する
    const std::uint8_t checksum = static_cast<std::uint8_t>(
        data[0]
        + data[1]
        + data[2]
        + data[3]);

    // 計算した値と5バイト目のチェックサムを比較する
    return checksum == data[4];
}

/**
 * @brief DHT11から温度・湿度を取得する
 */
bool Dht11Sensor::read(float& temperature, float& humidity)
{
    // センサーから受信する5バイトのデータを0で初期化する
    std::uint8_t data[5] = {};

    // DHT11から40bitのデータを取得する
    if (!readRawData(data))
    {
        return false;
    }

    // 受信データのチェックサムを検証する
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

    // 湿度の整数部と小数部を合算して湿度を求める
    humidity =
        static_cast<float>(data[0])
        +
        static_cast<float>(data[1]) / 10.0f;

    // 温度の整数部と小数部を合算して温度を求める
    temperature =
        static_cast<float>(data[2])
        +
        static_cast<float>(data[3]) / 10.0f;

    // 温度が想定範囲外の場合はエラーとする
    if (temperature < -40.0f ||
        temperature > 80.0f)
    {
        std::cerr
            << "DHT11 temperature range error."
            << std::endl;

        return false;
    }

    // 湿度が0～100%の範囲外の場合はエラーとする
    if (humidity < 0.0f ||
        humidity > 100.0f)
    {
        std::cerr
            << "DHT11 humidity range error."
            << std::endl;

        return false;
    }

    // 温度と湿度の取得および検証が完了したことを通知する
    return true;
}
