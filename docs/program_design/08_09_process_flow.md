# 08_09 プロセスフロー設計

## 1. 目的

本書では、Sensor ProcessとCommunication Processの起動から終了までの処理フローを定義する。

これまでのプログラム設計を、実際のプログラムが動作する順番に整理する。

---

## 2. 全体構成

本システムでは、2つのプロセスが連携して動作する。

```text id="q5qk8m"
┌─────────────────────┐
│   Sensor Process    │
│                     │
│ DHT11読み取り       │
│ SensorData生成      │
└──────────┬──────────┘
           │
           │ Message Queue
           ▼
┌─────────────────────┐
│ Communication       │
│ Process             │
│                     │
│ SensorData受信      │
│ HTTPS送信           │
└──────────┬──────────┘
           │
           │ HTTPS
           ▼
┌─────────────────────┐
│ Cloudflare Worker   │
└──────────┬──────────┘
           │
           ▼
      Cloudflare D1
```

---

## 3. プロセスの起動

システム起動後、systemdによって各プロセスが起動する。

対象サービス：

```text id="m6d2e7"
sensor-process.service
communication-process.service
```

基本的には、

```text id="i9i8z3"
OS起動
  ↓
systemd
  ↓
Sensor Process
  ↓
Communication Process
```

という形で動作する。

Communication Processはネットワークが利用可能になってから起動する。

---

## 4. Sensor Process起動フロー

Sensor Process起動時の処理は以下とする。

```text id="m9v7a3"
Sensor Process起動
      ↓
Dht11Sensor生成・初期化
      ↓
Message Queue初期化
      ↓
定常処理開始
```

初期化に失敗した場合は、エラーをログへ出力してプロセスを終了する。

---

## 5. Communication Process起動フロー

Communication Process起動時の処理は以下とする。

```text id="4b5qfz"
Communication Process起動
      ↓
設定読み込み
      ↓
Message Queue初期化
      ↓
CloudflareClient初期化
      ↓
受信待ち開始
```

初期化に必要な設定が取得できない場合など、通信処理を開始できない場合はエラーとして扱う。

---

## 6. Sensor Processの定常処理

Sensor Processは5秒周期でセンサデータを取得する。

基本フロー：

```text id="5c7nbs"
センサ読み取り
      ↓
成功？
  ┌───┴───┐
 YES      NO
  ↓        ↓
SensorData  ログ出力
生成        ↓
  ↓       次周期
Queue送信
  ↓
5秒待機
  ↓
繰り返し
```

---

## 7. SensorData生成フロー

DHT11の読み取りに成功するとSensorDataを生成する。

```text id="5y2y2y"
DHT11
  ↓
temperature取得
  ↓
humidity取得
  ↓
data_id設定
  ↓
timestamp設定
  ↓
SensorData完成
```

この時点で、通信に必要なデータがすべて揃う。

---

## 8. data_id生成フロー

`data_id`はSensor Processが管理する。

SensorDataを生成するたびに、次の識別番号を設定する。

```text id="4f98yq"
SensorData生成
     ↓
data_id = 次の番号
```

センサ読み取りに失敗した場合はSensorDataを生成しないため、基本的に`data_id`も消費しない。

---

## 9. timestamp生成フロー

`timestamp`はSensorDataを生成するときに取得する。

```text id="7k5z0r"
DHT11読み取り成功
      ↓
現在時刻取得
      ↓
timestamp設定
```

単位はUTC Unix秒とする。

---

## 10. Message Queue送信フロー

SensorDataが完成したらMessage Queueへ送信する。

```text id="l1hj2j"
SensorData完成
      ↓
Queue送信
      │
      ├─ 成功 → 5秒待機
      │
      └─ 失敗 → ログ出力 → 5秒待機
```

Queue送信失敗時にSensor Processを停止しない。

---

## 11. Communication Processの受信フロー

Communication ProcessはQueueからSensorDataを受信するまで待機する。

```text id="7qk9uk"
受信待ち
   ↓
SensorData受信
   ↓
通信処理
   ↓
次の受信待ち
```

データがない場合は、そのまま待機する。

---

## 12. HTTP送信フロー

SensorDataを受信したらCloudflare Workerへ送信する。

```text id="p5y7g8"
SensorData
      ↓
CloudflareClient
      ↓
JSON生成
      ↓
HTTPヘッダー設定
      ↓
HTTPS POST
      ↓
HTTPレスポンス
```

---

## 13. 通信結果の判定

HTTPレスポンスを受け取ったら、通信結果を確認する。

```text id="5sq6v3"
HTTPレスポンス
      ↓
ステータス確認
      │
      ├─ 2xx → 成功
      │
      ├─ 4xx → 失敗・再送しない
      │
      └─ 5xx → リトライ
```

HTTP通信そのものに失敗した場合もリトライ対象とする。

---

## 14. 通信成功フロー

HTTP 2xxの場合は通信成功とする。

```text id="6g0w8a"
HTTPS POST
   ↓
HTTP 2xx
   ↓
成功
   ↓
次のSensorData受信
```

