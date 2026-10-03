# 08_03 プログラム設計：データ設計

## 1. 目的

本書は、Raspberry Pi 4B Edge IoT Systemで使用するデータ構造を定義する。

主に以下のデータを対象とする。

* SensorData
* temperature
* humidity
* timestamp
* data_id
* POSIX Message Queueで受け渡すデータ
* Cloudflare Workerへ送信するJSON
* Cloudflare D1へ保存するデータ

本書では、Sensor Processで取得したデータが、Communication Processを経由してCloudflare WorkerおよびD1へ到達するまでのデータの対応関係を明確にする。

---

# 2. データの基本的な流れ

センサデータは以下の流れで扱う。

```text
DHT11
  │
  │ 温度・湿度
  ▼
SensorData
  │
  │ POSIX Message Queue
  ▼
Communication Process
  │
  │ JSON
  ▼
Cloudflare Worker
  │
  │ SQL
  ▼
Cloudflare D1
```

基本的には、Sensor Processで作成したSensorDataをCommunication Processが受け取り、その内容をCloudflare Workerへ送信する。

---

# 3. SensorData

## 3.1 役割

`SensorData`は、1回のセンサ取得によって得られたデータをまとめて保持するための構造体である。

Sensor ProcessとCommunication Processの間で共通して使用する。

---

## 3.2 定義

SensorDataは以下のメンバーを持つ。

```text
SensorData
├── temperature
├── humidity
├── timestamp
└── data_id
```

C++では以下の構造を基本とする。

```text
struct SensorData
{
    double temperature;
    double humidity;
    std::int64_t timestamp;
    std::uint64_t data_id;
};
```

実際のC++コードは `include/common/SensorData.h` に定義する。

---

# 4. temperature

## 4.1 意味

`temperature`はDHT11から取得した温度を表す。

単位は℃とする。

---

## 4.2 型

```text
double
```

とする。

---

## 4.3 例

```text
25.4
26.1
27.0
```

---

## 4.4 データの流れ

```text
DHT11
  │
  ▼
Dht11Sensor
  │
  ▼
SensorData.temperature
```

Dht11Sensorが取得した温度をSensorDataへ設定する。

Communication Processではtemperatureの値を変更しない。

---

# 5. humidity

## 5.1 意味

`humidity`はDHT11から取得した相対湿度を表す。

単位は `%RH` とする。

---

## 5.2 型

```text
double
```

とする。

---

## 5.3 例

```text
60.0
61.5
64.0
```

---

## 5.4 データの流れ

```text
DHT11
  │
  ▼
Dht11Sensor
  │
  ▼
SensorData.humidity
```

Communication Processではhumidityの値を変更しない。

---

# 6. timestamp

## 6.1 意味

`timestamp`は、センサ値を取得した時刻を表す。

通信した時刻ではなく、**センサ値を取得した時刻**を記録する。

---

## 6.2 型

```text
std::int64_t
```

とする。

---

## 6.3 単位

Unix時間の秒単位とする。

---

## 6.4 基準時刻

UTCを基準とする。

例えば、

```text
1750000000
```

のような値を保持する。

---

## 6.5 timestampを設定するタイミング

timestampはSensorDataを生成するときに設定する。

概念的には以下の順序とする。

```text
DHT11読み取り
    ↓
現在時刻取得
    ↓
SensorData生成
    ↓
timestamp設定
```

timestampはCommunication Processで更新しない。

---

# 7. data_id

## 7.1 意味

`data_id`は、SensorDataを識別するためのIDである。

各センサデータに対してSensor Processが設定する。

---

## 7.2 型

```text
std::uint64_t
```

とする。

---

## 7.3 生成場所

data_idはSensor Processで生成する。

```text
Sensor Process
      │
      ▼
data_id生成
      │
      ▼
SensorData
```

Communication Processでは新しいdata_idを生成しない。

---

## 7.4 データ送信時の扱い

Communication ProcessはSensorDataを受信した後もdata_idをそのまま保持する。

CloudflareClientもdata_idを変更しない。

