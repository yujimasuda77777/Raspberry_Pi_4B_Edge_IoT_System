# 08_01 プログラム設計：システム概要

## 1. 目的

本書は、Raspberry Pi 4B Edge IoT System におけるプログラム全体の構成と、各プログラム・クラスの役割を定義する。

本書では、システム設計およびソフトウェア設計で定義された構成をもとに、C++プログラムを実装するために必要となるプログラム単位の構成を整理する。

本書は、以下の設計書とC++ソースコードの間をつなぐ位置付けとする。

* 要求仕様
* システムアーキテクチャ
* ソフトウェアアーキテクチャ
* データ・IPC・通信設計
* 運用・復旧設計
* システム管理・セキュリティ設計
* プログラム設計
* C++ソースコード

---

## 2. 対象範囲

本書で扱う対象は、Raspberry Pi 4B上で動作するC++プログラムとする。

対象となる主なプログラムは以下の2つである。

* Sensor Process
* Communication Process

また、これらのプロセスから利用される以下のクラスも対象とする。

* Dht11Sensor
* SensorDataMessageQueue
* CloudflareClient
* SensorData

Cloudflare WorkerおよびD1はRaspberry Pi上のC++プログラムではないため、本書ではC++プログラムとの接続関係を中心に扱う。

---

## 3. プログラム全体構成

Raspberry Pi上のプログラムは、以下の2プロセスに分離する。

```text
Raspberry Pi 4B
│
├── Sensor Process
│   │
│   ├── Dht11Sensor
│   │
│   └── SensorDataMessageQueue
│
└── Communication Process
    │
    ├── SensorDataMessageQueue
    │
    └── CloudflareClient
```

Sensor ProcessはDHT11から温度・湿度を取得し、SensorDataを生成してMessage Queueへ送信する。

Communication ProcessはMessage QueueからSensorDataを受信し、Cloudflare WorkerへHTTPS POSTする。

---

## 4. データの流れ

プログラム全体のデータの流れを以下に示す。

```text
DHT11
  │
  │ 温度・湿度
  ▼
Dht11Sensor
  │
  │ SensorData生成
  ▼
Sensor Process
  │
  │ POSIX Message Queue
  ▼
Communication Process
  │
  │ SensorData
  ▼
CloudflareClient
  │
  │ HTTPS POST
  ▼
Cloudflare Worker
  │
  ▼
Cloudflare D1
```

ブラウザからCloudflare Workerへアクセスした場合は、D1に保存されたデータを取得して表示する。

```text
Browser
   │
   ▼
Cloudflare Worker
   │
   ▼
Cloudflare D1
```

---

## 5. プログラム構成要素

本システムで使用する主なプログラム構成要素を以下に示す。

| 構成要素                   | 種類   | 主な役割                          |
| ---------------------- | ---- | ----------------------------- |
| Sensor Process         | プロセス | センサ値を取得して送信する                 |
| Communication Process  | プロセス | センサデータを受信してCloudflareへ送信する    |
| Dht11Sensor            | クラス  | DHT11との通信とセンサ値取得を行う           |
| SensorDataMessageQueue | クラス  | プロセス間でSensorDataを受け渡す         |
| CloudflareClient       | クラス  | Cloudflare WorkerとのHTTPS通信を行う |
| SensorData             | 構造体  | センサデータを保持する                   |

---

## 6. Sensor Process

### 6.1 役割

Sensor Processは、一定周期でDHT11から温度・湿度を取得し、SensorDataとしてMessage Queueへ送信する。

Sensor Processはセンサデータを作成する側のプロセスである。

---

### 6.2 主な処理

Sensor Processでは以下の処理を行う。

1. プログラムを初期化する
2. DHT11を初期化する
3. Message Queueをオープンする
4. 一定周期でDHT11を読み取る
5. SensorDataを生成する
6. data_idを設定する
7. timestampを設定する
8. Message Queueへ送信する
9. 次の周期まで待機する
10. プロセス終了時にリソースを解放する

---

### 6.3 Sensor Processが担当しない処理

Sensor Processは以下の処理を担当しない。

* Cloudflare Workerとの通信
* HTTPS通信
* D1へのデータ保存
* HTTPリトライ
* Cloudflare APIの処理

Sensor Processは、基本的に

