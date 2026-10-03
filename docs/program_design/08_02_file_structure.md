# 08_02 プログラム設計：ファイル構成

## 1. 目的

本書は、Raspberry Pi 4B Edge IoT SystemのC++プログラムにおけるファイル構成を定義する。

プログラムを実装する際に、

* どのファイルを作成するか
* 各ファイルに何を記述するか
* ヘッダファイルとcppファイルをどのように対応させるか
* 各プロセスのmain.cppが何を担当するか

を明確にする。

---

## 2. 基本方針

本システムでは、機能ごとにソースコードを分離する。

基本的な考え方は以下とする。

```text
ヘッダファイル
    ↓
クラス・構造体の定義

cppファイル
    ↓
クラス・関数の実装
```

また、プロセスごとのエントリーポイントとして `main.cpp` を配置する。

---

## 3. プロジェクト全体のファイル構成

プログラム設計上の基本構成を以下とする。

```text
Raspberry_Pi_4B_Edge_IoT_System/
│
├── include/
│   │
│   ├── common/
│   │   └── SensorData.h
│   │
│   ├── sensor/
│   │   └── Dht11Sensor.h
│   │
│   ├── ipc/
│   │   └── SensorDataMessageQueue.h
│   │
│   └── communication/
│       └── CloudflareClient.h
│
├── src/
│   │
│   ├── sensor/
│   │   ├── main.cpp
│   │   └── Dht11Sensor.cpp
│   │
│   ├── ipc/
│   │   └── SensorDataMessageQueue.cpp
│   │
│   └── communication/
│       ├── main.cpp
│       └── CloudflareClient.cpp
│
├── config/
│   └── communication.env.example
│
├── systemd/
│   ├── sensor-process.service
│   └── communication-process.service
│
├── CMakeLists.txt
│
└── docs/
    └── ...
```

---

## 4. includeディレクトリ

`include/`には、プログラムから利用するクラスや構造体の宣言を配置する。

```text
include/
├── common/
│   └── SensorData.h
├── sensor/
│   └── Dht11Sensor.h
├── ipc/
│   └── SensorDataMessageQueue.h
└── communication/
    └── CloudflareClient.h
```

---

## 5. SensorData.h

### ファイル

```text
include/common/SensorData.h
```

### 役割

SensorDataを定義する。

Sensor ProcessとCommunication Processの両方から利用する共通データ構造とする。

### 主な内容

* `SensorData`構造体
* 温度
* 湿度
* timestamp
* data_id

### イメージ

```text
SensorData.h
    │
    ├── temperature
    ├── humidity
    ├── timestamp
    └── data_id
```

SensorDataの具体的な型やデータ仕様については、`08_03_data_design.md`で定義する。

---

## 6. Dht11Sensor.h

### ファイル

```text
include/sensor/Dht11Sensor.h
```

### 役割

DHT11を操作する `Dht11Sensor` クラスを宣言する。

### 主な内容

* クラス定義
* コンストラクタ
* 初期化処理
* センサ読み取り処理
* 必要なメンバ変数

Dht11Sensorの具体的な設計は、`08_04_sensor.md`で定義する。

---

## 7. SensorDataMessageQueue.h

### ファイル

```text
include/ipc/SensorDataMessageQueue.h
```

### 役割

POSIX Message Queueを操作する `SensorDataMessageQueue` クラスを宣言する。

### 主な内容

* Queueのオープン
* Queueへの送信
* Queueからの受信
* Queue関連リソースの管理
* 必要なメンバ変数

詳細は `08_05_ipc.md` で定義する。

---

## 8. CloudflareClient.h

### ファイル

```text
include/communication/CloudflareClient.h
```

### 役割

Cloudflare WorkerとのHTTPS通信を行う `CloudflareClient` クラスを宣言する。

### 主な内容

* Cloudflare Workerへの接続
* SensorData送信
* HTTP結果取得
* 通信結果の判定
* 必要な通信関連情報

詳細は `08_07_cloudflare_client.md` で定義する。

---

# 9. srcディレクトリ

`src/`にはC++の実装ファイルを配置する。