```text
Sensor Process
      │
      │ data_id = 100
      ▼
Message Queue
      │
      │ data_id = 100
      ▼
Communication Process
      │
      │ data_id = 100
      ▼
Cloudflare Worker
```

---

# 8. data_idとリトライ

data_idは通信リトライにおいて重要となる。

例えば、Sensor Processが以下のSensorDataを生成したとする。

```text
data_id    = 100
temperature = 25.4
humidity    = 60.0
timestamp   = 1750000000
```

Communication ProcessがCloudflare Workerへの送信に失敗した場合、同じデータを再送する。

```text
1回目
data_id = 100

2回目
data_id = 100

3回目
data_id = 100
```

リトライするたびに新しいdata_idを生成してはいけない。

---

# 9. SensorDataの生成

Sensor Processでは、DHT11から取得した値を使用してSensorDataを生成する。

概念的には以下のようになる。

```text
DHT11読み取り
    │
    ├── temperature
    └── humidity
          │
          ▼
    timestamp取得
          │
          ▼
    data_id生成
          │
          ▼
      SensorData
```

SensorDataには、1回のセンサ取得に必要な情報をまとめて格納する。

---

# 10. SensorDataの完成状態

Message Queueへ送信する時点では、SensorDataの4項目がすべて設定されている状態とする。

```text
SensorData
├── temperature : 設定済み
├── humidity    : 設定済み
├── timestamp   : 設定済み
└── data_id     : 設定済み
```

未設定のSensorDataをMessage Queueへ送信しない。

---

# 11. Message Queueで受け渡すデータ

Sensor ProcessとCommunication Processの間では、SensorDataを受け渡す。

```text
Sensor Process
      │
      │ SensorData
      ▼
POSIX Message Queue
      │
      │ SensorData
      ▼
Communication Process
```

Message Queueでは、SensorDataの各メンバーを同じ意味のまま受け渡す。

---

# 12. Message Queue上のデータ

Message Queueへ送信するデータは、SensorDataの内容を1メッセージとして扱う。

1メッセージは以下の情報を持つ。

```text
temperature
humidity
timestamp
data_id
```

Sensor ProcessとCommunication Processで、同じデータ形式を使用する。

---

# 13. Message Queueのデータ順序

通常の処理では、Sensor Processが生成したSensorDataを順番にMessage Queueへ送信する。

例えば、

```text
data_id = 1
data_id = 2
data_id = 3
data_id = 4
```

という順番で生成された場合、Communication Processはこれらを順次受信する。

ただし、通信処理の完了順とSensorDataの生成順は同一とは限らない。

例えば、

```text
data_id = 10
```

の通信中にリトライが発生しても、

```text
data_id = 11
```

のデータ生成自体はSensor Process側で継続する。

---

# 14. JSONデータ

Communication Processは、SensorDataをCloudflare Workerへ送信する際にJSON形式へ変換する。

送信するJSONは以下を基本とする。

```text
{
  "data_id": 1,
  "temperature": 25.4,
  "humidity": 60.0,
  "timestamp": 1750000000
}
```

JSONの各項目はSensorDataの各メンバーに対応する。

---

# 15. JSONとSensorDataの対応

対応関係は以下とする。

| SensorData    | JSON          | 内容      |
| ------------- | ------------- | ------- |
| `data_id`     | `data_id`     | データ識別ID |
| `temperature` | `temperature` | 温度      |
| `humidity`    | `humidity`    | 湿度      |
| `timestamp`   | `timestamp`   | センサ取得時刻 |

データの意味を変換せず、そのままCloudflare Workerへ渡す。

---

# 16. JSON変換の担当

JSONへの変換はCloudflareClientが担当する。

```text
Communication Process
        │
        │ SensorData
        ▼
CloudflareClient
        │
        │ JSON
        ▼
Cloudflare Worker
```

Communication Processは、JSON文字列を直接組み立てる処理を基本的には担当しない。

---

# 17. HTTP POST

Cloudflare Workerへのデータ送信にはHTTP POSTを使用する。

```text
POST
```

リクエストボディにSensorDataをJSONとして設定する。

概念的には以下となる。

```text
HTTP POST
Content-Type: application/json

{
  "data_id": 1,
  "temperature": 25.4,
  "humidity": 60.0,
  "timestamp": 1750000000
}
```

