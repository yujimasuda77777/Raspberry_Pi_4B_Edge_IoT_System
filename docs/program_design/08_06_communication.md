# 08_06 通信処理設計

## 1. 目的

本書では、Communication Processのプログラム設計を定義する。

Communication Processは、Message QueueからSensorDataを受信し、Cloudflare WorkerへHTTPSで送信する。

基本的な役割は以下である。

```text
Message Queue
      ↓
Communication Process
      ↓
Cloudflare Worker
```

---

## 2. Communication Processの役割

Communication Processは以下を担当する。

* Message QueueからSensorDataを受信する
* 受信したSensorDataをCloudflareClientへ渡す
* Cloudflare WorkerへのHTTP通信を実行する
* 通信結果を確認する
* 必要に応じて再送する
* 通信関連のログを出力する

---

## 3. Communication Processが担当しない処理

Communication Processは以下の処理を直接担当しない。

* DHT11からのデータ取得
* GPIO制御
* SensorDataの生成
* `data_id`の新規生成
* `timestamp`の新規生成
* D1への直接アクセス
* ブラウザへのデータ表示

これらは別のコンポーネントが担当する。

---

## 4. Communication Processのファイル

対象ファイルは以下とする。

```text
include/
└── communication/
    └── CloudflareClient.h

src/
└── communication/
    ├── main.cpp
    └── CloudflareClient.cpp
```

また、IPC処理には以下を使用する。

```text
include/
└── ipc/
    └── SensorDataMessageQueue.h

src/
└── ipc/
    └── SensorDataMessageQueue.cpp
```

---

## 5. Communication Processの基本処理

Communication Processの基本的な処理順序は以下とする。

```text
Communication Process起動
        ↓
設定読み込み
        ↓
Message Queue初期化
        ↓
CloudflareClient初期化
        ↓
SensorData受信待ち
        ↓
SensorData受信
        ↓
Cloudflare Workerへ送信
        ↓
結果確認
        ↓
次のSensorData受信待ち
```

---

## 6. Message Queueからの受信

Communication ProcessはMessage QueueからSensorDataを受信する。

データを受信するまで待機する。

```text
Queue
  ↓
SensorData受信
  ↓
Communication Process
```

受信したSensorDataは、そのまま通信処理へ渡す。

---

## 7. SensorDataの扱い

Communication Processが受信するSensorDataは以下の4項目を持つ。

```text
temperature
humidity
timestamp
data_id
```

Communication Processでは、これらの値をCloudflare Workerへ送信する。

---

## 8. HTTP通信

Communication ProcessはCloudflare WorkerへHTTPS POSTを行う。

送信先は以下とする。

```text
https://raspi-iot.yujimasuda77777.workers.dev/
```

HTTPメソッドはPOSTを使用する。

```text
POST /
```

---

## 9. POSTデータ

Cloudflare Workerへ送信するJSONは以下の形式とする。

```json
{
  "data_id": 1,
  "temperature": 25.4,
  "humidity": 60.0,
  "timestamp": 1750000000
}
```

SensorDataの内容をJSONへ変換して送信する。

---

## 10. SensorDataからJSONへの変換

変換関係は以下とする。

| SensorData    | JSON          |
| ------------- | ------------- |
| `data_id`     | `data_id`     |
| `temperature` | `temperature` |
| `humidity`    | `humidity`    |
| `timestamp`   | `timestamp`   |

値の意味は変換しない。

---

## 11. HTTPヘッダー

HTTP POSTでは、JSONデータを送信することを示す。

主なヘッダーは以下とする。

```text
Content-Type: application/json
```

また、Cloudflare Workerとの通信には共有シークレットを使用する。

```text
X-Edge-IoT-Shared-Secret: <secret>
```

---

## 12. 共有シークレット

共有シークレットはソースコードに直接記述しない。

Raspberry Pi側では環境変数から取得する。

環境変数名：

```text
CLOUDFLARE_SHARED_SECRET
```

実際の設定ファイル：

```text
/etc/raspberry-pi-edge-iot/communication.env
```

Git管理対象には実際のシークレットを保存しない。

---

## 13. 通信設定

Cloudflare WorkerのURLや共有シークレットなど、通信に必要な設定はプログラム本体と分離する。

