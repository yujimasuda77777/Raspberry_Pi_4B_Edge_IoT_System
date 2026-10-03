# 08_08 エラー・リトライ設計

## 1. 目的

本書では、本システムで発生するエラーに対する基本的な処理と、通信時のリトライ処理を定義する。

対象となる主な処理は以下である。

* センサ読み取り
* Message Queue
* HTTPS通信
* HTTPステータス
* 通信タイムアウト
* Communication Process

---

## 2. エラー処理の基本方針

本システムでは、1回のエラーですぐにプロセス全体を終了させない。

基本的には、

```text
エラー発生
    ↓
エラーを記録
    ↓
可能なら処理を継続
```

とする。

ただし、初期化など、処理を継続できないエラーについてはプロセスを終了する場合がある。

---

## 3. エラーの分類

主なエラーを以下のように分類する。

| 分類          | 例            | 基本処理          |
| ----------- | ------------ | ------------- |
| センサエラー      | DHT11読み取り失敗  | 次周期で再取得       |
| Queue送信エラー  | Queue満杯      | 現在データを破棄して次周期 |
| Queue初期化エラー | Queueを開けない   | プロセス終了        |
| 通信エラー       | DNS、TCP、TLS等 | リトライ          |
| タイムアウト      | HTTP応答なし     | リトライ          |
| HTTP 4xx    | 400、401等     | 基本的にリトライしない   |
| HTTP 5xx    | 500、503等     | リトライ          |

---

## 4. センサ読み取りエラー

DHT11からデータを取得できなかった場合、その周期のデータは作成しない。

```text
DHT11読み取り
      ↓
   失敗
      ↓
ログ出力
      ↓
現在周期を終了
      ↓
5秒後に再取得
```

Sensor Process自体は終了しない。

---

## 5. センサ読み取りエラー時のdata_id

センサ読み取りに失敗した場合、SensorDataを生成しない。

したがって、その周期では`data_id`も消費しない。

例えば、

```text
1回目 → data_id = 1
2回目 → 読み取り失敗
3回目 → data_id = 2
```

とする。

センサ読み取りに成功したデータだけがSensorDataとなる。

---

## 6. Message Queue送信エラー

Sensor ProcessがMessage QueueへSensorDataを送信できなかった場合は、エラーとして記録する。

基本的には現在のSensorDataを破棄し、次のセンサ周期へ進む。

```text
SensorData生成
      ↓
Queue送信
      ↓
失敗
      ↓
ログ出力
      ↓
現在データを破棄
      ↓
次の周期
```

Queue送信のためにSensor Processを無期限に停止させない。

---

## 7. Message Queue初期化エラー

Message Queueを開けないなど、Sensor Processの動作に必要な初期化に失敗した場合は、センサ処理を開始しない。

```text
Sensor Process起動
      ↓
Queue初期化
      ↓
失敗
      ↓
エラー出力
      ↓
プロセス終了
```

このような初期化エラーは、実行中の一時的なセンサエラーとは区別する。

---

## 8. Communication ProcessのQueue受信エラー

Communication ProcessがMessage Queueからデータを受信できなかった場合、エラー内容に応じて処理する。

一時的なエラーの場合は、受信待ち処理を継続する。

Communication Process自体を無条件に終了しない。

---

## 9. HTTPS通信エラー

Cloudflare WorkerへのHTTPS通信でエラーが発生した場合は、再送対象とする。

対象例：

* DNS解決失敗
* TCP接続失敗
* TLS接続失敗
* 接続タイムアウト
* レスポンスタイムアウト
* その他の通信ライブラリ上の一時的な通信エラー

基本的な流れは以下とする。

```text
HTTP送信
   ↓
通信エラー
   ↓
リトライ
```

---

## 10. HTTPタイムアウト

HTTP通信のタイムアウトは5秒とする。

```text
HTTP送信開始
      ↓
5秒以内に応答
      │
      ├─ 応答あり → 結果確認
      │
      └─ 応答なし → タイムアウト
                         ↓
                       リトライ
```

これにより、通信先の障害などによってCommunication Processが長時間停止することを防ぐ。

---

## 11. HTTP 2xx