**「センサ値を取得してSensorDataを作り、Message Queueへ渡す」**

ことに集中する。

---

## 7. Communication Process

### 7.1 役割

Communication Processは、Sensor ProcessからMessage Queue経由でSensorDataを受信し、Cloudflare Workerへ送信する。

---

### 7.2 主な処理

Communication Processでは以下の処理を行う。

1. プログラムを初期化する
2. Message Queueをオープンする
3. CloudflareClientを初期化する
4. Message QueueからSensorDataを受信する
5. SensorDataをCloudflareClientへ渡す
6. Cloudflare WorkerへHTTPS POSTする
7. 通信結果を確認する
8. 必要に応じてリトライする
9. 次のSensorDataを受信する
10. プロセス終了時にリソースを解放する

---

### 7.3 Communication Processが担当しない処理

Communication ProcessはDHT11を直接操作しない。

以下の処理はDht11Sensorへ任せる。

* GPIO操作
* DHT11通信
* 温度取得
* 湿度取得

また、HTTPS通信そのものはCloudflareClientへ任せる。

---

## 8. Dht11Sensor

### 8.1 役割

Dht11Sensorは、DHT11から温度と湿度を取得するためのクラスである。

Sensor Processから利用される。

---

### 8.2 担当する処理

Dht11Sensorは以下を担当する。

* GPIOの初期化
* DHT11との通信
* 温度の取得
* 湿度の取得
* 読み取り結果の返却
* センサ読み取りに必要なリソースの管理

---

### 8.3 担当しない処理

Dht11Sensorは以下を担当しない。

* 5秒周期の管理
* data_idの生成
* timestampの生成
* Message Queueへの送信
* Cloudflare通信
* HTTPリトライ

周期管理などのアプリケーション処理はSensor Processが担当する。

---

## 9. SensorData

SensorDataは、Sensor ProcessとCommunication Processの間で受け渡すセンサデータを表す。

基本構造は以下とする。

```text
SensorData
├── temperature
├── humidity
├── timestamp
└── data_id
```

各メンバーの意味は以下のとおり。

| メンバー        | 型      | 内容                 |
| ----------- | ------ | ------------------ |
| temperature | double | 温度 [℃]             |
| humidity    | double | 湿度 [%RH]           |
| timestamp   | int64  | センサ取得時刻（UTC Unix秒） |
| data_id     | uint64 | センサデータを識別するID      |

詳細なデータ型や値の扱いについては、`08_03_data_design.md`で定義する。

---

## 10. SensorDataMessageQueue

SensorDataMessageQueueは、Sensor ProcessとCommunication Processの間でSensorDataを受け渡すためのクラスである。

---

### 10.1 Sensor Process側

Sensor ProcessはMessage QueueへSensorDataを送信する。

```text
Sensor Process
      │
      │ send
      ▼
POSIX Message Queue
```

---

### 10.2 Communication Process側

Communication ProcessはMessage QueueからSensorDataを受信する。

```text
POSIX Message Queue
      │
      │ receive
      ▼
Communication Process
```

---

### 10.3 Message Queueの役割

Message Queueは、2つのプロセスを直接依存させないための通信手段として使用する。

Sensor ProcessはCommunication Processを直接呼び出さない。

Communication ProcessもSensor Processを直接呼び出さない。

両プロセスはMessage Queueを介してデータを受け渡す。

---

## 11. CloudflareClient

CloudflareClientは、Cloudflare WorkerとのHTTPS通信を担当するクラスである。

Communication Processから利用される。

---

### 11.1 担当する処理

CloudflareClientは以下を担当する。

* Cloudflare WorkerへのHTTPS接続
* POSTリクエストの生成
* SensorDataのJSON化
* HTTPリクエスト送信
* HTTPステータスコード取得
* 通信結果の返却

---

### 11.2 担当しない処理

CloudflareClientは以下を担当しない。

* DHT11読み取り
* Message Queue操作
* センサ周期管理
* SensorDataの生成
* プロセス制御

通信に関する処理をCloudflareClientに集約することで、Communication Processの処理を単純化する。

---

## 12. data_idの扱い

data_idはSensor Processで生成する。

Sensor ProcessがSensorDataを生成する際にdata_idを設定する。

