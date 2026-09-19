## 7. システムアーキテクチャ

### 7.1 アーキテクチャ概要

本システムは、Raspberry Pi 4BをEdge Deviceとして配置し、DHT11から取得した温湿度データをCloudflare Workerへ送信する構成とする。

Cloudflare Workerは受信したセンサーデータをCloudflare D1へ保存する。

BrowserはCloudflare Workerを介して保存されたセンサーデータを参照する。

Raspberry Pi 4B上では、Edge Applicationがセンサーデータの取得、データ処理、クラウドへの送信などを担当する。

Edge Application内部のProcess、Thread、Queue、IPCなどの具体的な構成は、9章および11章で定義する。

### 7.2 システム構成要素

本システムは、以下の主要構成要素からなる。

| ID | 構成要素 | 主な責務 |
|---|---|---|
| SYS-001 | DHT11 | 温度・湿度データを提供する |
| SYS-002 | Raspberry Pi 4B | Edge Deviceとしてセンサーデータを取得・処理・送信する |
| SYS-003 | Edge Application | Raspberry Pi上でセンサーデータ処理および通信を行う |
| SYS-004 | Wi-Fi / Internet | Edge DeviceとCloudflare間の通信経路を提供する |
| SYS-005 | Cloudflare Worker | センサーデータの受信およびD1とのデータ連携を行う |
| SYS-006 | Cloudflare D1 | センサーデータを保存する |
| SYS-007 | Browser | センサーデータを参照・表示する |

### 7.3 論理アーキテクチャ

```mermaid
flowchart LR
    Sensor["DHT11<br/>温湿度センサー"]

    subgraph EdgeDevice["Edge Device<br/>Raspberry Pi 4B"]
        EdgeApp["Edge Application"]
    end

    Network["Wi-Fi / Internet"]

    subgraph Cloud["Cloudflare"]
        Worker["Cloudflare Worker"]
        D1["Cloudflare D1"]
    end

    Browser["Browser"]

    Sensor -->|温度・湿度データ| EdgeApp
    EdgeApp -->|センサーデータ| Network
    Network --> Worker
    Worker -->|保存| D1

    Browser -->|参照要求| Worker
    Worker -->|データ| Browser
````



### 7.4 Edge Applicationの責務

Raspberry Pi 4B上のEdge Applicationは、以下の責務を持つ。

* DHT11から温度・湿度データを取得する。
* 取得したセンサーデータを処理する。
* センサーデータをCloudflare Workerへ送信する。
* 通信結果を処理する。
* 必要に応じてセンサーデータを保持する。
* 異常発生時に適切な処理を行う。
* 起動および終了時に必要な処理を行う。
* 動作状況および異常状況をログへ記録する。

具体的な責務分割および実行単位は、8章および9章で定義する。

### 7.5 Cloudflare Workerの責務

Cloudflare Workerは、以下の責務を持つ。

* Raspberry Piからセンサーデータを受信する。
* 受信したセンサーデータをCloudflare D1へ保存する。
* Browserからのセンサーデータ参照要求を受け付ける。
* Cloudflare D1からセンサーデータを取得する。
* Browserへセンサーデータを返却する。

Cloudflare Worker内部の詳細実装は、本システムの設計対象外とする。

ただし、Edge ApplicationおよびBrowserとのインターフェースについては、本システムの設計対象とする。

### 7.6 Cloudflare D1の責務

Cloudflare D1は、Cloudflare Workerから受信したセンサーデータを保存する。

保存するデータ項目およびデータ構造は10章で定義する。

### 7.7 Browserの責務

Browserは、Cloudflare Workerを介してセンサーデータを参照する。

主な表示対象は以下とする。

* 温度の現在値
* 湿度の現在値
* センサーデータの履歴
* センサーデータの時系列グラフ

Browser内部の詳細な画面構成および実装方式は、本設計では詳細化しない。

### 7.8 アーキテクチャ上の方針

Raspberry Pi上のEdge ApplicationとCloudflare側の処理を分離し、それぞれの責務を明確にする。

Edge Applicationは、センサーおよびネットワークとの境界を担当する。

Cloudflare Workerは、Edge ApplicationとD1およびBrowserとの間のデータ連携を担当する。

Cloudflare D1はデータ保存を担当する。

Browserはセンサーデータの参照および表示を担当する。

Edge Application内部のProcess、Thread、Queue、IPC、状態管理、Retry、Backoffなどの詳細な構造は、9章～16章で定義する。
