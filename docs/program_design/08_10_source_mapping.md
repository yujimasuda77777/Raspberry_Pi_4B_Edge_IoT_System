# 08_10 ソース対応設計

## 1. 目的

本書では、これまでに定義したプログラム設計と、実際に作成するソースファイルとの対応関係を定義する。

設計書を見たときに、

* どのファイルを作るのか
* そのファイルは何を担当するのか
* どのクラスを作るのか
* どの処理をどこに実装するのか

が分かることを目的とする。

---

## 2. 対象ソース構成

基本的なソース構成は以下とする。

```text
include/
├── common/
│   └── SensorData.h
│
├── ipc/
│   └── SensorDataMessageQueue.h
│
├── sensor/
│   └── Dht11Sensor.h
│
└── communication/
    └── CloudflareClient.h


src/
├── sensor/
│   ├── main.cpp
│   └── Dht11Sensor.cpp
│
├── communication/
│   ├── main.cpp
│   └── CloudflareClient.cpp
│
└── ipc/
    └── SensorDataMessageQueue.cpp
```

---

## 3. ファイルの役割

各ファイルの基本的な役割を以下とする。

| ファイル                         | 主な役割                       |
| ---------------------------- | -------------------------- |
| `SensorData.h`               | SensorData構造体の定義           |
| `SensorDataMessageQueue.h`   | Message Queueクラスの宣言        |
| `SensorDataMessageQueue.cpp` | Message Queueクラスの実装        |
| `Dht11Sensor.h`              | DHT11クラスの宣言                |
| `Dht11Sensor.cpp`            | DHT11クラスの実装                |
| `sensor/main.cpp`            | Sensor Process全体の制御        |
| `CloudflareClient.h`         | CloudflareClientクラスの宣言     |
| `CloudflareClient.cpp`       | HTTPS通信の実装                 |
| `communication/main.cpp`     | Communication Process全体の制御 |

---

## 4. SensorData

### 対応ファイル

```text
include/common/SensorData.h
```

### 役割

プロセス間で受け渡すSensorDataのデータ構造を定義する。

基本構造：

```text
SensorData
├── temperature
├── humidity
├── timestamp
└── data_id
```

SensorData自体には、センサ処理や通信処理を実装しない。

---

## 5. SensorDataMessageQueue

### 対応ファイル

```text
include/ipc/SensorDataMessageQueue.h
src/ipc/SensorDataMessageQueue.cpp
```

### 役割

POSIX Message Queueを扱う。

主な処理：

```text
Queue
├── open
├── send
├── receive
└── close
```

Message QueueのOS依存処理をこのクラスにまとめる。

---

## 6. SensorDataMessageQueueとプロセスの関係

Sensor Processでは主に送信側として使用する。

```text
sensor/main.cpp
      ↓
SensorDataMessageQueue
      ↓
POSIX Message Queue
```

Communication Processでは受信側として使用する。

```text
POSIX Message Queue
      ↓
SensorDataMessageQueue
      ↓
communication/main.cpp
```

---

## 7. Dht11Sensor

### 対応ファイル

```text
include/sensor/Dht11Sensor.h
src/sensor/Dht11Sensor.cpp
```

### 役割

DHT11から温度・湿度を取得する。

基本的な責務：

```text
GPIO初期化
   ↓
DHT11読み取り
   ↓
温度・湿度取得
   ↓
GPIO終了
```

---

## 8. Dht11Sensorが担当しない処理

Dht11Sensorは以下を担当しない。

* Message Queue送信
* `data_id`生成
* `timestamp`管理
* HTTPS通信
* Cloudflare Worker通信
* リトライ制御
* systemd制御

これらはそれぞれ担当する場所で処理する。

---

## 9. Sensor Process main.cpp

### 対応ファイル

```text
src/sensor/main.cpp
```

### 役割

Sensor Process全体の処理を制御する。

主な処理：

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
Queue送信
      ↓
繰り返し
```

---

## 10. Sensor Process main.cppとDht11Sensor.cppの分担

処理の流れと実装場所を分ける。

| 処理           | 実装場所                           |
| ------------ | ------------------------------ |
| DHT11のGPIO処理 | `Dht11Sensor.cpp`              |
| 温度・湿度の取得     | `Dht11Sensor.cpp`              |
| センサ初期化       | `Dht11Sensor.cpp`              |
| SensorData生成 | `sensor/main.cpp`              |
| data_id管理    | `sensor/main.cpp`              |
| timestamp取得  | `sensor/main.cpp`              |
| Queue送信      | `sensor/main.cpp`からQueueクラスを使用 |
| 5秒周期制御       | `sensor/main.cpp`              |

---

## 11. Communication Process main.cpp

### 対応ファイル

```text
src/communication/main.cpp
```

### 役割

Communication Process全体の処理を制御する。

基本フロー：

```text
Communication Process起動
      ↓
設定読み込み
      ↓
