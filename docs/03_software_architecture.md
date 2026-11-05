## 8. ソフトウェアアーキテクチャ

### 8.1 概要

Raspberry Pi 4B上で動作するEdge Applicationは、センサーデータの取得、データ処理、通信、異常処理などの責務を分離した構造とする。

各機能の責務を明確にすることで、機能追加や変更の影響範囲を限定し、保守性を確保する。

具体的なProcessおよびThreadへの割り当ては9章で定義する。

### 8.2 ソフトウェア構成

Edge Applicationは、以下の論理的な機能要素から構成する。

| ID | 機能要素 | 主な責務 |
|---|---|---|
| SW-001 | Application Control | アプリケーション全体の初期化、起動、終了を管理する |
| SW-002 | Sensor | DHT11から温度・湿度データを取得する |
| SW-003 | Data Processing | 取得したセンサーデータを処理する |
| SW-004 | Data Management | センサーデータの一時保持および機能間の受け渡しを管理する |
| SW-005 | Communication | Cloudflare Workerとの通信を行う |
| SW-006 | Error Handling | センサー、通信、内部処理などの異常を処理する |
| SW-007 | Logging | 動作状況および異常状況を記録する |

### 8.3 ソフトウェア構成図

```mermaid
flowchart TB
    App["Application Control"]

    Sensor["Sensor<br/>DHT11"]
    Process["Data Processing"]
    Data["Data Management"]
    Comm["Communication"]
    Error["Error Handling"]
    Log["Logging"]

    App --> Sensor
    Sensor --> Process
    Process --> Data
    Data --> Comm

    App --> Error
    Sensor --> Error
    Process --> Error
    Comm --> Error

    App --> Log
    Sensor --> Log
    Process --> Log
    Comm --> Log
    Error --> Log

    Comm -->|HTTP| Worker["Cloudflare Worker"]
```

## 9. Process / Thread設計

### 9.1 Process / Thread構成の基本方針

Raspberry Pi 4B上のEdge Applicationは、センサー取得と通信処理をProcess単位で分離する。

Process構成は以下の方針とする。

- Sensor Process：DHT11からのセンサーデータ取得を担当する
- Communication Process：IPC Message QueueからSensorDataを取得し、Cloudflare Workerへ送信する
- Process間のデータ受け渡し：IPC Message Queueを使用する
- 各Processは独立して動作し、Communication Processの通信遅延や異常終了がSensor Processの動作に直接影響しない構成とする。
ただし、IPC Message Queueの容量超過など、Process間通信に起因する影響については別途定義する。
- Processの起動・停止・異常終了時の管理はsystemdと連携する

Threadは必要最小限とし、本システムでは各Processの主処理をMain Threadで実行する構成を基本とする。将来、処理の並列化が必要になった場合はThreadを追加できる構造とする。

### 9.2 Process構成

Edge Applicationは、以下の2 Processで構成する。

| Process | 主な責務 |
|---|---|
| Sensor Process | DHT11から温度・湿度を周期取得し、SensorDataを生成してIPC Message Queueへ送信する |
| Communication Process | IPC Message QueueからSensorDataを取得し、Cloudflare WorkerへHTTPS POSTで送信する |

```mermaid
flowchart LR
    Systemd["systemd"]

    subgraph Edge["Raspberry Pi 4B / Edge Application"]
        SP["Sensor Process
Main Thread"]
        Q["IPC Message Queue"]
        CP["Communication Process
Main Thread"]
    end

    DHT["DHT11"]
    Net["Wi-Fi / Internet"]
    Worker["Cloudflare Worker"]

    DHT --> SP
    SP -->|SensorData| Q
    Q --> CP
    CP -->|HTTPS POST| Net
    Net --> Worker
    Systemd -.->|起動・停止・再起動| SP
    Systemd -.->|起動・停止・再起動| CP
```

### 9.3 Sensor Process

Sensor Processは、DHT11から温度・湿度を取得する。

主な処理は以下とする。

1. 初期化
2. IPC Message Queue作成
3. DHT11取得処理
4. SensorData生成
5. IPC Message Queueへの送信
6. 異常処理
7. 周期待機
8. Shutdown処理

