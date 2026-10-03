# 08_07 CloudflareClient設計

## 1. 目的

本書では、Cloudflare WorkerとのHTTPS通信を担当する`CloudflareClient`のプログラム設計を定義する。

`CloudflareClient`は、Communication Processから受け取った`SensorData`をJSONへ変換し、Cloudflare WorkerへHTTPS POSTする。

基本的な位置づけは以下とする。

```text id="7j7xqj"
Communication Process
        ↓
CloudflareClient
        ↓
HTTP / HTTPS
        ↓
Cloudflare Worker
```

---

## 2. 対象ファイル

対象ファイルは以下とする。

```text id="5e7t1r"
include/
└── communication/
    └── CloudflareClient.h

src/
└── communication/
    └── CloudflareClient.cpp
```

---

## 3. CloudflareClientの役割

CloudflareClientは、Cloudflare Workerとの通信処理を担当する。

主な責務は以下とする。

* Cloudflare WorkerのURL管理
* 共有シークレットの設定
* SensorDataのJSON化
* HTTPヘッダー設定
* HTTPS POST
* HTTPタイムアウト設定
* HTTPステータス取得
* 通信エラーの取得
* 通信結果の返却

---

## 4. CloudflareClientが担当しない処理

CloudflareClientは以下を担当しない。

* DHT11の読み取り
* SensorDataの生成
* `data_id`の生成
* Message Queueの操作
* センサ取得周期の管理
* リトライ回数の管理
* リトライ待ち時間の管理
* systemdによるプロセス管理
* D1への直接アクセス

これらは別のコンポーネントが担当する。

---

## 5. Communication Processとの関係

Communication Processは、CloudflareClientを使用して通信を行う。

```text id="j4z4gq"
Communication Process
        │
        │ SensorData
        ▼
CloudflareClient
        │
        │ HTTPS POST
        ▼
Cloudflare Worker
```

Communication Processは、

「HTTPSをどのように実行するか」

を直接意識しない。

---

## 6. CloudflareClientの入力

CloudflareClientが送信対象として受け取るデータは`SensorData`とする。

```text id="1f8xj4"
SensorData
├── temperature
├── humidity
├── timestamp
└── data_id
```

CloudflareClientは、このデータをCloudflare Workerが受け取れるJSON形式へ変換する。

---

## 7. JSONへの変換

SensorDataとJSONの対応は以下とする。

| SensorData    | JSON          |
| ------------- | ------------- |
| `data_id`     | `data_id`     |
| `temperature` | `temperature` |
| `humidity`    | `humidity`    |
| `timestamp`   | `timestamp`   |

例：

```json id="j6n6f2"
{
  "data_id": 10,
  "temperature": 25.4,
  "humidity": 60.0,
  "timestamp": 1750000000
}
```

JSON化によって、SensorDataの意味や値を変更してはいけない。

---

## 8. JSON生成の責務

JSON生成はCloudflareClientが担当する。

そのため、Communication ProcessでJSON文字列を直接組み立てない。

```text id="w6hr6d"
Communication Process
        ↓
SensorData
        ↓
CloudflareClient
        ↓
JSON生成
        ↓
HTTPS POST
```

これにより、通信処理をCloudflareClientへ集約する。

---

## 9. Cloudflare Worker URL

通信先のURLは以下とする。

```text id="r8j53b"
https://raspi-iot.yujimasuda77777.workers.dev/
```

URLはプログラム本体に直接埋め込むのではなく、設定値として扱える構成を基本とする。

---

## 10. 共有シークレット

Cloudflare Workerとの通信では共有シークレットを使用する。

HTTPヘッダーは以下とする。

```text id="kz6pgo"
X-Edge-IoT-Shared-Secret: <secret>
```

Raspberry Pi側では、環境変数から取得する。

```text id="q5yq9m"
CLOUDFLARE_SHARED_SECRET
```

実際の値をソースコードへ直接記述しない。

---

## 11. 共有シークレットの扱い

共有シークレットは機密情報として扱う。

そのため、以下を行わない。

* ソースコードへの直接記述
* Gitへの登録
* ログへの出力
* HTTPレスポンスログへの出力

実際の設定はRaspberry Pi上の設定ファイルで行う。

```text id="z1kq9h"
/etc/raspberry-pi-edge-iot/communication.env
```

---

## 12. HTTPヘッダー

Cloudflare WorkerへのPOSTでは、少なくとも以下のヘッダーを設定する。

```text id="o1y6dz"
Content-Type: application/json
X-Edge-IoT-Shared-Secret: <secret>
```

`Content-Type`によって、送信データがJSONであることを示す。

共有シークレット用ヘッダーによって、Worker側で送信元を確認できるようにする。

---

## 13. HTTPS通信