Message Queue初期化
      ↓
CloudflareClient初期化
      ↓
Queue受信待ち
      ↓
SensorData受信
      ↓
CloudflareClientで送信
      ↓
結果確認
      ↓
必要ならリトライ
      ↓
次のSensorData
```

---

## 12. CloudflareClient

### 対応ファイル

```text
include/communication/CloudflareClient.h
src/communication/CloudflareClient.cpp
```

### 役割

Cloudflare WorkerとのHTTPS通信を担当する。

主な処理：

```text
SensorData受信
      ↓
JSON生成
      ↓
HTTPヘッダー設定
      ↓
HTTPS POST
      ↓
HTTPレスポンス取得
      ↓
通信結果返却
```

---

## 13. CloudflareClientが担当しない処理

CloudflareClientは以下を担当しない。

* Message Queue受信
* DHT11読み取り
* `data_id`生成
* 5秒周期制御
* リトライ回数管理
* systemd制御

リトライの判断はCommunication Process側で行う。

---

## 14. Communication Process main.cppとCloudflareClient.cppの分担

| 処理           | 実装場所                     |
| ------------ | ------------------------ |
| Queue受信      | `communication/main.cpp` |
| SensorData取得 | `communication/main.cpp` |
| HTTPS送信指示    | `communication/main.cpp` |
| JSON生成       | `CloudflareClient.cpp`   |
| HTTPヘッダー設定   | `CloudflareClient.cpp`   |
| HTTPS通信      | `CloudflareClient.cpp`   |
| HTTPステータス取得  | `CloudflareClient.cpp`   |
| リトライ判定       | `communication/main.cpp` |
| リトライ待機       | `communication/main.cpp` |
| 最大リトライ回数管理   | `communication/main.cpp` |

---

## 15. データの受け渡し

SensorDataは以下の順番で渡される。

```text
Dht11Sensor
      │
      │ temperature
      │ humidity
      ▼
sensor/main.cpp
      │
      │ SensorData
      ▼
SensorDataMessageQueue
      │
      │ SensorData
      ▼
communication/main.cpp
      │
      │ SensorData
      ▼
CloudflareClient
      │
      │ JSON
      ▼
Cloudflare Worker
```

---

## 16. SensorDataの変換箇所

SensorDataの値は、基本的に以下の場所では変更しない。

```text
Sensor Process
      ↓
Message Queue
      ↓
Communication Process
      ↓
CloudflareClient
```

CloudflareClientではSensorDataをJSON形式へ変換する。

つまり、

```text
SensorData
    ↓
JSON
```

という形式変換だけを行う。

---

## 17. data_idの対応

`data_id`はSensor Processで生成する。

```text
sensor/main.cpp
      ↓
data_id生成
      ↓
SensorData
      ↓
Queue
      ↓
Communication Process
      ↓
CloudflareClient
      ↓
JSON
      ↓
Worker
      ↓
D1
```

Communication Processでは`data_id`を変更しない。

---

## 18. timestampの対応

`timestamp`もSensor Processで設定する。

```text
DHT11読み取り成功
      ↓
timestamp取得
      ↓
SensorData
```

その後は変更せずにCloudflare Workerまで渡す。

---

## 19. リトライ時のソース対応

リトライはCommunication Processで行う。

```text
communication/main.cpp
```

基本構造：

```text
SensorData受信
      ↓
初回送信
      ↓
結果確認
      ↓
失敗？
  ┌───┴───┐
 NO       YES
  ↓        ↓
次のデータ  リトライ判定
             ↓
          再送
```

リトライ対象の場合は同じSensorDataを使用する。

---

## 20. エラー処理の対応

| エラー         | 主な処理場所                                |
| ----------- | ------------------------------------- |
| DHT11読み取り失敗 | `sensor/main.cpp`                     |
| DHT11初期化失敗  | `sensor/main.cpp` / `Dht11Sensor.cpp` |
| Queue初期化失敗  | 各`main.cpp`                           |
| Queue送信失敗   | `sensor/main.cpp`                     |
| Queue受信エラー  | `communication/main.cpp`              |
| HTTP 4xx    | `communication/main.cpp`              |
| HTTP 5xx    | `communication/main.cpp`              |
| HTTPS通信エラー  | `CloudflareClient.cpp`                |
| リトライ制御      | `communication/main.cpp`              |
| プロセス異常終了    | systemd                               |

---

## 21. 設定値の対応

Cloudflare通信に必要な設定は環境変数から取得する。

### Raspberry Pi側

```text
/etc/raspberry-pi-edge-iot/communication.env
```

主な設定：

```text
CLOUDFLARE_SHARED_SECRET
```

実際の秘密情報はGit管理対象にしない。

GitHubには例として、

```text
config/communication.env.example
```

を配置する。

---

## 22. systemdの対応

### Sensor Process

```text
systemd/
└── sensor-process.service
```

実行ファイル：

```text
/usr/local/bin/sensor-process
```

### Communication Process

```text
systemd/
└── communication-process.service
```

実行ファイル：

```text
/usr/local/bin/communication-process
```

---

## 23. CMakeLists.txtとの対応

プロジェクト全体のビルド設定は以下で管理する。

```text
CMakeLists.txt
```

主な対象：

```text
src/sensor/main.cpp
src/sensor/Dht11Sensor.cpp
src/ipc/SensorDataMessageQueue.cpp
src/communication/main.cpp
src/communication/CloudflareClient.cpp
```

それぞれ必要なヘッダファイルを参照する。

---

## 24. ヘッダとcppの対応

基本的に、クラスについてはヘッダとcppを対応させる。

```text
Dht11Sensor.h
      ↕
