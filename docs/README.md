# Raspberry Pi Edge IoT System Design

本ディレクトリは、Raspberry Pi 4Bを中心としたEdge IoTシステムの設計書である。

## 構成

- Raspberry Pi 4B
- Raspberry Pi OS
- DHT11
- Wi-Fi / Internet
- C++ Edge Application
- Cloudflare Worker
- Cloudflare D1
- Browser

## 設計書の構成

| ファイル | 内容 |
|---|---|
| 01_requirements.md | 1～6章 要求・システム定義 |
| 02_system_architecture.md | 7章 システムアーキテクチャ |
| 03_software_architecture.md | 8～9章 ソフトウェア・Process / Thread設計 |
| 04_data_ipc_communication.md | 10～13章 データ・IPC・周期・通信設計 |
| 05_operation_recovery.md | 14～18章 状態・異常・復旧・ログ・Lifecycle設計 |
| 06_system_management_security.md | 19～20章 systemd・セキュリティ設計 |
| 07_verification_review.md | 21～25章 テスト・設計判断・設計決定事項・将来拡張・最終レビュー |

## 設計完了

25章の設計レビュー完了をもって設計を確定し、その後の実装は本設計を基準とする。

設計変更が必要になった場合は、影響確認 → 設計判断 → 設計書更新 → 実装の順序で扱う。