---

## 15. 通信失敗フロー

リトライ対象の通信失敗の場合は、最大回数まで再送する。

```text id="f0j6fy"
HTTPS POST
   ↓
通信失敗
   ↓
リトライ判定
   ↓
待機
   ↓
再送
```

---

## 16. リトライフロー

1つのSensorDataに対する最大送信回数は4回とする。

```text id="h4m4cv"
初回送信
   ↓
失敗
   ↓
1秒待機
   ↓
1回目リトライ
   ↓
失敗
   ↓
2秒待機
   ↓
2回目リトライ
   ↓
失敗
   ↓
4秒待機
   ↓
3回目リトライ
```

---

## 17. リトライ成功

途中のリトライで通信に成功した場合、そのSensorDataの処理を終了する。

```text id="7q8c2k"
初回送信
   ↓
失敗
   ↓
1秒
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

## 18. リトライ全失敗

4回すべての送信に失敗した場合は、そのSensorDataの処理を終了する。

```text id="t7d5f4"
最大4回送信
      ↓
すべて失敗
      ↓
エラーログ
      ↓
次のSensorData受信
```

Communication Process自体は終了しない。

---

## 19. リトライ時のデータ

リトライ時には最初に受信したSensorDataをそのまま使用する。

```text id="2x7q6a"
data_id = 10
temperature = 25.4
humidity = 60.0
timestamp = 1750000000
```

これらを変更せずに再送する。

---

## 20. Sensor ProcessとCommunication Processの並行動作

2つのプロセスは独立して動作する。

例えば、

```text id="7f9w2q"
Sensor Process
    │
    ├─ DHT11読み取り
    │
    ├─ Queue送信
    │
    └─ 5秒待機
             │
             │
Communication Process
    │
    ├─ Queue受信
    │
    ├─ HTTPS送信
    │
    └─ 次の受信待ち
```

Sensor Processが5秒ごとにデータを生成する一方、Communication ProcessはQueueからデータを受け取ったタイミングで通信処理を行う。

---

## 21. Communication Processが遅い場合

Communication ProcessがHTTPS通信などで時間がかかっている場合、Sensor Processは独立して動作する。

生成されたSensorDataはQueueへ蓄積される。

```text id="lbyi0q"
Sensor Process
   ↓
data 1
   ↓
data 2
   ↓
data 3
   ↓
Queue
   ↓
Communication Process
```

Queueの最大容量は100メッセージとする。

---

## 22. Queueが満杯になった場合

Queueが満杯の場合、Sensor Processの送信は失敗する。

```text id="y2p1qu"
Queue
[100件]
   ↓
新しいSensorData
   ↓
送信失敗
   ↓
ログ
   ↓
次周期
```

Sensor ProcessはQueueの空きを無期限に待たない。

---

## 23. Communication Processが停止した場合

Communication Processが停止している場合でも、Sensor Processは動作を継続する。

Queueに空きがある間はSensorDataを送信できる。

```text id="q4h4eq"
Sensor Process
     ↓
Message Queue
     ↓
Communication Process停止
```

Queueが満杯になった場合は、新しいデータのQueue送信が失敗する。

---

## 24. Sensor Processが停止した場合

Sensor Processが停止すると、新しいSensorDataは生成されなくなる。

Communication ProcessはQueueに残っているSensorDataを処理した後、次のデータを待機する。

```text id="6g7h4a"
Sensor Process停止
      ↓
新規データなし
      ↓
Queue内データ処理
      ↓
受信待ち
```

---

## 25. Cloudflare Worker側の処理

Communication ProcessからHTTPS POSTされたSensorDataはCloudflare Workerで受信する。

基本的な流れは以下とする。

```text id="8z8zqf"
Raspberry Pi
      ↓
HTTPS POST
      ↓
Cloudflare Worker
      ↓
データ確認
      ↓
D1へ保存
```

Cloudflare WorkerとD1の詳細処理は、Raspberry Pi側プログラム設計の対象外とする。

---

## 26. D1への保存

Cloudflare Workerは受信したSensorDataをD1へ保存する。

保存対象：

```text id="1g7a6w"
data_id
temperature
humidity
timestamp
```

D1では`data_id`を一意に扱う。

---

## 27. 1件のデータの全体フロー

SensorData 1件について、システム全体では以下のように流れる。

```text id="o2z4c8"
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
Message Queue
 ↓
Communication Process
 ↓
CloudflareClient
 ↓
JSON生成
 ↓
HTTPS POST
 ↓
Cloudflare Worker
 ↓
D1
```

---

## 28. 正常系の全体フロー

正常時は以下の流れとなる。

```text id="y3k0e1"
Sensor Process
      ↓
DHT11読み取り成功
      ↓
SensorData生成
      ↓
Queue送信成功
      ↓
Communication Process
      ↓
SensorData受信
      ↓
HTTPS POST
      ↓
HTTP 2xx
      ↓
成功
      ↓