Dht11Sensor.cpp
```

```text
SensorDataMessageQueue.h
      ↕
SensorDataMessageQueue.cpp
```

```text
CloudflareClient.h
      ↕
CloudflareClient.cpp
```

クラスの宣言を`.h`、クラスの実装を`.cpp`に分ける。

---

## 25. main.cppの考え方

`main.cpp`には、処理全体の流れを記述する。

例えばSensor Processでは、

```text
初期化
 ↓
センサ読み取り
 ↓
SensorData生成
 ↓
Queue送信
 ↓
待機
 ↓
繰り返し
```

という流れが分かるようにする。

細かいハードウェア処理や通信処理は、それぞれのクラスへ分離する。

---

## 26. 実装時の基本ルール

実装では以下のルールを基本とする。

### 26.1 設計にない処理を勝手に追加しない

必要な処理が増えた場合は、まずプログラム設計を確認する。

### 26.2 1つのクラスに責務を集中させない

Dht11Sensor、Message Queue、CloudflareClientなど、それぞれの責務を明確にする。

### 26.3 main.cppを複雑にしすぎない

main.cppは全体の処理順序が分かることを優先する。

### 26.4 データを不用意に変更しない

SensorDataの値は、センサ取得時からD1保存まで基本的に維持する。

---

## 27. 設計からソースへの対応表

| プログラム設計               | ソース                                        |
| --------------------- | ------------------------------------------ |
| SensorData            | `include/common/SensorData.h`              |
| Message Queue         | `include/ipc/SensorDataMessageQueue.h`     |
| Message Queue実装       | `src/ipc/SensorDataMessageQueue.cpp`       |
| DHT11                 | `include/sensor/Dht11Sensor.h`             |
| DHT11実装               | `src/sensor/Dht11Sensor.cpp`               |
| Sensor Process        | `src/sensor/main.cpp`                      |
| Cloudflare Client     | `include/communication/CloudflareClient.h` |
| Cloudflare Client実装   | `src/communication/CloudflareClient.cpp`   |
| Communication Process | `src/communication/main.cpp`               |

---

## 28. プログラム設計書との対応

本書まで含めると、プログラム設計は以下のように整理される。

```text
08_01_system_overview.md
        ↓
システム全体

08_02_file_structure.md
        ↓
ファイル構成

08_03_data_design.md
        ↓
データ構造

08_04_sensor.md
        ↓
センサ処理

08_05_ipc.md
        ↓
プロセス間通信

08_06_communication.md
        ↓
通信処理

08_07_cloudflare_client.md
        ↓
HTTPS通信

08_08_error_retry.md
        ↓
エラー・リトライ

08_09_process_flow.md
        ↓
処理フロー

08_10_source_mapping.md
        ↓
実際のソースへの対応
```

---

## 29. 実装への移行

プログラム設計完了後は、以下の順番で実装する。

```text
プログラム設計
      ↓
SensorData
      ↓
Message Queue
      ↓
Dht11Sensor
      ↓
CloudflareClient
      ↓
Sensor Process
      ↓
Communication Process
      ↓
CMake
      ↓
ビルド
      ↓
動作確認
```

各段階でコンパイル・確認を行いながら進める。

---

## 30. 実装前の確認事項

実装を開始する前に、以下を確認する。

* [ ] SensorDataの定義が確定している
* [ ] Message Queueの仕様が確定している
* [ ] DHT11の仕様が確定している
* [ ] Cloudflare通信の仕様が確定している
* [ ] エラー処理が定義されている
* [ ] リトライ仕様が定義されている
* [ ] プロセスフローが定義されている
* [ ] ソースファイルの対応が定義されている

これらを確認した上で実装へ進む。

---

## 31. まとめ

本書では、プログラム設計と実際のソースファイルを対応付けた。

重要な対応関係は以下である。

```text
SensorData
    ↓
Sensor Process
    ↓
Message Queue
    ↓
Communication Process
    ↓
CloudflareClient
```

それぞれの処理を適切なファイル・クラスへ分離することで、設計と実装の対応を明確にする。

これで、詳細なプログラム設計からC++実装へ移行できる状態とする。