HTTPステータスが2xxの場合、通信成功とする。

```text
HTTP 2xx
   ↓
成功
   ↓
リトライしない
   ↓
次のSensorData
```

---

## 12. HTTP 4xx

HTTPステータスが4xxの場合、基本的にリトライしない。

例：

```text
400 Bad Request
401 Unauthorized
403 Forbidden
404 Not Found
```

これらは、送信内容や認証、URLなどの設定に問題がある可能性があるため、一時的な通信障害とは扱わない。

```text
HTTP 4xx
   ↓
送信失敗
   ↓
リトライしない
   ↓
ログ出力
   ↓
次のSensorData
```

---

## 13. HTTP 5xx

HTTPステータスが5xxの場合は、リトライ対象とする。

例：

```text
500 Internal Server Error
502 Bad Gateway
503 Service Unavailable
504 Gateway Timeout
```

基本的な処理は以下とする。

```text
HTTP 5xx
   ↓
リトライ
```

---

## 14. リトライ対象

以下をリトライ対象とする。

### 通信エラー

* DNS解決失敗
* TCP接続失敗
* TLS接続失敗
* 接続タイムアウト
* レスポンスタイムアウト

### HTTPエラー

* HTTP 5xx

---

## 15. リトライ対象外

基本的に以下はリトライしない。

### HTTP 2xx

成功しているため、再送しない。

### HTTP 4xx

送信側の条件に問題がある可能性があるため、基本的に再送しない。

---

## 16. リトライ回数

1つのSensorDataに対する送信回数は、最大4回とする。

内訳は以下である。

```text
1回目：初回送信
2回目：リトライ
3回目：リトライ
4回目：リトライ
```

つまり、リトライ回数は最大3回である。

```text
最大送信回数 = 4回
```

---

## 17. リトライ待ち時間

リトライ前には待ち時間を設定する。

待ち時間は以下とする。

| 送信      | 待ち時間 |
| ------- | ---: |
| 初回送信    |   なし |
| 1回目リトライ |   1秒 |
| 2回目リトライ |   2秒 |
| 3回目リトライ |   4秒 |

イメージ：

```text
初回送信
   ↓ 失敗
1秒待つ
   ↓
1回目リトライ
   ↓ 失敗
2秒待つ
   ↓
2回目リトライ
   ↓ 失敗
4秒待つ
   ↓
3回目リトライ
```

---

## 18. リトライ時のSensorData

リトライ時には、最初に受信したSensorDataをそのまま使用する。

新しいSensorDataを生成しない。

```text
SensorData
data_id = 10
temperature = 25.4
humidity = 60.0
timestamp = 1750000000
       ↓
初回送信
       ↓
失敗
       ↓
同じSensorData
       ↓
リトライ
```

---

## 19. リトライ時に変更しない値

リトライ時には以下を変更しない。

```text
data_id
temperature
humidity
timestamp
```

特に`data_id`をリトライごとに変更してはいけない。

---

## 20. data_idと重複登録

同じSensorDataをリトライすると、Cloudflare Workerに同じデータが複数回到達する可能性がある。

本システムでは、`data_id`をデータ識別子として使用する。

D1では`data_id`を一意に扱う。

```text
data_id INTEGER NOT NULL UNIQUE
```

そのため、同じ`data_id`を持つデータを重複して保存しない設計とする。

---

## 21. リトライ成功

リトライによって送信に成功した場合、そのSensorDataの処理を完了する。

```text
初回送信
   ↓
失敗
   ↓
1秒待機
   ↓
リトライ
   ↓
HTTP 200
   ↓
成功
   ↓
次のSensorData
```

---

## 22. リトライ全失敗

最大4回の送信をすべて失敗した場合、そのSensorDataの処理を終了する。

```text
初回送信
   ↓
失敗
   ↓
リトライ
   ↓
失敗
   ↓
リトライ
   ↓
失敗
   ↓
リトライ
   ↓
失敗
   ↓
エラー記録
   ↓
次のSensorData
```

Communication Process自体は継続する。

---

## 23. リトライ失敗データの扱い

最大回数まで送信しても成功しなかったSensorDataは、Communication Process内で永久に保持しない。