設定値の例は以下のファイルで管理する。

```text
config/
└── communication.env.example
```

実際のシークレットはRaspberry Pi上の設定ファイルに設定する。

---

## 14. CloudflareClientの役割

Communication ProcessからHTTP通信の詳細を分離するため、`CloudflareClient`を使用する。

```text
Communication Process
        ↓
CloudflareClient
        ↓
HTTPS
        ↓
Cloudflare Worker
```

Communication ProcessはHTTPライブラリの細かな処理を直接扱わない。

---

## 15. Communication ProcessとCloudflareClientの責務分担

### Communication Process

担当：

* SensorDataの受信
* CloudflareClientの呼び出し
* 通信結果の判断
* リトライ制御
* 処理全体のループ

### CloudflareClient

担当：

* HTTP通信
* JSON生成
* HTTPヘッダー設定
* HTTPS接続
* HTTPステータス取得
* HTTP通信エラーの取得

---

## 16. 通信結果の確認

Cloudflare Workerへの送信後、HTTPステータスを確認する。

基本的な考え方は以下とする。

```text
HTTP通信
   ↓
ステータス確認
   │
   ├─ 2xx → 成功
   │
   ├─ 4xx → 基本的に再送しない
   │
   └─ 5xx → 再送
```

通信そのものに失敗した場合も、再送対象とする。

---

## 17. HTTP 2xxの場合

HTTPステータスが2xxの場合、送信成功とする。

```text
POST
 ↓
HTTP 2xx
 ↓
送信成功
 ↓
次のSensorDataを受信
```

正常に送信できたSensorDataは、Communication Process内で再送しない。

---

## 18. HTTP 4xxの場合

HTTPステータスが4xxの場合、基本的には再送しない。

4xxは、送信内容や認証など、送信側の条件によるエラーである可能性があるためである。

```text
POST
 ↓
HTTP 4xx
 ↓
送信失敗
 ↓
再送しない
 ↓
次のSensorData
```

具体的なエラー分類は`08_08_error_retry.md`で定義する。

---

## 19. HTTP 5xxの場合

HTTPステータスが5xxの場合、Cloudflare Worker側の一時的なエラーである可能性があるため、再送対象とする。

```text
POST
 ↓
HTTP 5xx
 ↓
再送
```

再送回数や待ち時間については`08_08_error_retry.md`で定義する。

---

## 20. 通信エラーの場合

以下のような通信エラーが発生した場合は、再送対象とする。

* DNS解決失敗
* TCP接続失敗
* TLS接続失敗
* 接続タイムアウト
* レスポンスタイムアウト
* その他の一時的な通信エラー

基本的な処理は以下とする。

```text
通信
 ↓
通信エラー
 ↓
再送判定
 ↓
再送
```

---

## 21. HTTPタイムアウト

HTTP通信にはタイムアウトを設定する。

設計上のHTTPタイムアウトは5秒とする。

通信処理が無期限に待機することを防ぐ。

```text
HTTP開始
   ↓
最大5秒
   ↓
応答なし
   ↓
タイムアウト
```

---

## 22. リトライ

通信エラーやHTTP 5xxが発生した場合は、同じSensorDataを再送する。

再送時に新しいSensorDataを生成してはいけない。

```text
data_id = 10
      ↓
1回目送信
      ↓
失敗
      ↓
data_id = 10
      ↓
再送
```

同じデータを同じ`data_id`で送信する。

---

## 23. リトライ時の注意

リトライによって以下の値を変更しない。

```text
data_id
temperature
humidity
timestamp
```

再送は「同じSensorDataをもう一度送る」処理とする。

---

## 24. Cloudflare Workerからの応答

Cloudflare WorkerからHTTPレスポンスを受信する。

Communication Processでは、主にHTTPステータスを使用して送信結果を判断する。

Workerから返される本文については、必要に応じてログへ出力する。

ただし、共有シークレットなどの機密情報をログへ出力してはいけない。

---

## 25. 通信成功後の処理

SensorDataの送信が成功した場合、Communication Processは次のSensorDataを受信する。

```text
SensorData受信
      ↓
HTTP送信
      ↓
成功
      ↓
次のSensorData受信
```