```text
src/
├── sensor/
│   ├── main.cpp
│   └── Dht11Sensor.cpp
│
├── ipc/
│   └── SensorDataMessageQueue.cpp
│
└── communication/
    ├── main.cpp
    └── CloudflareClient.cpp
```

基本的に、

```text
include/xxx/XXX.h
        ↓
src/xxx/XXX.cpp
```

という対応関係を持たせる。

---

# 10. Sensor Processのファイル

Sensor Processは以下のファイルで構成する。

```text
include/sensor/Dht11Sensor.h
src/sensor/Dht11Sensor.cpp
src/sensor/main.cpp
```

さらにMessage Queueを使用するため、

```text
include/ipc/SensorDataMessageQueue.h
src/ipc/SensorDataMessageQueue.cpp
```

も利用する。

---

## 11. Sensor Process main.cpp

### ファイル

```text
src/sensor/main.cpp
```

### 役割

Sensor Processのエントリーポイントとする。

`main.cpp`では、Sensor Process全体の処理の流れを記述する。

### 主な処理

```text
Sensor Process起動
      ↓
Dht11Sensor生成・初期化
      ↓
Message Queue生成・初期化
      ↓
メインループ
      ↓
DHT11読み取り
      ↓
SensorData生成
      ↓
Message Queue送信
      ↓
一定時間待機
      ↓
次の周期へ
```

### main.cppに記述しない処理

以下の具体的な処理はmain.cppに集中させない。

* DHT11のGPIO制御
* DHT11通信処理
* POSIX Message Queueのシステムコール処理
* Cloudflare通信

これらは専用クラスへ分離する。

---

# 12. Dht11Sensor.cpp

### ファイル

```text
src/sensor/Dht11Sensor.cpp
```

### 役割

`Dht11Sensor.h`で宣言したDht11Sensorクラスを実装する。

### 主な処理

* GPIO初期化
* DHT11通信
* 温度取得
* 湿度取得
* センサ読み取り結果の返却
* GPIO関連リソースの解放

---

# 13. IPCのファイル

IPC機能は以下のファイルで構成する。

```text
include/ipc/SensorDataMessageQueue.h
src/ipc/SensorDataMessageQueue.cpp
```

---

## 14. SensorDataMessageQueue.cpp

### ファイル

```text
src/ipc/SensorDataMessageQueue.cpp
```

### 役割

`SensorDataMessageQueue.h`で宣言したクラスを実装する。

### 主な処理

* POSIX Message Queueのオープン
* Message Queueへの送信
* Message Queueからの受信
* Queue関連エラー処理
* Queue関連リソースの解放

Sensor ProcessとCommunication Processの両方から利用する。

---

# 15. Communication Processのファイル

Communication Processは以下のファイルで構成する。

```text
include/communication/CloudflareClient.h
src/communication/CloudflareClient.cpp
src/communication/main.cpp
```

さらにMessage Queueを使用するため、

```text
include/ipc/SensorDataMessageQueue.h
src/ipc/SensorDataMessageQueue.cpp
```

も利用する。

---

## 16. Communication Process main.cpp

### ファイル

```text
src/communication/main.cpp
```

### 役割

Communication Processのエントリーポイントとする。

### 主な処理

```text
Communication Process起動
      ↓
Message Queue初期化
      ↓
CloudflareClient初期化
      ↓
メインループ
      ↓
Message Queue受信
      ↓
SensorData取得
      ↓
CloudflareClientへ送信依頼
      ↓
通信結果確認
      ↓
必要に応じてリトライ
      ↓
次のデータを受信
```

### main.cppに記述しない処理

以下は専用クラスへ分離する。

* POSIX Message Queueの詳細処理
* HTTPS通信の詳細処理
* JSON生成の詳細処理
* libcurlの詳細操作

---

# 17. CloudflareClient.cpp

### ファイル

```text
src/communication/CloudflareClient.cpp
```

### 役割

`CloudflareClient.h`で宣言したクラスを実装する。

### 主な処理

* libcurl初期化
* HTTPSリクエスト生成
* HTTPヘッダ設定
* SensorDataのJSON化
* POST送信
* HTTPステータス取得
* 通信結果の返却
* libcurl関連リソースの解放