つまり、無限リトライは行わない。

```text
最大4回
   ↓
すべて失敗
   ↓
エラー記録
   ↓
次のSensorData
```

---

## 24. エラー時のログ

エラー発生時には、原因を確認できる情報をログへ出力する。

主な情報：

* エラーの種類
* HTTPステータス
* リトライ回数
* 通信エラー内容
* `data_id`

例：

```text
HTTP request failed.
data_id=10
retry=2
```

ただし、共有シークレットなどの機密情報はログへ出力しない。

---

## 25. エラー処理の責務

エラー処理は、エラーが発生した場所に応じて担当を分ける。

### Dht11Sensor

センサ読み取り結果を返す。

### Sensor Process

センサエラーやQueue送信エラーを処理する。

### CloudflareClient

HTTP通信を実行し、通信結果を返す。

### Communication Process

HTTPステータスや通信結果を確認し、リトライするか判断する。

---

## 26. リトライ制御の責務

リトライ処理はCommunication Processが担当する。

CloudflareClientは1回のHTTP送信を担当する。

```text
Communication Process
        │
        │ 送信要求
        ▼
CloudflareClient
        │
        │ 1回送信
        ▼
      結果
        │
        ▼
Communication Process
        │
        ├─ 成功 → 次のデータ
        │
        └─ 失敗 → リトライ判断
```

---

## 27. エラー処理全体

システム全体では以下のように処理する。

```text
DHT11
  │
  ├─ 成功
  │    ↓
  │ SensorData
  │    ↓
  │ Queue
  │
  └─ 失敗
       ↓
     ログ
       ↓
     次周期


Queue
  │
  ├─ 送信成功
  │    ↓
  │ Communication Process
  │
  └─ 送信失敗
       ↓
     ログ
       ↓
     次周期


HTTPS
  │
  ├─ 2xx
  │    ↓
  │  成功
  │
  ├─ 4xx
  │    ↓
  │  リトライしない
  │
  ├─ 5xx
  │    ↓
  │  リトライ
  │
  └─ 通信エラー
       ↓
     リトライ
```

---

## 28. 設計上のポイント

### 28.1 無限リトライをしない

1つのSensorDataに対して最大4回まで送信する。

### 28.2 一時的な障害にはリトライする

HTTP 5xxや通信エラーはリトライ対象とする。

### 28.3 4xxは基本的に再送しない

送信側の条件に問題がある可能性があるため、同じデータを繰り返し送信しない。

### 28.4 SensorDataを保持する

リトライでは新しいSensorDataを生成せず、同じデータを使用する。

### 28.5 プロセス全体を止めない

1件のSensorDataの送信に失敗しても、Communication Processは次のデータ処理へ進む。

---

## 29. 上位設計との対応

本書は以下の設計を具体化する。

* `04_data_ipc_communication.md`

  * データ受け渡し
  * HTTPS通信

* `05_operation_recovery.md`

  * エラー処理
  * リトライ
  * タイムアウト
  * プロセス継続

* `06_system_management_security.md`

  * ログ
  * 共有シークレット

---

## 30. 次の設計書との関係

次の`08_09_process_flow.md`では、ここまでに定義した各処理を、Sensor ProcessとCommunication Processの起動から終了までの流れとして整理する。

```text
08_04 センサ
      ↓
08_05 IPC
      ↓
08_06 通信
      ↓
08_07 CloudflareClient
      ↓
08_08 エラー・リトライ
      ↓
08_09 プロセスフロー
```

---

## 31. まとめ

本システムでは、エラーが発生しても可能な限り処理を継続する。

通信については、以下のルールとする。

```text
HTTP 2xx
    → 成功

HTTP 4xx
    → 基本的にリトライしない

HTTP 5xx
    → リトライ

通信エラー
    → リトライ

タイムアウト
    → リトライ
```

1つのSensorDataについて、

```text
初回送信
+
最大3回のリトライ
=
最大4回の送信
```

とする。

リトライ間隔は、

```text
1秒 → 2秒 → 4秒
```

とし、リトライ時も同じ`data_id`、`temperature`、`humidity`、`timestamp`を使用する。