CloudflareClientはHTTPSを使用してCloudflare Workerへ接続する。

```text id="x8v5e8"
Raspberry Pi
     │
     │ HTTPS
     ▼
Cloudflare Worker
```

通信にはHTTPSを使用し、HTTPでは通信しない。

---

## 14. HTTPタイムアウト

HTTP通信には5秒のタイムアウトを設定する。

```text id="q7n4ae"
通信開始
   ↓
最大5秒
   ↓
応答なし
   ↓
タイムアウト
```

無期限に通信処理が停止することを防ぐ。

タイムアウトが発生した場合は、Communication Process側で再送判定を行う。

---

## 15. HTTPステータス

CloudflareClientはHTTPレスポンスのステータスコードを取得する。

例：

```text id="j7rj0a"
200 → 成功
400 → クライアント側エラー
401 → 認証エラー
404 → リソースなし
500 → サーバ側エラー
503 → サービス利用不可
```

CloudflareClientはステータスコードを取得してCommunication Processへ返す。

ステータスコードの意味に基づくリトライ判断はCommunication Process側で行う。

---

## 16. CloudflareClientとリトライの分離

CloudflareClient自身は、リトライ回数を管理しない。

例えばHTTP 500が発生した場合、

```text id="x5jz1b"
CloudflareClient
      ↓
HTTP 500
      ↓
結果を返す
      ↓
Communication Process
      ↓
再送するか判断
```

という構成とする。

これにより、通信処理とリトライ制御を分離する。

---

## 17. 通信エラー

HTTPステータスを取得できないような通信エラーも発生する可能性がある。

例：

* DNS解決失敗
* TCP接続失敗
* TLS接続失敗
* 接続タイムアウト
* レスポンスタイムアウト

この場合、CloudflareClientは通信失敗として結果を返す。

---

## 18. 通信結果

CloudflareClientからCommunication Processへは、少なくとも以下の情報を返せるようにする。

```text id="2o3y5q"
通信成功／失敗
HTTPステータス
通信エラー情報
```

実装では、この情報を扱いやすい形にまとめる。

---

## 19. CloudflareClientのインターフェース

CloudflareClientの外部インターフェースは、SensorDataを渡して通信結果を取得できる構成とする。

概念的には以下の形とする。

```text id="f5e6b7"
CloudflareClient
        │
        ├── 初期化
        │
        └── SensorData送信
                │
                ▼
           通信結果
```

具体的な戻り値やメンバー変数は、実装時に本設計に従って定義する。

---

## 20. CloudflareClientの初期化

CloudflareClientを使用する前に初期化を行う。

初期化では主に以下を準備する。

* Cloudflare Worker URL
* 共有シークレット
* HTTP通信に必要なリソース

イメージは以下とする。

```text id="4scj5k"
CloudflareClient生成
        ↓
設定読み込み
        ↓
HTTP通信準備
        ↓
送信可能
```

---

## 21. Communication Processからの使用

Communication Processでは、以下のような流れで使用する。

```text id="5w6vcc"
SensorData受信
      ↓
CloudflareClientへ渡す
      ↓
HTTP POST
      ↓
結果取得
      ↓
成功／失敗を判断
```

CloudflareClient内部のHTTP処理をCommunication Processへ漏らさない。

---

## 22. HTTPライブラリ

HTTPS通信にはHTTPクライアントライブラリを使用する。

現在のシステムでは`libcurl`を使用する。

CloudflareClientは、`libcurl`の処理をラップする役割を持つ。

```text id="3o1q5s"
Communication Process
        ↓
CloudflareClient
        ↓
libcurl
        ↓
HTTPS
        ↓
Cloudflare Worker
```

---

## 23. libcurlを直接main.cppから使用しない

Communication Processの`main.cpp`では、libcurl APIを直接使用しない。

例えば、

```text id="e5jzsp"
main.cpp
  ↓
curl_easy_init()
curl_easy_setopt()
curl_easy_perform()
...
```

という構成にはしない。

代わりに、

```text id="xg8lq5"
main.cpp
  ↓
CloudflareClient
  ↓
libcurl
```

とする。

---

## 24. CloudflareClient.cppの責務

`CloudflareClient.cpp`では、CloudflareClientの具体的な処理を実装する。

主な処理は以下とする。

```text id="ddynb9"
SensorData受信
      ↓
JSON生成
      ↓
HTTPリクエスト作成
      ↓
HTTPヘッダー設定
      ↓
タイムアウト設定
      ↓
HTTPS POST
      ↓
HTTPステータス取得
      ↓
結果返却
```

---

## 25. CloudflareClient.hの責務

`CloudflareClient.h`では、CloudflareClientを使用する側に必要な宣言を定義する。

主な内容：

