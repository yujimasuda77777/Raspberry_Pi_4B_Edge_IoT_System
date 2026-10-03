# 08_04 センサ処理設計

## 1. 目的

本書では、Raspberry Pi 4B上で動作するSensor Processのプログラム設計を定義する。

対象は以下の2つである。

* `Dht11Sensor`
* `Sensor Process`

DHT11から温度・湿度を取得し、`SensorData`を生成してMessage Queueへ送信するまでの処理を定義する。

---

## 2. 対象ファイル

センサ処理に関係するファイルは以下のとおりとする。

```text
include/
├── common/
│   └── SensorData.h
│
└── sensor/
    └── Dht11Sensor.h

src/
└── sensor/
    ├── main.cpp
    └── Dht11Sensor.cpp
```

---

## 3. センサ処理の役割分担

センサ処理では、DHT11を直接扱う部分と、プロセス全体を制御する部分を分離する。

### Dht11Sensor

DHT11そのものを扱う。

主な責務：

* GPIOの初期化
* DHT11からのデータ取得
* 温度の取得
* 湿度の取得
* センサ読み取り結果の返却
* センサ終了処理

### Sensor Process

センサ処理全体を制御する。

主な責務：

* Dht11Sensorの初期化
* SensorDataの生成
* `data_id`の生成
* `timestamp`の取得
* Message Queueの初期化
* SensorDataの送信
* 一定周期での繰り返し
* センサ読み取り失敗時の処理

---

## 4. Dht11Sensorの責務

`Dht11Sensor`は、DHT11からデータを取得するためのクラスである。

Sensor Processからは、DHT11の細かな通信方法を意識せずに使用できる構成とする。

イメージは以下のとおり。

```text
Sensor Process
      │
      │ 温度・湿度を取得
      ▼
Dht11Sensor
      │
      │ GPIOを使用
      ▼
    DHT11
```

Sensor Processは、

「DHT11の通信処理をどう実装しているか」

を意識しない。

---

## 5. Dht11Sensorの初期化

Sensor Processの起動時に、Dht11Sensorを初期化する。

初期化では、主に以下を行う。

1. GPIO番号を設定する
2. GPIOを使用可能な状態にする
3. DHT11の読み取りに必要な状態を準備する

使用するGPIOは以下とする。

```text
BCM GPIO14
```

初期化に失敗した場合、Sensor Processはセンサ読み取りを開始しない。

---

## 6. Dht11Sensorのデータ取得

Dht11Sensorは、DHT11から以下の2つの値を取得する。

```text
温度
湿度
```

単位は以下とする。

| データ         | 単位  |
| ----------- | --- |
| temperature | ℃   |
| humidity    | %RH |

取得した値は、Sensor Processから`SensorData`へ格納する。

---

## 7. SensorDataの生成

センサから取得した温度・湿度を使用して、Sensor Processが`SensorData`を生成する。

`SensorData`は以下の情報を持つ。

```text
temperature
humidity
timestamp
data_id
```

データの生成順序は以下とする。

```text
DHT11
  ↓
温度・湿度取得
  ↓
SensorData生成
  ↓
data_id設定
  ↓
timestamp設定
  ↓
完成したSensorData
```

---

## 8. data_idの生成

`data_id`はSensor Processが生成する。

基本的には、センサデータを生成するたびに値を増加させる。

例：

```text
1
2
3
4
5
...
```

`data_id`は、同じセンサデータを識別するために使用する。

特にCommunication Processで再送が発生した場合、同じ`data_id`を使用する。

そのため、再送時に新しい`data_id`を生成してはいけない。

```text
1回目
data_id = 10
       ↓
HTTP送信失敗
       ↓
再送
data_id = 10
```

この仕組みにより、同一データを識別できる。

---

## 9. timestampの設定

`timestamp`は、センサデータを取得した時点でSensor Processが設定する。

単位はUTC Unix秒とする。

```text
timestamp = センサ取得時刻
```

その後、Message QueueやHTTP通信を行っても、基本的にこの値は変更しない。

つまり、

```text
DHT11取得
  ↓
timestamp設定
  ↓
Message Queue
  ↓
HTTP POST
  ↓
Cloudflare Worker
  ↓
D1
```

という流れで、同じ`timestamp`を引き継ぐ。

---

## 10. センサ取得周期

Sensor Processは、5秒周期でDHT11からデータを取得する。

基本的な処理は以下のとおり。

```text
起動
 ↓
DHT11初期化
 ↓
Message Queue初期化
 ↓
センサ読み取り
 ↓
SensorData生成
 ↓
Message Queueへ送信
 ↓
5秒待機
 ↓
センサ読み取り
 ↓
...
```

---

## 11. センサ読み取り成功時

DHT11の読み取りに成功した場合、取得した温度・湿度をSensorDataへ格納する。

その後、

```text
data_id
timestamp
temperature
humidity
```

が設定されたSensorDataをMessage Queueへ送信する。

---

## 12. センサ読み取り失敗時

DHT11の読み取りに失敗した場合、その周期のデータは送信しない。

処理は以下とする。

```text
DHT11読み取り
      │
      ├─ 成功 → SensorData生成 → Queue送信
      │
      └─ 失敗 → ログ出力 → 次の周期へ
```