---

# 18. configディレクトリ

設定ファイルを配置する。

```text
config/
└── communication.env.example
```

---

## 19. communication.env.example

### ファイル

```text
config/communication.env.example
```

### 役割

Communication Processが使用する環境変数の設定例を提供する。

例えば以下のような情報を定義する。

```text
CLOUDFLARE_WORKER_URL=...
CLOUDFLARE_SHARED_SECRET=...
```

実際の秘密情報はこのファイルに記述しない。

実際の環境ではsystemdから環境ファイルを読み込む。

---

# 20. systemdディレクトリ

systemdのサービス定義ファイルを配置する。

```text
systemd/
├── sensor-process.service
└── communication-process.service
```

---

## 21. sensor-process.service

### 役割

Sensor Processをsystemdから起動・監視するためのサービス定義。

主な設定対象は以下とする。

* 実行ユーザー
* 実行ファイル
* 起動条件
* 再起動条件
* 標準出力・エラー出力

---

## 22. communication-process.service

### 役割

Communication Processをsystemdから起動・監視するためのサービス定義。

Sensor Processとは別サービスとして管理する。

ネットワーク通信を行うため、ネットワーク起動後に開始する構成とする。

---

# 23. CMakeLists.txt

### ファイル

```text
CMakeLists.txt
```

### 役割

C++プログラムをビルドするためのCMake設定を定義する。

主な設定対象は以下とする。

* C++標準
* includeディレクトリ
* Sensor Processのソース
* Communication Processのソース
* IPC関連ソース
* 必要なライブラリ
* 実行ファイル

---

## 24. 実行ファイル

本システムでは、2つの実行ファイルを生成する。

```text
sensor-process
communication-process
```

対応関係は以下とする。

| 実行ファイル                | main.cpp                     | 役割                     |
| --------------------- | ---------------------------- | ---------------------- |
| sensor-process        | `src/sensor/main.cpp`        | DHT11からセンサデータを取得する     |
| communication-process | `src/communication/main.cpp` | センサデータをCloudflareへ送信する |

---

# 25. ヘッダとcppの対応

基本的な対応関係を以下とする。

| ヘッダ                                        | cpp                                      | クラス                    |
| ------------------------------------------ | ---------------------------------------- | ---------------------- |
| `include/sensor/Dht11Sensor.h`             | `src/sensor/Dht11Sensor.cpp`             | Dht11Sensor            |
| `include/ipc/SensorDataMessageQueue.h`     | `src/ipc/SensorDataMessageQueue.cpp`     | SensorDataMessageQueue |
| `include/communication/CloudflareClient.h` | `src/communication/CloudflareClient.cpp` | CloudflareClient       |
| `include/common/SensorData.h`              | なし                                       | SensorData             |

SensorDataはデータ構造の定義のみを行うため、対応するcppファイルは作成しない。

---

# 26. ファイル間の関係

全体の関係を以下に示す。

```text
                       SensorData.h
                            │
              ┌─────────────┴─────────────┐
              │                           │
              ▼                           ▼
       Sensor Process             Communication Process
              │                           │
       ┌──────┴──────┐             ┌──────┴──────┐
       │             │             │             │
       ▼             ▼             ▼             ▼
 Dht11Sensor   MessageQueue   MessageQueue   CloudflareClient
       │             │             │             │
       ▼             │             │             ▼
      DHT11           └──────┬──────┘          HTTPS
                             │                   │
                             ▼                   ▼
                       POSIX Message Queue   Cloudflare
```

---

# 27. ファイルごとの責任

各ファイルの責任を明確にする。