---

# 18. HTTPヘッダ

Cloudflare Workerへの通信では、共有シークレットをHTTPヘッダに設定する。

使用するヘッダ名は以下とする。

```text
X-Edge-IoT-Shared-Secret
```

共有シークレットの実際の値はソースコードに記述しない。

---

# 19. 環境変数

Raspberry Pi側では、共有シークレットを環境変数から取得する。

使用する環境変数は以下とする。

```text
CLOUDFLARE_SHARED_SECRET
```

Cloudflare Worker側では、WorkerのSecretとして以下を使用する。

```text
EDGE_IOT_SHARED_SECRET
```

実際の秘密値はGit管理対象に含めない。

---

# 20. Workerで受信するデータ

Cloudflare Workerでは、HTTP POSTのJSONから以下の値を受け取る。

```text
data_id
temperature
humidity
timestamp
```

Workerは受信した値をD1へ保存する。

---

# 21. D1のデータ

D1では、以下のテーブルを使用する。

```text
sensor_data
```

基本的な構造は以下とする。

```text
sensor_data
├── id
├── data_id
├── temperature
├── humidity
└── timestamp
```

---

# 22. D1のカラム

D1のカラム定義は以下を基本とする。

| カラム           | 型       | 内容                       |
| ------------- | ------- | ------------------------ |
| `id`          | INTEGER | D1側の主キー                  |
| `data_id`     | INTEGER | Sensor Processが生成したデータID |
| `temperature` | REAL    | 温度                       |
| `humidity`    | REAL    | 湿度                       |
| `timestamp`   | INTEGER | センサ取得時刻                  |

---

# 23. D1のdata_id

D1では、Sensor Processから送られてきたdata_idをそのまま保存する。

data_idはD1側で新しく生成しない。

```text
Sensor Process
    │
    │ data_id = 100
    ▼
Communication Process
    │
    │ data_id = 100
    ▼
Cloudflare Worker
    │
    │ data_id = 100
    ▼
D1
```

---

# 24. data_idの一意性

`data_id`はセンサデータを識別するために使用する。

D1では同じdata_idが重複して登録されないようにする。

基本的なテーブル定義では、`data_id`にUNIQUE制約を設定する。

```text
data_id INTEGER NOT NULL UNIQUE
```

これにより、通信リトライによって同じSensorDataが再送された場合でも、同じdata_idを持つデータを重複して保存しない構成とする。

---

# 25. D1のidとdata_idの違い

D1には`id`と`data_id`の2つの識別情報が存在する。

### id

D1のレコードを識別するためのD1側のID。

### data_id

Sensor Processが生成したSensorDataを識別するためのID。

```text
D1
│
├── id       → D1側のレコードID
│
└── data_id  → センサデータ側のID
```

この2つを同じものとして扱わない。

---

# 26. timestampの保存

D1にはSensorDataのtimestampをそのまま保存する。

```text
SensorData.timestamp
        │
        ▼
JSON.timestamp
        │
        ▼
D1.timestamp
```

Communication Processで通信時刻に変更しない。

---

# 27. データ変換の全体

SensorDataがD1へ保存されるまでの対応を以下に示す。

```text
DHT11
  │
  │ 温度・湿度
  ▼
SensorData
  │
  ├── data_id
  ├── temperature
  ├── humidity
  └── timestamp
  │
  ▼
Message Queue
  │
  ▼
Communication Process
  │
  ▼
JSON
  │
  ├── data_id
  ├── temperature
  ├── humidity
  └── timestamp
  │
  ▼
Cloudflare Worker
  │
  ▼
D1
  │
  ├── id
  ├── data_id
  ├── temperature
  ├── humidity
  └── timestamp
```

---

# 28. 1件のデータの具体例

例えばDHT11から以下の値を取得したとする。

```text
temperature = 25.4
humidity    = 60.0
```

Sensor Processでは以下のSensorDataを生成する。

```text
data_id    = 1
temperature = 25.4
humidity    = 60.0
timestamp   = 1750000000
```

Message Queueでは、このSensorDataを1メッセージとして送信する。