Communication Processは、送信成功したデータをローカルに保存しない。

データの永続保存はCloudflare D1側で行う。

---

## 26. 通信失敗後の処理

再送対象のエラーの場合は、定義された回数まで再送する。

すべての再送に失敗した場合は、エラーをログへ出力して、そのSensorDataの処理を終了する。

```text
送信
 ↓
失敗
 ↓
再送
 ↓
失敗
 ↓
再送
 ↓
すべて失敗
 ↓
エラー記録
 ↓
次のSensorData
```

具体的な回数と待ち時間は`08_08_error_retry.md`で定義する。

---

## 27. Communication Processのメインループ

`main.cpp`では、以下の処理を繰り返す。

```text
while
  ↓
QueueからSensorData受信
  ↓
CloudflareClientで送信
  ↓
通信結果確認
  ↓
必要ならリトライ
  ↓
次のSensorData受信
```

通信処理の詳細はCloudflareClientへ分離する。

---

## 28. Communication Processの責務範囲

Communication Processの責務をまとめると以下となる。

```text
Message Queue
      ↓
SensorData受信
      ↓
送信判断
      ↓
CloudflareClient
      ↓
HTTP POST
      ↓
結果確認
      ↓
リトライ判断
```

---

## 29. 通信処理で変更しないデータ

Communication Processは、通信のためにSensorDataの値を変更しない。

以下の値は維持する。

```text
temperature
humidity
timestamp
data_id
```

JSONへの変換はデータ形式の変換であり、SensorData自体を書き換えるものではない。

---

## 30. 通信処理とデータ保存の分離

Communication ProcessはCloudflare Workerへデータを送信する。

Cloudflare Workerは受信したデータをD1へ保存する。

したがって、Raspberry Pi側の役割は以下となる。

```text
SensorData
   ↓
HTTPS POST
   ↓
Cloudflare Worker
   ↓
D1
```

Communication ProcessがD1へ直接接続することはない。

---

## 31. Communication Processの終了

Communication Processを終了する場合は、以下のリソースを適切に終了する。

* Message Queue
* CloudflareClient
* HTTP通信関連リソース

正常終了時には必要な終了処理を行ってからプロセスを終了する。

---

## 32. 設計上のポイント

Communication Processでは以下を基本方針とする。

### 32.1 センサ処理と通信処理を分離する

Communication ProcessはDHT11を直接扱わない。

### 32.2 HTTP処理をCloudflareClientへ分離する

Communication Processは通信ライブラリの詳細を意識しない。

### 32.3 SensorDataを維持する

通信処理によってSensorDataの値を変更しない。

### 32.4 通信失敗時に再送する

一時的な通信障害やHTTP 5xxを想定して再送する。

### 32.5 無期限に待機しない

HTTP通信には5秒のタイムアウトを設定する。

---

## 33. 上位設計との対応

本書は以下の設計を具体化する。

* `03_software_architecture.md`

  * Communication Process
  * Cloudflare Workerとの通信

* `04_data_ipc_communication.md`

  * HTTPS通信
  * JSONデータ

* `05_operation_recovery.md`

  * 通信失敗
  * リトライ
  * タイムアウト

* `06_system_management_security.md`

  * 共有シークレット
  * 設定ファイル

---

## 34. 次の設計書との関係

次の`08_07_cloudflare_client.md`では、Communication Processから呼び出されるCloudflareClientについて、クラスの責務と処理内容を具体化する。

```text
08_05_ipc
      ↓
Communication Process
      ↓
08_06_communication
      ↓
CloudflareClient
      ↓
08_07_cloudflare_client
```

---

## 35. まとめ

Communication Processは、Message Queueから受信したSensorDataをCloudflare WorkerへHTTPS POSTする。

基本的な処理は以下である。

```text
Message Queue
      ↓
SensorData受信
      ↓
CloudflareClient
      ↓
HTTPS POST
      ↓
Cloudflare Worker
      ↓
結果確認
      ↓
成功 → 次のデータ
失敗 → 必要に応じて再送
```

Communication Processは、センサ処理とHTTP通信の間をつなぐ役割を持つ。

HTTP通信そのものの詳細はCloudflareClientへ分離する。