| ファイル                            | 主な責任                            |
| ------------------------------- | ------------------------------- |
| `SensorData.h`                  | データ構造の定義                        |
| `Dht11Sensor.h`                 | Dht11Sensorの宣言                  |
| `Dht11Sensor.cpp`               | DHT11処理の実装                      |
| `SensorDataMessageQueue.h`      | Message Queueクラスの宣言             |
| `SensorDataMessageQueue.cpp`    | Message Queue処理の実装              |
| `sensor/main.cpp`               | Sensor Process全体の処理             |
| `CloudflareClient.h`            | CloudflareClientの宣言             |
| `CloudflareClient.cpp`          | HTTPS通信の実装                      |
| `communication/main.cpp`        | Communication Process全体の処理      |
| `communication.env.example`     | 通信設定の記述例                        |
| `sensor-process.service`        | Sensor Processのsystemd設定        |
| `communication-process.service` | Communication Processのsystemd設定 |
| `CMakeLists.txt`                | ビルド設定                           |

---

# 28. main.cppとクラスの責任分担

プログラムを実装するときは、以下のように責任を分ける。

### Sensor Process

```text
main.cpp
    │
    ├── 「いつ処理するか」
    │
    ├── Dht11Sensor
    │       └── 「どうやってセンサを読むか」
    │
    └── SensorDataMessageQueue
            └── 「どうやってデータを渡すか」
```

### Communication Process

```text
main.cpp
    │
    ├── 「いつ処理するか」
    │
    ├── SensorDataMessageQueue
    │       └── 「どうやってデータを受け取るか」
    │
    └── CloudflareClient
            └── 「どうやってCloudflareへ送るか」
```

この分担を基本として実装する。

---

# 29. ソースコードの記述順序

C++ソースコードを作成する際は、基本的に以下の順序で設計・実装する。

```text
1. SensorData.h
       ↓
2. Dht11Sensor.h
       ↓
3. Dht11Sensor.cpp
       ↓
4. SensorDataMessageQueue.h
       ↓
5. SensorDataMessageQueue.cpp
       ↓
6. CloudflareClient.h
       ↓
7. CloudflareClient.cpp
       ↓
8. sensor/main.cpp
       ↓
9. communication/main.cpp
       ↓
10. CMakeLists.txt
```

ただし、実際の実装では依存関係やコンパイル確認の都合により順序を調整する場合がある。

---

# 30. プログラム設計と実装の対応

このファイル構成を基準として、後続のプログラム設計書では各ファイルの中身を具体化する。

例えば、

```text
08_04_sensor.md
        ↓
Dht11Sensor.h
Dht11Sensor.cpp
sensor/main.cpp
```

という対応を持たせる。

同様に、

```text
08_05_ipc.md
        ↓
SensorDataMessageQueue.h
SensorDataMessageQueue.cpp
```

```text
08_07_cloudflare_client.md
        ↓
CloudflareClient.h
CloudflareClient.cpp
```

という対応にする。

---

# 31. 設計変更時の考え方

実装中にファイルを追加・削除・統合する必要が発生した場合は、単純にソースコードだけを変更しない。

以下の順番で確認する。

```text
設計上必要か
    ↓
プログラム設計を変更
    ↓
ファイル構成を変更
    ↓
CMakeLists.txtを変更
    ↓
ソースコードを変更
    ↓
ビルド確認
```

設計と実装の不一致を放置しないことを基本とする。

---

# 32. まとめ

本システムでは、Raspberry Pi上のC++プログラムを以下の構成とする。

```text
include/
├── common/
│   └── SensorData.h
├── sensor/
│   └── Dht11Sensor.h
├── ipc/
│   └── SensorDataMessageQueue.h
└── communication/
    └── CloudflareClient.h

src/
├── sensor/
│   ├── main.cpp
│   └── Dht11Sensor.cpp
├── ipc/
│   └── SensorDataMessageQueue.cpp
└── communication/
    ├── main.cpp
    └── CloudflareClient.cpp
```

基本的な設計方針は以下とする。

* ヘッダではクラス・構造体を宣言する
* cppでは処理を実装する
* `main.cpp` はプロセス全体の処理の流れを担当する
* DHT11処理はDht11Sensorに分離する
* IPC処理はSensorDataMessageQueueに分離する
* Cloudflare通信はCloudflareClientに分離する
* SensorDataは共通データ構造として扱う
* Sensor ProcessとCommunication Processを分離する
* 設計とソースコードの対応関係を明確にする

このファイル構成を、以降のプログラム設計およびC++実装の基本構成とする。