* クラス定義
* コンストラクタ
* デストラクタ
* 初期化関連
* SensorData送信関数
* 通信結果を扱うための定義
* 必要なメンバー変数

実装詳細は`CloudflareClient.cpp`に置く。

---

## 26. ヘッダと実装の対応

基本的に、`.h`と`.cpp`の構造を対応させる。

```text id="p8e8x0"
CloudflareClient.h
        │
        ├── CloudflareClient()
        ├── ~CloudflareClient()
        ├── initialize()
        └── sendSensorData()
                │
                ▼
CloudflareClient.cpp
        │
        ├── CloudflareClient::CloudflareClient()
        ├── CloudflareClient::~CloudflareClient()
        ├── CloudflareClient::initialize()
        └── CloudflareClient::sendSensorData()
```

宣言と実装の順序も、できるだけ一致させる。

---

## 27. データの変更禁止

CloudflareClientは、通信のためにSensorDataの意味を変更しない。

例えば、

```text id="w4ex1b"
temperature = 25.4
humidity = 60.0
data_id = 10
timestamp = 1750000000
```

を受け取った場合、JSONにも同じ値を設定する。

---

## 28. data_idの扱い

`data_id`はCloudflareClientで生成しない。

Sensor Processが生成した値をそのままJSONへ設定する。

```text id="8n7jby"
Sensor Process
data_id = 10
      ↓
Message Queue
data_id = 10
      ↓
Communication Process
data_id = 10
      ↓
CloudflareClient
data_id = 10
```

---

## 29. timestampの扱い

`timestamp`もCloudflareClientで生成しない。

Sensor Processが設定した値をそのままJSONへ設定する。

通信時刻ではなく、センサ取得時刻を送信する。

---

## 30. 通信成功の判定

基本的にはHTTPステータスが2xxの場合を成功とする。

```text id="m7f6c2"
HTTP 2xx
    ↓
CloudflareClient
    ↓
成功結果
```

それ以外のHTTPステータスは、Communication Process側でエラーとして扱う。

---

## 31. CloudflareClientのログ

CloudflareClientでは、通信障害の調査に必要な情報をログへ出力できるようにする。

例えば以下を対象とする。

* HTTPステータス
* 通信エラー
* タイムアウト
* 接続失敗

ただし、共有シークレットなどの機密情報はログへ出力しない。

---

## 32. CloudflareClientの終了

Communication Process終了時には、CloudflareClientが使用しているHTTP通信関連リソースを解放する。

```text id="s7nqvx"
Communication Process終了
        ↓
CloudflareClient終了処理
        ↓
HTTPリソース解放
        ↓
プロセス終了
```

---

## 33. 設計上のポイント

CloudflareClientでは以下を基本方針とする。

### 33.1 HTTP処理を隠蔽する

Communication Processからlibcurlの詳細を隠す。

### 33.2 SensorDataをそのまま送る

データの意味や値を変更しない。

### 33.3 リトライは担当しない

CloudflareClientは1回のHTTP送信を担当する。

リトライはCommunication Process側で行う。

### 33.4 設定値と機密情報を分離する

URLや共有シークレットなどをソースコードへ直接埋め込まない。

### 33.5 通信結果を返す

Communication Processが成功／失敗を判断できる情報を返す。

---

## 34. 上位設計との対応

本書は以下の設計を具体化する。

* `03_software_architecture.md`

  * Communication Process
  * Cloudflare Worker

* `04_data_ipc_communication.md`

  * HTTPS
  * JSON
  * HTTP通信

* `05_operation_recovery.md`

  * タイムアウト
  * 通信エラー
  * リトライ

* `06_system_management_security.md`

  * 共有シークレット
  * 設定ファイル

---

## 35. 次の設計書との関係

次の`08_08_error_retry.md`では、通信エラーやHTTPステータスごとの処理と、リトライ回数・待ち時間を具体的に定義する。

```text id="0z8s3f"
08_06_communication
        ↓
CloudflareClient
        ↓
通信結果
        ↓
08_08_error_retry
        ↓
再送判断
```

---

## 36. まとめ

CloudflareClientは、Communication Processから受け取ったSensorDataをCloudflare WorkerへHTTPS POSTするクラスである。

基本的な役割は以下となる。

```text id="zj5j2q"
SensorData
    ↓
CloudflareClient
    ↓
JSON生成
    ↓
HTTPヘッダー設定
    ↓
libcurl
    ↓
HTTPS POST
    ↓
Cloudflare Worker
    ↓
HTTPステータス
    ↓
Communication Process
```

CloudflareClientはHTTP通信の詳細を担当する一方、センサ処理、Message Queue、リトライ制御、D1保存などは担当しない。

これにより、各処理の責務を明確に分離する。
