#ifndef DHT11_SENSOR_H
#define DHT11_SENSOR_H

/**
 * @file Dht11Sensor.h
 * @brief DHT11温湿度センサクラスの定義
 */

class Dht11Sensor
{

public:
	
	 /**
     * @brief コンストラクタ
     * @param gpioPin DHT11を接続するGPIO番号（BCM番号）
     */
	explicit Dht11Sensor(int gpioPin);
	
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
    /** DHT11を接続するGPIO番号（BCM番号） */
    int m_gpioPin;
	
};

#endif // DHT11_SENSOR_H