```text
Sensor Process
      │
      │ data_idを生成
      ▼
SensorData
      │
      ▼
Message Queue
      │
      ▼
Communication Process
```

Communication Processはdata_idを変更しない。

CloudflareClientもdata_idを変更しない。

同じSensorDataを通信リトライする場合も、同じdata_idを使用する。

これにより、同じデータを再送した場合でも、Cloudflare側で同一データとして識別できる。

---

## 13. timestampの扱い

timestampはSensorDataを生成する際に設定する。

timestampはセンサ値を取得した時刻を表し、UTC Unix秒で保持する。

```text
DHT11読み取り
      │
      ▼
SensorData生成
      │
      ├── temperature
      ├── humidity
      ├── timestamp
      └── data_id
```

Communication Processではtimestampを変更しない。

---

## 14. 処理周期

Sensor Processは、5秒周期を基本としてセンサ値を取得する。

概念的な処理は以下のとおり。

```text
┌──────────────────────────┐
│ Sensor Process           │
├──────────────────────────┤
│ DHT11読み取り            │
│ SensorData生成           │
│ Message Queue送信        │
│                          │
│ 5秒待機                  │
│                          │
│ 次の読み取り             │
└──────────────────────────┘
```

5秒周期の管理はSensor Processが担当する。

Dht11Sensor自身は周期を管理しない。

---

## 15. エラー処理の基本方針

エラーが発生した場合でも、可能な限りプロセス全体を停止させずに処理を継続する。

### Sensor Process

DHT11読み取りに失敗した場合は、その周期のデータを送信せず、次の周期で再度読み取りを行う。

```text
読み取り
  │
  ├── 成功 → SensorData生成 → Queue送信
  │
  └── 失敗 → エラー記録 → 次周期へ
```

### Communication Process

Cloudflareへの通信に失敗した場合は、通信結果に応じてリトライを行う。

詳細なリトライ条件は `08_08_error_retry.md` で定義する。

---

## 16. プロセスを分離する理由

Sensor ProcessとCommunication Processを分離することで、センサ取得処理とネットワーク通信処理を独立させる。

```text
Sensor Process
    │
    │ SensorData
    ▼
Message Queue
    │
    ▼
Communication Process
```

この構成により、以下のような役割分担が明確になる。

### Sensor Process

```text
センサを読む
↓
データを作る
↓
データを渡す
```

### Communication Process

```text
データを受け取る
↓
Cloudflareへ送る
```

センサ処理とネットワーク処理を1つのプログラムに詰め込まないことを基本方針とする。

---

## 17. main.cppの役割

各プロセスの `main.cpp` は、そのプロセス全体の処理を管理する。

ただし、個々の機能をすべて `main.cpp` に記述することは避ける。

例えばSensor Processでは、

```text
main.cpp
   │
   ├── Dht11Sensor
   │
   └── SensorDataMessageQueue
```

という関係にする。

Communication Processでは、

```text
main.cpp
   │
   ├── SensorDataMessageQueue
   │
   └── CloudflareClient
```

という関係にする。

`main.cpp` は「処理の流れ」を担当し、具体的な機能は各クラスに分離する。

---

## 18. クラスの基本的な依存関係

プログラム全体の依存関係を以下に示す。

```text
Sensor Process
 │
 ├── Dht11Sensor
 │
 └── SensorDataMessageQueue
        │
        │ POSIX Message Queue
        │
        ▼
Communication Process
 │
 ├── SensorDataMessageQueue
 │
 └── CloudflareClient
```

SensorDataは両プロセスで共通して使用する。

```text
              SensorData
              ▲       ▲
              │       │
              │       │
      Sensor Process   Communication Process
```

---

## 19. 初期化処理

Sensor Processでは、概ね以下の順序で初期化する。

```text
Sensor Process起動
      │
      ▼
Dht11Sensor初期化
      │
      ▼
Message Queue初期化
      │
      ▼
メインループ開始
```

Communication Processでは、概ね以下の順序で初期化する。

```text
Communication Process起動
      │
      ▼
Message Queue初期化
      │
      ▼
CloudflareClient初期化
      │
      ▼
メインループ開始
```

具体的な初期化処理については、各機能のプログラム設計書で定義する。

---

## 20. 終了処理

プロセス終了時には、使用しているリソースを適切に解放する。

Sensor Processでは、主に以下を対象とする。