DHT11からのデータ取得に失敗した場合は、SensorDataを生成せず、異常処理を行った後、次の取得周期へ進む。
IPC Message Queueの作成に失敗した場合は、エラー内容をログに記録し、Sensor Processを異常終了させる。
Queue作成の再試行はSensor Process内部では行わず、再起動はsystemdに委ねる。
IPC Message Queueへの送信に失敗した場合、Queue Fullであれば対象のSensorDataを破棄し、次の取得周期へ進む。
Queue Full以外の送信異常が発生した場合は、異常をログに記録し、次の取得周期へ進む。
センサーデータ取得周期は5秒とする。Linux環境であるため、周期にはスケジューリング遅延が発生し得るものとする。


### 9.4 Communication Process

Communication Processは、IPC Message QueueからSensorDataを取得し、Cloudflare Workerへ送信する。

主な処理は以下とする。

1. 初期化
2. IPC Message Queue接続
3. Queue不存在時の接続再試行
4. IPC Message QueueからSensorData取得
5. HTTP POST送信
6. HTTP Response判定
7. Retry / Backoff
8. 送信結果記録
9. Shutdown処理

IPC Message QueueはSensor Processが作成する。
Communication Processは既存のIPC Message Queueに接続する。

Queueが存在しない場合は、一定時間待機した後、接続を再試行する。
Queueへの接続に成功するまで、この処理を繰り返す。
Queueへの接続に成功した後、SensorDataの受信処理を開始する。

IPC Message QueueにSensorDataが存在しない場合は、次のSensorDataが到着するまで受信処理を待機する（Blocking方式）。
SensorDataを取得した場合は、Cloudflare Workerへの送信処理を行う。

送信処理完了後、次の送信処理を開始するまで1秒間待機する。
Queueに複数のSensorDataが蓄積されている場合も、同じ送信間隔で順次処理する。

Queueへの接続時にQueue不存在以外の異常が発生した場合は、異常として扱い、ログに記録する。

Retryを実施しても送信に失敗した場合は、対象のSensorDataを破棄し、送信結果を記録する。
通信処理はSensor Processと独立して実行する。

### 9.5 Process間の責務分担

| 項目 | Sensor Process | Communication Process |
|---|---|---|
| センサー取得 | ○ | - |
| SensorData生成 | ○ | - |
| Queue送信 | ○ | - |
| Queue受信 | - | ○ |
| HTTP通信 | - | ○ |
| Retry / Backoff | - | ○ |
| Cloudflare連携 | - | ○ |
| センサー異常処理 | ○ | - |
| 通信異常処理 | - | ○ |
| Queue送信異常処理| ○ | - |
| Queue受信異常処理| - | ○ |

### 9.6 ProcessとThreadの関係

各ProcessはMain Threadを基本実行単位とする。

```text
Sensor Process
└── Main Thread

Communication Process
└── Main Thread
```

本システムでは、センサー取得と通信処理をProcess単位で分離することを優先し、不要なThread分割は行わない。

### 9.7 Process障害の分離

Sensor ProcessとCommunication Processを分離することで、一方のProcessに異常が発生した場合でも、もう一方のProcessを独立して再起動できる構成とする。

ただし、Sensor Processが停止している場合は新規SensorDataが生成されず、Communication Processが停止している場合はQueueへデータが蓄積する。
Sensor ProcessおよびCommunication Processの稼働状態はsystemdで確認する。
また、Queueへの送信失敗などの異常はログに記録する。

Queueの容量を超過した場合、Sensor Processからの新規データ送信は失敗する。

### 9.8 Process管理

Processの起動、停止、異常終了時の再起動はsystemdで管理する。

systemdの詳細設定は19章で定義する。

### 9.9 Process / Thread設計の決定事項

本章のProcess / Thread構成は以下で確定する。

- Sensor ProcessとCommunication Processを分離する
- Process間通信にはIPC Message Queueを使用する
- 各ProcessはMain Threadを基本とする
- Sensor Processは5秒周期でSensorDataを生成する
- Communication ProcessはQueueからデータを取得して送信する
- Retry / BackoffはCommunication Processで実行する
- Process管理はsystemdで行う
- Communication Processの通信遅延や異常終了は、Sensor Processの動作に直接影響しない
- Queue容量超過時は、Sensor Processからの新規データ送信が失敗する
- Retry終了後、送信に失敗したSensorDataは破棄する
- IPC Message Queueの作成はSensor Processが担当する
- Communication Processは既存のIPC Message Queueに接続する
- Communication ProcessはQueue不存在時に接続を再試行する