次のデータ
```

---

## 29. センサエラー時の全体フロー

```text id="g4g2g3"
Sensor Process
      ↓
DHT11読み取り
      ↓
失敗
      ↓
ログ出力
      ↓
5秒待機
      ↓
再度読み取り
```

この場合、Communication Processへデータは送信されない。

---

## 30. 通信エラー時の全体フロー

```text id="n1g3kw"
SensorData
      ↓
Queue
      ↓
Communication Process
      ↓
HTTPS POST
      ↓
通信エラー
      ↓
1秒待機
      ↓
再送
      ↓
成功
```

再送時も同じSensorDataを使用する。

---

## 31. HTTP 4xx時の全体フロー

```text id="5e9m5n"
HTTPS POST
      ↓
HTTP 4xx
      ↓
エラー記録
      ↓
リトライしない
      ↓
次のSensorData
```

---

## 32. HTTP 5xx時の全体フロー

```text id="b2d1z6"
HTTPS POST
      ↓
HTTP 5xx
      ↓
リトライ
      ↓
1秒
      ↓
再送
```

最大4回まで送信する。

---

## 33. プロセス終了

プロセスを終了する場合は、使用しているリソースを解放してから終了する。

### Sensor Process

```text id="7d9l0f"
終了要求
   ↓
センサ終了処理
   ↓
Queueクローズ
   ↓
終了
```

### Communication Process

```text id="z7w1j0"
終了要求
   ↓
CloudflareClient終了処理
   ↓
Queueクローズ
   ↓
終了
```

---

## 34. systemdによる再起動

プロセスが異常終了した場合は、systemdによる再起動を行う。

対象サービス：

```text id="1qf4ly"
sensor-process.service
communication-process.service
```

基本設定：

```text id="f5m4qk"
Restart=on-failure
RestartSec=5s
```

これにより、プロセスが異常終了した場合は一定時間後に再起動する。

---

## 35. systemdとプログラム内リトライの違い

プログラム内のリトライとsystemdの再起動は役割が異なる。

### プログラム内リトライ

一時的な通信エラーなどに対して使用する。

```text id="a2z2t5"
通信失敗
 ↓
1秒
 ↓
再送
```

### systemd

プロセス自体が異常終了した場合に使用する。

```text id="8z3w8h"
プロセス異常終了
 ↓
systemd
 ↓
5秒
 ↓
プロセス再起動
```

---

## 36. プロセスフロー全体

本システムの基本的な動作をまとめると以下となる。

```text id="2o9v3q"
                 ┌─────────────────┐
                 │  Sensor Process │
                 └────────┬────────┘
                          │
                       DHT11
                          │
                          ▼
                    SensorData生成
                          │
                          ▼
                   Message Queue
                          │
                          ▼
              ┌──────────────────────┐
              │ Communication Process│
              └──────────┬───────────┘
                         │
                  CloudflareClient
                         │
                         ▼
                    HTTPS POST
                         │
                         ▼
                Cloudflare Worker
                         │
                         ▼
                    Cloudflare D1
```

---

## 37. 設計上のポイント

### 37.1 2プロセスを独立させる

センサ処理と通信処理を分離する。

### 37.2 SensorDataを一貫して引き渡す

Sensor Processで生成したSensorDataを、Queue、Communication Process、CloudflareClientへ引き渡す。

### 37.3 エラー処理を分散させすぎない

センサエラーはSensor Process、通信エラーとリトライはCommunication Processで扱う。

### 37.4 プロセス異常はsystemdで復旧する

アプリケーション内のリトライと、プロセス再起動を役割分担する。

---

## 38. 上位設計との対応

本書は以下の設計を具体化する。

* `03_software_architecture.md`

  * プロセス構成
  * データフロー

* `04_data_ipc_communication.md`

  * SensorData
  * Message Queue
  * HTTPS

* `05_operation_recovery.md`

  * エラー処理
  * リトライ
  * systemd再起動

* `06_system_management_security.md`

  * systemd
  * 設定ファイル

---

## 39. 次の設計書との関係

最後の`08_10_source_mapping.md`では、ここまで定義したプログラム設計と、実際に作成するソースファイルを対応付ける。

```text id="q6x2pb"
プログラム設計
      ↓
08_10_source_mapping.md
      ↓
.h / .cpp / main.cpp
      ↓
実装
```

---

## 40. まとめ

本システムは、Sensor ProcessとCommunication ProcessをMessage Queueで接続し、SensorDataをCloudflare Workerへ送信する。

全体の基本フローは以下である。

```text id="8qg9e4"
DHT11
 ↓
Sensor Process
 ↓
SensorData
 ↓
Message Queue
 ↓
Communication Process
 ↓
CloudflareClient
 ↓
HTTPS
 ↓
Cloudflare Worker
 ↓
D1
```

正常時はこの流れを繰り返す。

一時的なエラーが発生した場合は、定義された範囲でリトライする。

プロセス自体が異常終了した場合は、systemdによる再起動で復旧する。