Sensor Process自体は終了しない。

次の5秒周期で、再度DHT11の読み取りを行う。

---

## 13. Message Queue送信失敗時

SensorDataをMessage Queueへ送信できなかった場合は、エラーをログへ出力する。

Sensor Processは停止せず、次の周期の処理を継続する。

基本的な考え方は以下とする。

```text
SensorData生成
      ↓
Queue送信
      │
      ├─ 成功 → 次の周期
      │
      └─ 失敗 → ログ出力 → 次の周期
```

Sensor Processでは、Queue送信のために無期限に待ち続ける処理は行わない。

---

## 14. Sensor Processのメイン処理

Sensor Processの`main.cpp`では、センサ処理全体を制御する。

大まかな処理順序は以下とする。

```text
Sensor Process起動
      ↓
Dht11Sensor初期化
      ↓
Message Queue初期化
      ↓
5秒周期処理
      ↓
DHT11読み取り
      ↓
SensorData生成
      ↓
Message Queue送信
      ↓
5秒待機
      ↓
繰り返し
```

---

## 15. main.cppとDht11Sensor.cppの役割

### main.cpp

Sensor Process全体を制御する。

担当する処理：

* プログラム起動
* Dht11Sensor生成
* Message Queue生成
* センサ読み取りの呼び出し
* SensorData生成
* `data_id`設定
* `timestamp`設定
* Queue送信
* 周期処理

### Dht11Sensor.cpp

DHT11を直接扱う処理を実装する。

担当する処理：

* GPIO処理
* DHT11通信処理
* 温度取得
* 湿度取得
* センサ終了処理

---

## 16. Sensor Processから見たDht11Sensor

Sensor Processは、Dht11Sensorの内部処理を直接扱わない。

イメージとしては以下とする。

```text
Sensor Process

「温度と湿度をください」
        │
        ▼
   Dht11Sensor
        │
        ▼
      DHT11
        │
        ▼
   温度・湿度
        │
        ▼
Sensor Process
```

これにより、Sensor Processの処理を単純にする。

---

## 17. センサ処理で扱うデータ

センサ処理では、以下のデータを扱う。

| 項目          | 内容                      |
| ----------- | ----------------------- |
| temperature | DHT11から取得した温度           |
| humidity    | DHT11から取得した湿度           |
| timestamp   | センサ取得時のUTC Unix秒        |
| data_id     | Sensor Processが生成する識別番号 |

この4項目を1つの`SensorData`として扱う。

---

## 18. センサ処理の責務範囲

Sensor Processは以下を担当する。

```text
DHT11
 ↓
センサ値取得
 ↓
SensorData生成
 ↓
data_id設定
 ↓
timestamp設定
 ↓
Message Queue送信
```

一方、以下はSensor Processの責務ではない。

* HTTP通信
* HTTPS通信
* Cloudflare Workerとの通信
* D1への保存
* JSON通信データの作成
* HTTPリトライ
* Cloudflareの認証処理

これらはCommunication Process側で担当する。

---

## 19. センサ処理の終了

Sensor Processを終了する場合は、使用していたリソースを適切に解放する。

主な対象は以下とする。

```text
Dht11Sensor
Message Queue
GPIO関連リソース
```

正常終了時には、必要な終了処理を行ってからプロセスを終了する。

---

## 20. 設計上のポイント

センサ処理では、以下を基本方針とする。

### 20.1 センサ処理を単純にする

Sensor Processは、

```text
読む
↓
SensorDataを作る
↓
送る
```

ことに集中する。

### 20.2 通信処理を持たせない

Sensor Processから直接Cloudflareへ通信しない。

通信処理はCommunication Processに分離する。

### 20.3 センサ異常でプロセス全体を停止しない

1回の読み取り失敗でSensor Processを終了させない。

次の周期で再度読み取りを行う。

### 20.4 SensorDataを完成させてから送る

Message Queueへ送信する時点で、

```text
temperature
humidity
timestamp
data_id
```

がすべて設定済みであることを基本とする。

---

## 21. 上位設計との対応

本書は以下の設計を具体化する。

* `03_software_architecture.md`

  * Sensor Process
  * DHT11
  * プロセス分離

* `04_data_ipc_communication.md`

  * SensorData
  * Message Queue

* `05_operation_recovery.md`

  * センサ読み取り失敗時の継続動作

---

## 22. 次の設計書との関係

次の`08_05_ipc.md`では、ここで生成したSensorDataを、Sensor ProcessからCommunication Processへ渡す方法を定義する。

```text
08_04_sensor
      │
      │ SensorData
      ▼
08_05_ipc
      │
      ▼
Communication Process
```

---

## 23. まとめ

Sensor Processでは、DHT11から5秒周期で温度・湿度を取得する。

取得したデータに、

* `data_id`
* `timestamp`

を付加して`SensorData`を完成させる。

完成したSensorDataをMessage Queueへ送信し、Communication Processへ渡す。

Sensor Processはセンサ取得とデータ生成に集中し、HTTPやCloudflareなどの通信処理は担当しない。

基本的な役割は以下のとおりである。

```text
DHT11
 ↓
Dht11Sensor
 ↓
Sensor Process
 ↓
SensorData
 ↓
Message Queue
```