* DHT11関連リソース
* GPIO関連リソース
* Message Queue

Communication Processでは、主に以下を対象とする。

* Message Queue
* HTTPS通信関連リソース
* CloudflareClient関連リソース

---

## 21. プログラム全体の処理イメージ

全体を単純化すると、以下の処理となる。

```text
【Sensor Process】

DHT11
  │
  ▼
Dht11Sensor
  │
  ▼
SensorData生成
  │
  ▼
Message Queue
  │
  │
  ▼
【Communication Process】
  │
  ▼
SensorData受信
  │
  ▼
CloudflareClient
  │
  ▼
HTTPS POST
  │
  ▼
Cloudflare Worker
  │
  ▼
D1
```

---

## 22. プログラム設計における基本原則

本システムのプログラム設計では、以下を基本原則とする。

### 22.1 役割を分ける

1つのクラスや関数に複数の役割を持たせない。

---

### 22.2 main.cppを複雑にしない

`main.cpp` は処理の流れを分かりやすくすることを優先する。

---

### 22.3 センサ処理と通信処理を分離する

DHT11の処理とCloudflareへの通信処理を直接結び付けない。

---

### 22.4 データを共通化する

Sensor ProcessとCommunication Processの間では、SensorDataを共通のデータ構造として使用する。

---

### 22.5 クラスの責任を明確にする

各クラスが「何をするクラスなのか」を明確にし、他のクラスの仕事をできるだけ担当させない。

---

### 22.6 設計と実装を対応させる

プログラム設計書に記載したクラス、関数、データ構造と、実際のC++ソースコードが対応するようにする。

設計書に存在しない処理を実装側で独自に追加する場合は、設計変更として扱う。

---

## 23. 上位設計との関係

本書は、上位設計で定義されたシステム構成をC++プログラムへ展開するための設計書である。

```text
要求仕様
   ↓
システムアーキテクチャ
   ↓
ソフトウェアアーキテクチャ
   ↓
データ・IPC・通信設計
   ↓
運用・復旧設計
   ↓
システム管理・セキュリティ設計
   ↓
プログラム設計
   ↓
C++実装
```

プログラム設計では、上位設計をそのまま繰り返すのではなく、

* どのプロセスが担当するか
* どのクラスが担当するか
* どのデータを受け渡すか
* どのクラスがどのクラスを利用するか
* どこでエラーを処理するか

を具体化する。

---

## 24. 今後の詳細設計

本書で定義した全体構成をもとに、以下の文書で各部分を詳細化する。

### 08_02_file_structure.md

ソースコードのディレクトリ構成、ヘッダファイル、cppファイル、設定ファイル、systemdファイルの構成を定義する。

### 08_03_data_design.md

SensorData、data_id、timestamp、JSONデータなどのデータ構造を定義する。

### 08_04_sensor.md

Dht11SensorおよびSensor Processの詳細設計を定義する。

### 08_05_ipc.md

POSIX Message QueueおよびSensorDataMessageQueueの詳細設計を定義する。

### 08_06_communication.md

Communication Processの詳細設計を定義する。

### 08_07_cloudflare_client.md

CloudflareClientおよびCloudflare WorkerとのHTTPS通信の詳細設計を定義する。

### 08_08_error_retry.md

センサエラー、Message Queueエラー、HTTPエラー、通信リトライなどの詳細設計を定義する。

### 08_09_process_flow.md

Sensor ProcessおよびCommunication Processの処理フロー、シーケンスを定義する。

### 08_10_source_mapping.md

プログラム設計と実際のC++ソースコードとの対応関係を定義する。

---

## 25. まとめ

本システムのRaspberry Pi側プログラムは、以下の2プロセスを中心に構成する。

```text
Sensor Process
    │
    │ SensorData
    ▼
POSIX Message Queue
    │
    ▼
Communication Process
    │
    │ HTTPS
    ▼
Cloudflare Worker
```

Sensor Processはセンサデータの取得とMessage Queueへの送信を担当する。

Communication ProcessはMessage Queueからの受信とCloudflare Workerへの送信を担当する。

Dht11Sensor、SensorDataMessageQueue、CloudflareClientは、それぞれの機能をクラスとして分離する。

この構成を基本として、以降のプログラム設計書で各機能を詳細化する。