Communication Processが受信すると、CloudflareClientが以下のJSONへ変換する。

```text
{
  "data_id": 1,
  "temperature": 25.4,
  "humidity": 60.0,
  "timestamp": 1750000000
}
```

Cloudflare Workerが受信し、D1へ保存する。

D1では概念的に以下のようになる。

```text
id | data_id | temperature | humidity | timestamp
---|---------|-------------|----------|-----------
1  | 1       | 25.4        | 60.0     | 1750000000
```

---

# 29. データを変更してよい場所

各データ項目をどこで生成・変更するかを明確にする。

| データ         | 生成・設定          | 以降の処理     |
| ----------- | -------------- | --------- |
| temperature | Sensor Process | 基本的に変更しない |
| humidity    | Sensor Process | 基本的に変更しない |
| timestamp   | Sensor Process | 変更しない     |
| data_id     | Sensor Process | 変更しない     |
| D1のid       | Cloudflare D1  | D1が管理     |

Sensor Processで作成したSensorDataの内容は、Communication Process以降では変更しないことを基本とする。

---

# 30. データの責任範囲

各構成要素のデータに対する責任を以下とする。

### Dht11Sensor

DHT11から温度・湿度を取得する。

### Sensor Process

温度・湿度からSensorDataを作成し、timestampとdata_idを設定する。

### SensorDataMessageQueue

SensorDataをプロセス間で受け渡す。

### Communication Process

SensorDataを受信し、CloudflareClientへ渡す。

### CloudflareClient

SensorDataをJSONへ変換してCloudflare Workerへ送信する。

### Cloudflare Worker

受信したJSONを検証し、D1へ保存する。

### D1

データを永続的に保存する。

---

# 31. データ設計上の基本原則

本システムでは、以下を基本原則とする。

## 31.1 センサ取得時の情報を保持する

temperature、humidity、timestamp、data_idを1つのSensorDataとして扱う。

---

## 31.2 通信処理でデータを変更しない

Communication Processは、センサデータの内容を変更するのではなく、そのままCloudflareへ渡す。

---

## 31.3 リトライ時もdata_idを変更しない

同一データの再送では、同じdata_idを使用する。

---

## 31.4 timestampを通信時刻に変更しない

timestampはセンサ取得時刻を表すため、通信時に更新しない。

---

## 31.5 秘密情報とセンサデータを分離する

共有シークレットはSensorDataに含めない。

共有シークレットは環境変数から取得し、通信時のHTTPヘッダに設定する。

---

# 32. 後続設計との関係

本書で定義したデータ構造を基準として、後続のプログラム設計を行う。

### `08_04_sensor.md`

Sensor ProcessがSensorDataをどのように生成するかを定義する。

### `08_05_ipc.md`

SensorDataをMessage Queueでどのように送受信するかを定義する。

### `08_06_communication.md`

Communication ProcessがSensorDataをどのように扱うかを定義する。

### `08_07_cloudflare_client.md`

SensorDataをどのようにJSONへ変換し、Cloudflare Workerへ送信するかを定義する。

### `08_08_error_retry.md`

通信失敗時にSensorDataとdata_idをどのように扱うかを定義する。

### `08_09_process_flow.md`

SensorDataがシステム内を移動する処理順序を定義する。

### `08_10_source_mapping.md`

ここで定義したデータ設計とC++ソースコードの対応を定義する。

---

# 33. まとめ

本システムでは、1回のセンサ取得データを以下のSensorDataとして扱う。

```text
SensorData
├── temperature
├── humidity
├── timestamp
└── data_id
```

このSensorDataを、

```text
Sensor Process
    ↓
POSIX Message Queue
    ↓
Communication Process
    ↓
CloudflareClient
    ↓
Cloudflare Worker
    ↓
Cloudflare D1
```

という流れで受け渡す。

特に以下の3点を重要な設計ルールとする。

1. `timestamp`はセンサ取得時刻を保持する
2. `data_id`はSensor Processで生成し、通信リトライでも変更しない
3. D1では`data_id`をUNIQUEとして扱い、同一データの重複保存を防止する

これを本システムのデータ設計の基本とする。
