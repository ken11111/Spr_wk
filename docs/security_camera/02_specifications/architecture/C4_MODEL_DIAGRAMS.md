# セキュリティカメラシステム C4モデル図

## 概要

C4モデルによるセキュリティカメラシステムのアーキテクチャ図です。4つのレベル（Context、Container、Component、Code）でシステム全体を階層的に表現します。

## Level 1: System Context Diagram

```plantuml
@startuml c4_context
!include https://raw.githubusercontent.com/plantuml-stdlib/C4-PlantUML/master/C4_Context.puml

!define DEVICONS https://raw.githubusercontent.com/tupadr3/plantuml-icon-font-sprites/master/devicons
!include DEVICONS/react.puml

LAYOUT_WITH_LEGEND()

title System Context Diagram - Security Camera System

Person(user, "監視オペレータ", "セキュリティカメラシステムの監視・操作を行うユーザー")
Person(admin, "システム管理者", "システムの設定・保守を行う管理者")

System(security_camera, "Spresense Security Camera System", "Phase 11 Enhanced Control\nリアルタイム映像監視システム\n適応制御・予防的回復機能")

System_Ext(network_infrastructure, "Network Infrastructure", "WiFi/TCP通信基盤")
System_Ext(storage_system, "Recording Storage", "映像データ保存システム")
System_Ext(monitoring_dashboard, "Monitoring Dashboard", "Web-based管理ダッシュボード")

Rel(user, security_camera, "映像監視・制御操作", "USB/WiFi")
Rel(admin, security_camera, "システム設定・診断", "USB/WiFi")
Rel(security_camera, network_infrastructure, "映像ストリーミング・メトリクス送信", "TCP/WiFi")
Rel(security_camera, storage_system, "録画データ保存", "TCP/WiFi")
Rel(user, monitoring_dashboard, "リアルタイム監視", "HTTPS")
Rel(admin, monitoring_dashboard, "システム管理", "HTTPS")
Rel(monitoring_dashboard, security_camera, "メトリクス取得・制御指令", "TCP/WiFi")

@enduml
```

## Level 2: Container Diagram

```plantuml
@startuml c4_container
!include https://raw.githubusercontent.com/plantuml-stdlib/C4-PlantUML/master/C4_Container.puml

LAYOUT_WITH_LEGEND()

title Container Diagram - Security Camera System Architecture

Person(user, "監視オペレータ", "セキュリティカメラシステムの監視・操作")

System_Boundary(spresense_system, "Spresense Edge Device") {
    Container(camera_app, "Security Camera Application", "C/NuttX", "メインアプリケーション\nPhase 11制御エンジン")
    Container(camera_hw, "Camera Hardware Interface", "V4L2/ISX012", "ハードウェアカメラ制御\nJPEG/H.264エンコーダ")
    Container(wifi_stack, "WiFi Communication Stack", "GS2200M/TCP", "ネットワーク通信\nプロトコルハンドラ")
    Container(control_engine, "Adaptive Control Engine", "Phase 11", "多変数適応制御\nフレーム統計・予測")
}

System_Boundary(pc_system, "PC Host System") {
    Container(stream_receiver, "Stream Receiver", "C++/Qt", "映像ストリーム受信\nデコード・表示")
    Container(metrics_analyzer, "Metrics Analyzer", "C++/Python", "性能解析・可視化\n制御理論ダッシュボード")
    Container(recording_engine, "Recording Engine", "C++/FFmpeg", "映像録画・保存\nファイル管理")
    Container(web_dashboard, "Web Dashboard", "React/Node.js", "Webベース管理UI\nリアルタイム監視")
}

System_Boundary(external_systems, "External Systems") {
    ContainerDb(file_storage, "File Storage", "HDD/SSD", "録画ファイル保存")
    Container(network_infra, "Network Infrastructure", "WiFi Router", "ネットワーク基盤")
}

' User interactions
Rel(user, web_dashboard, "監視・操作", "HTTPS")
Rel(user, stream_receiver, "リアルタイム映像", "Direct USB/TCP")

' Spresense internal communication
Rel(camera_app, camera_hw, "カメラ制御・フレーム取得", "V4L2 API")
Rel(camera_app, wifi_stack, "データ送信・プロトコル処理", "TCP Socket")
Rel(camera_app, control_engine, "制御指令・フィードバック", "Function Call")
Rel(control_engine, camera_hw, "適応制御・FPS調整", "Runtime Control")

' Spresense to PC communication
Rel(wifi_stack, stream_receiver, "映像ストリーム", "MJPEG/TCP")
Rel(wifi_stack, metrics_analyzer, "メトリクスデータ", "JSON/TCP")
Rel(wifi_stack, web_dashboard, "システム状態", "WebSocket/TCP")

' PC internal communication
Rel(stream_receiver, recording_engine, "録画制御", "IPC")
Rel(metrics_analyzer, web_dashboard, "解析結果・グラフ", "REST API")
Rel(recording_engine, file_storage, "ファイル保存", "File I/O")

' External dependencies
Rel_Neighbor(wifi_stack, network_infra, "WiFi通信", "802.11")
Rel(web_dashboard, network_infra, "Web配信", "HTTP/WebSocket")

@enduml
```

## Level 3: Component Diagram - Spresense Edge Device

```plantuml
@startuml c4_component_spresense
!include https://raw.githubusercontent.com/plantuml-stdlib/C4-PlantUML/master/C4_Component.puml

LAYOUT_WITH_LEGEND()

title Component Diagram - Spresense Security Camera Application

Container(pc_host, "PC Host System", "C++/Qt", "映像受信・制御・可視化")

Container_Boundary(camera_app, "Security Camera Application") {
    Component(main_controller, "Main Controller", "camera_app_main.c", "アプリケーションメイン\nスレッド管理・初期化")

    Component(camera_manager, "Camera Manager", "camera_manager.c", "ISX012カメラ制御\nV4L2インターフェース\nフレーム取得")

    Component(encoder_manager, "Encoder Manager", "encoder_manager.c", "ハードウェアエンコーダ制御\nJPEG/H.264エンコード\n品質制御")

    Component(frame_queue, "Frame Queue", "frame_queue.c", "フレームバッファ管理\nAction/Emptyキュー\n動的サイズ調整")

    Component(usb_transport, "USB Transport", "usb_transport.c", "CDC-ACM通信\nデータ送信・優先度制御")

    Component(protocol_handler, "Protocol Handler", "protocol_handler.c", "MJPEG プロトコル実装\nパケット化・振り分け")

    Component(fps_controller, "FPS Controller", "fps_controller.c", "Phase 10 PID制御\nKp=0.15, Ki=0.02\n100ms制御周期")

    Component(frame_statistics, "Frame Statistics", "frame_statistics.c", "フレーム統計解析\n複雑度計算・予測\n10フレーム窓解析")

    Component(enhanced_control, "Enhanced Control", "enhanced_control.h", "Phase 11多変数制御\n適応PID・重み付け統合")

    Component(perf_logger, "Performance Logger", "perf_logger.c", "性能メトリクス収集\nシステム統計・ログ出力")

    Component(camera_threads, "Camera Threads", "camera_threads.c", "スレッド管理\n優先度制御・同期")
}

Container_Boundary(wifi_system, "WiFi Communication") {
    Component(wifi_manager, "WiFi Manager", "wifi_manager.c", "GS2200M制御\nWiFi接続管理")
    Component(tcp_server, "TCP Server", "tcp_server.c", "TCPサーバー\n接続・データ送信")
}

' Main control flow
Rel(main_controller, camera_manager, "初期化・制御", "Function Call")
Rel(main_controller, camera_threads, "スレッド起動", "pthread_create")
Rel(camera_threads, fps_controller, "制御スレッド実行", "100ms周期")

' Camera pipeline
Rel(camera_manager, encoder_manager, "Rawフレーム", "V4L2 Buffer")
Rel(encoder_manager, frame_queue, "エンコード済みフレーム", "JPEG/H.264")
Rel(frame_queue, protocol_handler, "フレームバッファ", "Queue Pop")
Rel(protocol_handler, usb_transport, "MJPEGパケット", "Protocol Data")

' Control system
Rel(frame_queue, fps_controller, "キュー深度測定", "Real-time")
Rel(fps_controller, camera_manager, "FPS制御指令", "Runtime Parameter")
Rel(frame_statistics, enhanced_control, "統計データ", "Multi-variable Input")
Rel(enhanced_control, fps_controller, "適応制御指令", "Gain Scheduling")

' Performance monitoring
Rel(camera_manager, perf_logger, "カメラ統計", "Performance Data")
Rel(encoder_manager, perf_logger, "エンコード統計", "Performance Data")
Rel(frame_queue, perf_logger, "キュー統計", "Performance Data")
Rel(frame_queue, frame_statistics, "フレームサイズ・時間", "Statistics Input")

' WiFi communication (conditional compilation)
Rel(protocol_handler, wifi_manager, "WiFiデータ送信", "TCP Socket", $tags="wifi")
Rel(wifi_manager, tcp_server, "接続管理", "Socket Control", $tags="wifi")

' External communication
Rel(usb_transport, pc_host, "USB CDC通信", "12Mbps Stream")
Rel(tcp_server, pc_host, "WiFi TCP通信", "Variable Bitrate", $tags="wifi")

@enduml
```

## Level 4: Code Diagram - FPS Control System

```plantuml
@startuml c4_code_fps_control
!include https://raw.githubusercontent.com/plantuml-stdlib/C4-PlantUML/master/C4_Component.puml

LAYOUT_WITH_LEGEND()

title Code Diagram - FPS Control System Implementation

Container_Boundary(fps_control_system, "FPS Control System") {

    Component(fps_controller_h, "fps_controller.h", "Header", "制御パラメータ定義\n構造体・関数プロトタイプ")

    Component(fps_controller_init, "fps_controller_init()", "Function", "PID制御器初期化\nKp=0.15, Ki=0.02, Kd=0.0\n設定値=3.5 frames")

    Component(fps_controller_update, "fps_controller_update()", "Function", "PID制御更新\n100ms周期実行\n誤差計算・積分・出力")

    Component(fps_controller_reset, "fps_controller_reset()", "Function", "制御器リセット\n積分項クリア\n初期状態復帰")

    Component(fps_controller_get_output, "fps_controller_get_output()", "Function", "制御出力取得\nFPS値[5-30]\nクランプ処理")

    Component(fps_controller_struct, "fps_controller_t", "Struct", "制御器状態管理\nkp, ki, kd, setpoint\nintegral, last_error, output")

    Component(stability_monitor, "stability_monitor()", "Function", "安定性監視\n分散計算・閾値判定\n10サンプル窓")

    Component(enhanced_control_interface, "enhanced_control_update()", "Function", "Phase 11統合\n多変数入力処理\n適応ゲイン調整")
}

Container_Boundary(frame_statistics_system, "Frame Statistics System") {
    Component(frame_stats_init, "frame_statistics_init()", "Function", "統計システム初期化\n窓サイズ=10frames")

    Component(frame_stats_update, "frame_statistics_update()", "Function", "フレーム統計更新\nサイズ・時間記録\n複雑度計算")

    Component(complexity_analyzer, "calculate_complexity_index()", "Function", "複雑度指数計算\nsqrt(variance)/avg_size\n正規化[0.0-2.0]")

    Component(trend_predictor, "predict_frame_trend()", "Function", "トレンド予測\n5フレーム窓\n線形外挿")
}

Container_Boundary(camera_control_system, "Camera Control System") {
    Component(camera_set_fps, "camera_set_fps_runtime()", "Function", "カメラFPS設定\nV4L2 VIDIOC_S_PARM\nリアルタイム変更")

    Component(camera_get_frame, "camera_get_frame()", "Function", "フレーム取得\nV4L2 DQBUF\nタイムスタンプ記録")
}

' Control flow relationships
Rel(fps_controller_init, fps_controller_struct, "構造体初期化", "Memory Setup")
Rel(fps_controller_update, fps_controller_struct, "状態更新", "Read/Write")
Rel(fps_controller_get_output, fps_controller_struct, "出力読み取り", "Read Access")
Rel(fps_controller_update, stability_monitor, "安定性チェック", "Function Call")

' Enhanced control integration
Rel(enhanced_control_interface, fps_controller_update, "制御指令送信", "Parameter Override")
Rel(frame_stats_update, complexity_analyzer, "複雑度解析", "Data Processing")
Rel(trend_predictor, enhanced_control_interface, "予測データ", "Multi-variable Input")

' Frame statistics flow
Rel(camera_get_frame, frame_stats_update, "フレームデータ", "Size/Timestamp")
Rel(frame_stats_update, fps_controller_update, "統計フィードバック", "Queue Depth")

' Camera control output
Rel(fps_controller_get_output, camera_set_fps, "FPS制御値", "Control Signal")

' Header dependencies
Rel(fps_controller_init, fps_controller_h, "定義参照", "Include")
Rel(fps_controller_update, fps_controller_h, "定義参照", "Include")
Rel(enhanced_control_interface, fps_controller_h, "構造体アクセス", "Include")

@enduml
```

## Level 3: Component Diagram - PC Host System

```plantuml
@startuml c4_component_pc_host
!include https://raw.githubusercontent.com/plantuml-stdlib/C4-PlantUML/master/C4_Component.puml

LAYOUT_WITH_LEGEND()

title Component Diagram - PC Host System Architecture

Container(spresense_device, "Spresense Device", "C/NuttX", "セキュリティカメラエッジデバイス")

Container_Boundary(pc_host_system, "PC Host System") {

    ' Stream Reception Layer
    package "Stream Reception Layer" #lightblue {
        Component(net_img_receiver, "NetImgReceiver", "Python/Socket", "TCP映像ストリーム受信\n4カメラ同時対応\n300回リトライ機能")
        Component(usb_receiver, "USB Receiver", "Python/Serial", "USB CDC-ACM受信\n12Mbps対応\nバイナリプロトコル")
        Component(protocol_parser, "Protocol Parser", "Python", "MJPEGプロトコル解析\nSYNC_WORD: 0xCAFEBABE\nCRC16検証")
    }

    ' Image Processing Layer
    package "Image Processing Layer" #lightcyan {
        Component(img_scaler, "ImgScaler", "Python/PIL", "JPEG→Bitmap変換\n高品質補間スケーリング\n解像度適応")
        Component(frame_buffer, "Frame Buffer", "Python/Threading", "フレームバッファ管理\nマルチスレッド安全\nメモリプール")
        Component(jpeg_decoder, "JPEG Decoder", "Python/PIL", "JPEGデコード\nエラーハンドリング\n品質検証")
    }

    ' Display & UI Layer
    package "Display & UI Layer" #lightyellow {
        Component(multi_cam_frame, "MultiCamFrame", "Python/wxPython", "4カメラレイアウト表示\n2x2グリッド自動調整\nESCキー終了制御")
        Component(webcam_panel, "WebCamPanel", "Python/wxPython", "個別カメラ表示パネル\nイベント駆動更新\nミューテックス保護")
        Component(camera_server_wrapper, "CameraServerWrapper", "Python", "カメラビットマップ管理\nサーバー接続ラッパー")
        Component(control_panel, "Control Panel", "HTML/JavaScript", "リアルタイム制御UI\n手動パラメータ調整")
    }

    ' Analysis & Monitoring Layer
    package "Analysis & Monitoring Layer" #lightgreen {
        Component(metrics_analyzer, "Metrics Analyzer", "Python/Pandas", "CSVメトリクス解析\nFPS安定性解析\n統計的性能評価")
        Component(time_series_stats, "Time Series Stats", "Python/NumPy", "時系列統計計算\n変動係数(CV)計算\nトレンド分析")
        Component(control_analysis, "Control Analysis", "Python/SciPy", "制御系設計パラメータ\nPID調整計算\n安定性解析")
        Component(perf_logger, "Performance Logger", "Python/CSV", "性能データログ出力\nリアルタイム統計\nアラート生成")
    }

    ' Dashboard & Visualization Layer
    package "Dashboard & Visualization Layer" #lightpink {
        Component(interactive_dashboard, "Interactive Dashboard", "Python/Plotly", "HTML対話式ダッシュボード\nリアルタイム可視化\nPlotlyチャート")
        Component(control_dashboard, "Control Dashboard", "HTML/CSS/JS", "制御系パフォーマンス表示\nBode線図・Nyquist線図\nステップ応答表示")
        Component(metrics_dashboard, "Metrics Dashboard", "Python/Matplotlib", "統計グラフ生成\nトレンドチャート\n性能レポート")
    }

    ' Data Management Layer
    package "Data Management Layer" #lavender {
        Component(csv_data_manager, "CSV Data Manager", "Python/OS", "メトリクスファイル管理\nディレクトリスキャン\nデータバックアップ")
        Component(config_manager, "Config Manager", "Python/JSON", "設定ファイル管理\nパラメータ保存\nデフォルト値管理")
        Component(session_manager, "Session Manager", "Python", "セッション状態管理\n接続維持\nタイムアウト処理")
    }
}

' External Systems
ContainerDb(file_storage, "File Storage", "HDD/SSD", "録画データ・メトリクス保存")
System_Ext(web_browser, "Web Browser", "Chrome/Firefox等\nダッシュボードアクセス")

' Data Flow - Reception
Rel(spresense_device, net_img_receiver, "TCP映像ストリーム", "Port 10080")
Rel(spresense_device, usb_receiver, "USB CDC-ACM", "/dev/ttyACM0")
Rel(net_img_receiver, protocol_parser, "生データ", "Socket Buffer")
Rel(usb_receiver, protocol_parser, "USBデータ", "Serial Buffer")

' Data Flow - Processing
Rel(protocol_parser, jpeg_decoder, "JPEGフレーム", "Binary Data")
Rel(jpeg_decoder, img_scaler, "Bitmapデータ", "PIL Image")
Rel(img_scaler, frame_buffer, "スケール済み画像", "Processed Frame")

' Data Flow - Display
Rel(frame_buffer, webcam_panel, "表示フレーム", "wxPython Event")
Rel(webcam_panel, multi_cam_frame, "パネル更新", "GUI Update")
Rel(camera_server_wrapper, webcam_panel, "ビットマップ", "Image Data")

' Data Flow - Analysis
Rel(spresense_device, metrics_analyzer, "メトリクスデータ", "CSV/JSON")
Rel(metrics_analyzer, time_series_stats, "生データ", "DataFrame")
Rel(time_series_stats, control_analysis, "統計結果", "Statistical Data")
Rel(control_analysis, perf_logger, "解析結果", "Analysis Results")

' Data Flow - Visualization
Rel(time_series_stats, interactive_dashboard, "統計データ", "JSON")
Rel(control_analysis, control_dashboard, "制御解析", "Plot Data")
Rel(perf_logger, metrics_dashboard, "ログデータ", "CSV")

' Data Flow - Storage & Config
Rel(csv_data_manager, file_storage, "ファイル操作", "File I/O")
Rel(config_manager, file_storage, "設定保存", "JSON Files")
Rel(perf_logger, file_storage, "ログ出力", "CSV/Log Files")

' External Access
Rel(web_browser, interactive_dashboard, "ダッシュボードアクセス", "HTTP")
Rel(web_browser, control_dashboard, "制御画面アクセス", "HTTP")

' Internal Communication
Rel(session_manager, net_img_receiver, "接続管理", "Session Control")
Rel(config_manager, control_panel, "設定読み込み", "Config Load")

@enduml
```

## Level 4: Code Diagram - Protocol Handler System

```plantuml
@startuml c4_code_protocol_handler
!include https://raw.githubusercontent.com/plantuml-stdlib/C4-PlantUML/master/C4_Component.puml

LAYOUT_WITH_LEGEND()

title Code Diagram - Protocol Handler System Implementation

Container_Boundary(protocol_handler_system, "Protocol Handler System") {

    ' Core Protocol Structure Definitions
    Component(protocol_header_struct, "packet_header_t", "Struct", "プロトコルヘッダー構造体\nmagic: 0x5350 ('SP')\nversion: 0x01\ntype, sequence, timestamp\npayload_size, checksum")

    Component(handshake_struct, "handshake_payload_t", "Struct", "ハンドシェイク情報\nvideo_width, video_height\nfps, codec, bitrate")

    Component(mjpeg_packet_struct, "mjpeg_packet_t", "Struct", "MJPEGパケット構造\nsync_word: 0xCAFEBABE\nsequence, size\njpeg_data, crc16")

    ' Protocol Processing Functions
    Component(protocol_init, "protocol_handler_init()", "Function", "プロトコル初期化\nバッファプール確保\nシーケンス番号初期化")

    Component(protocol_pack_nal, "protocol_pack_nal_unit()", "Function", "NALユニット→パケット変換\nH.264 NAL encapsulation\nタイムスタンプ付与")

    Component(protocol_create_handshake, "protocol_create_handshake()", "Function", "ハンドシェイクパケット生成\nカメラ設定情報送信")

    Component(protocol_crc16, "protocol_crc16()", "Function", "CRC16チェックサム計算\nCCITTアルゴリズム\nパケット整合性検証")

    Component(mjpeg_pack_frame, "mjpeg_pack_frame()", "Function", "JPEG→MJPEGパケット化\nSYNC_WORD付与\nシーケンス管理")

    Component(mjpeg_validate_header, "mjpeg_validate_header()", "Function", "MJPEGヘッダー検証\nマジックワード確認\nサイズ制限チェック")

    ' Packet Type Management
    Component(packet_type_enum, "packet_type_t", "Enum", "パケットタイプ定義\nHANDSHAKE, SPS, PPS\nIDR, SLICE, HEARTBEAT\nERROR")

    Component(codec_type_enum, "codec_type_t", "Enum", "コーデックタイプ\nH264 = 0x01\nJPEG = 0x02")

    ' Buffer Management
    Component(packet_buffer_pool, "packet_buffer_pool_t", "Struct", "パケットバッファプール\n4096バイト × 4バッファ\nスレッド安全管理")

    Component(buffer_alloc, "protocol_alloc_buffer()", "Function", "バッファ確保\nプールからの取得\nメモリ不足ハンドリング")

    Component(buffer_free, "protocol_free_buffer()", "Function", "バッファ解放\nプール返却\nメモリリーク防止")

    ' USB Transport Interface
    Component(usb_send_packet, "usb_transport_send_packet()", "Function", "USBパケット送信\nCDC-ACM インターフェース\nタイムアウト制御")

    Component(usb_connection_check, "usb_transport_is_connected()", "Function", "USB接続状態確認\nホスト接続検出")
}

Container_Boundary(camera_encoder_system, "Camera & Encoder Interface") {
    Component(camera_get_frame, "camera_get_frame()", "Function", "カメラフレーム取得\nV4L2 DQBUF\nタイムスタンプ記録")

    Component(encoder_get_nal, "encoder_get_nal_unit()", "Function", "H.264 NALユニット取得\nSPS/PPS/IDR/Slice\nエンコード済みデータ")

    Component(jpeg_get_data, "camera_get_jpeg_data()", "Function", "JPEGデータ取得\nハードウェアエンコーダ\n可変サイズフレーム")
}

Container_Boundary(network_transport_system, "Network Transport System") {
    Component(tcp_server_init, "tcp_server_init()", "Function", "TCPサーバー初期化\nポート10080 bind\nクライアント待機")

    Component(tcp_send_data, "tcp_send_data()", "Function", "TCP データ送信\nソケット書き込み\nエラーハンドリング")

    Component(wifi_manager_connect, "wifi_manager_connect()", "Function", "WiFi接続管理\nGS2200M制御\n接続状態監視")
}

' Structure Dependencies
Rel(protocol_init, protocol_header_struct, "構造体初期化", "Memory Setup")
Rel(protocol_create_handshake, handshake_struct, "ハンドシェイク構造体", "Data Fill")
Rel(mjpeg_pack_frame, mjpeg_packet_struct, "MJPEGパケット構造", "Packet Creation")

' Function Dependencies - Protocol Processing
Rel(protocol_pack_nal, protocol_header_struct, "ヘッダー設定", "Header Fill")
Rel(protocol_pack_nal, protocol_crc16, "チェックサム計算", "Function Call")
Rel(mjpeg_pack_frame, mjpeg_validate_header, "ヘッダー検証", "Validation")
Rel(mjpeg_validate_header, protocol_crc16, "CRC確認", "Checksum Verify")

' Buffer Management Flow
Rel(protocol_init, packet_buffer_pool, "バッファプール初期化", "Pool Setup")
Rel(buffer_alloc, packet_buffer_pool, "バッファ取得", "Pool Access")
Rel(buffer_free, packet_buffer_pool, "バッファ返却", "Pool Return")
Rel(protocol_pack_nal, buffer_alloc, "バッファ確保", "Memory Request")

' Data Input Flow
Rel(camera_get_frame, jpeg_get_data, "フレーム取得", "V4L2 Data")
Rel(encoder_get_nal, protocol_pack_nal, "NALユニット", "H.264 Data")
Rel(jpeg_get_data, mjpeg_pack_frame, "JPEGデータ", "Raw JPEG")

' Transport Output Flow
Rel(protocol_pack_nal, usb_send_packet, "プロトコルパケット", "Packet Data")
Rel(mjpeg_pack_frame, tcp_send_data, "MJPEGストリーム", "Stream Data")
Rel(usb_send_packet, usb_connection_check, "接続確認", "Status Check")

' Network Interface
Rel(tcp_send_data, tcp_server_init, "サーバー経由送信", "Socket Send")
Rel(tcp_server_init, wifi_manager_connect, "WiFi接続", "Network Setup")

' Type System
Rel(protocol_pack_nal, packet_type_enum, "パケットタイプ設定", "Type Assignment")
Rel(protocol_create_handshake, codec_type_enum, "コーデック指定", "Codec Type")

@enduml
```

## Level 4: Code Diagram - Frame Queue Management System

```plantuml
@startuml c4_code_frame_queue_system
!include https://raw.githubusercontent.com/plantuml-stdlib/C4-PlantUML/master/C4_Component.puml

LAYOUT_WITH_LEGEND()

title Code Diagram - Frame Queue Management System Implementation

Container_Boundary(frame_queue_system, "Frame Queue Management System") {

    ' Core Data Structures
    Component(v_buffer_struct, "v_buffer", "Struct", "フレームバッファ構造体\nstart: 32byte対齊ポインタ\nlength: バッファサイズ\njpg_len: JPEGデータサイズ\nid: バッファID\nnext: リンクリストポインタ")

    Component(v4l2_buffer_struct, "v4l2_buffer", "Struct", "V4L2バッファ構造体\nindex: バッファインデックス\ntype: バッファタイプ\nbytesused: 使用バイト数\ntimestamp: タイムスタンプ")

    Component(queue_state_struct, "queue_state_t", "Struct", "キュー状態管理\nempty_queue_head\naction_queue_head\nqueue_mutex\nqueue_condition")

    ' Buffer Management Functions
    Component(prepare_camera_buf, "multiwebcam_prepare_camera_buf()", "Function", "カメラバッファ準備\nV4L2バッファ確保\n32バイト境界アライン\nmmapメモリマッピング")

    Component(get_picture_buf, "multiwebcam_get_picture_buf()", "Function", "フレームバッファ取得\nV4L2 DQBUF実行\nタイムスタンプ取得\nJPEGサイズ記録")

    Component(set_picture_buf, "multiwebcam_set_picture_buf()", "Function", "フレームバッファ返却\nV4L2 QBUF実行\nバッファ再利用\nキューイング")

    ' Queue Operations
    Component(pull_empty, "multiwebcam_pull_empty()", "Function", "空きバッファ取得\nEmptyキューからPop\nミューテックス保護\n待機処理")

    Component(push_empty, "multiwebcam_push_empty()", "Function", "空きバッファ返却\nEmptyキューにPush\nシグナル送信\nメモリクリア")

    Component(pull_action, "multiwebcam_pull_action()", "Function", "処理バッファ取得\nActionキューからPop\n待機タイムアウト\n条件変数待機")

    Component(push_action, "multiwebcam_push_action()", "Function", "処理バッファ追加\nActionキューにPush\nシグナル通知\n処理トリガー")

    ' Thread Synchronization
    Component(queue_mutex, "queue_mutex", "pthread_mutex_t", "キューアクセス排他制御\nクリティカルセクション保護")

    Component(queue_condition, "queue_condition", "pthread_cond_t", "キュー状態変更通知\nスレッド間同期\nwait/signal機構")

    Component(buffer_semaphore, "buffer_semaphore", "sem_t", "バッファ利用可能数管理\nカウンティングセマフォ\nリソース制限")

    ' Performance Monitoring
    Component(queue_stats, "queue_statistics_t", "Struct", "キュー性能統計\nqueue_depth_current\nmax_queue_depth\navg_wait_time\nbuffer_utilization")

    Component(update_queue_stats, "update_queue_statistics()", "Function", "統計更新\n深度計測\n待機時間記録\nスループット計算")

    Component(get_queue_depth, "get_current_queue_depth()", "Function", "現在キュー深度取得\nリアルタイム監視\n制御フィードバック")

    ' Memory Management
    Component(buffer_pool_init, "buffer_pool_init()", "Function", "バッファプール初期化\nメモリ確保\nアライメント調整\nプール管理構造作成")

    Component(buffer_cleanup, "buffer_pool_cleanup()", "Function", "バッファクリーンアップ\nメモリ解放\nmunmapクリア\nリソース解放")
}

Container_Boundary(thread_management_system, "Thread Management System") {
    Component(camera_thread, "camera_thread()", "Function", "カメラスレッド\n優先度110\n連続フレーム取得\nバッファ管理")

    Component(jpeg_sender_thread, "jpeg_sender()", "Function", "JPEG送信スレッド\nネットワーク送信\nクライアント管理\nバッファ処理")

    Component(thread_sync_manager, "thread_sync_manager", "Component", "スレッド同期管理\n開始・終了制御\nシグナル処理")
}

Container_Boundary(performance_monitoring_system, "Performance Monitoring") {
    Component(perf_counter, "performance_counter", "Component", "性能計測\nフレーム処理時間\nスループット測定")

    Component(fps_calculator, "fps_calculator", "Component", "FPS計算\nフレームカウント\n時間窓統計")
}

' Structure Relationships
Rel(prepare_camera_buf, v_buffer_struct, "バッファ構造初期化", "Memory Setup")
Rel(get_picture_buf, v4l2_buffer_struct, "V4L2バッファ操作", "Kernel Interface")
Rel(queue_state_struct, queue_mutex, "排他制御", "Mutex Access")
Rel(queue_state_struct, queue_condition, "条件変数", "Condition Access")

' Queue Operations Flow
Rel(pull_empty, queue_state_struct, "Emptyキューアクセス", "Queue Read")
Rel(push_empty, queue_state_struct, "Emptyキュー更新", "Queue Write")
Rel(pull_action, queue_state_struct, "Actionキューアクセス", "Queue Read")
Rel(push_action, queue_state_struct, "Actionキュー更新", "Queue Write")

' Buffer Lifecycle
Rel(prepare_camera_buf, buffer_pool_init, "バッファプール設定", "Pool Init")
Rel(get_picture_buf, pull_empty, "空きバッファ取得", "Buffer Request")
Rel(set_picture_buf, push_action, "処理バッファ追加", "Buffer Ready")
Rel(buffer_cleanup, buffer_pool_init, "リソース解放", "Cleanup")

' Thread Integration
Rel(camera_thread, get_picture_buf, "フレーム取得", "Buffer Operation")
Rel(camera_thread, push_action, "処理キュー追加", "Queue Operation")
Rel(jpeg_sender_thread, pull_action, "送信バッファ取得", "Queue Operation")
Rel(jpeg_sender_thread, push_empty, "空きバッファ返却", "Queue Operation")

' Synchronization Flow
Rel(pull_empty, queue_condition, "バッファ待機", "Condition Wait")
Rel(push_empty, queue_condition, "バッファ通知", "Condition Signal")
Rel(pull_action, queue_condition, "処理待機", "Condition Wait")
Rel(push_action, queue_condition, "処理通知", "Condition Signal")

' Performance Monitoring Integration
Rel(push_action, update_queue_stats, "統計更新", "Stats Update")
Rel(pull_action, update_queue_stats, "統計更新", "Stats Update")
Rel(get_queue_depth, queue_stats, "深度取得", "Stats Read")

' External Interface
Rel(get_queue_depth, fps_calculator, "キュー深度", "Control Feedback")
Rel(update_queue_stats, perf_counter, "性能データ", "Performance Data")

@enduml
```

## Level 4: Code Diagram - Thread Management & Camera Control System

```plantuml
@startuml c4_code_thread_camera_system
!include https://raw.githubusercontent.com/plantuml-stdlib/C4-PlantUML/master/C4_Component.puml

LAYOUT_WITH_LEGEND()

title Code Diagram - Thread Management & Camera Control System Implementation

Container_Boundary(thread_management_system, "Thread Management System") {

    ' Thread Control Structures
    Component(thread_config_struct, "thread_config_t", "Struct", "スレッド設定構造体\nthread_id: pthread_t\npriority: 優先度\nstack_size: スタックサイズ\nis_running: 実行状態\nstop_flag: 停止フラグ")

    Component(camera_thread_args, "camera_thread_args_t", "Struct", "カメラスレッド引数\ncamera_fd: ファイルディスクリプタ\nqueue_manager: キュー管理\nstats_collector: 統計収集")

    Component(sender_thread_args, "sender_thread_args_t", "Struct", "送信スレッド引数\nnetwork_config: ネットワーク設定\nclient_list: クライアントリスト\nprotocol_handler: プロトコル")

    ' Main Thread Functions
    Component(start_camera_thread, "multiwebcam_start_camerathread()", "Function", "カメラスレッド起動\n優先度110設定\nREALTIMEスケジューラ\npthread_create実行")

    Component(start_jpeg_sender, "multiwebcam_start_jpegsender()", "Function", "JPEG送信スレッド起動\nネットワーク初期化\nクライアント管理\nsocket bind")

    Component(camera_thread_main, "camera_thread()", "Function", "カメラメインループ\n連続フレーム取得\nV4L2 DQBUF/QBUF\nバッファ循環処理")

    Component(jpeg_sender_main, "jpeg_sender()", "Function", "送信メインループ\nクライアント接続管理\nMJPEG送信\nエラーハンドリング")

    ' Thread Synchronization
    Component(thread_barrier, "pthread_barrier_t", "Barrier", "スレッド同期バリア\n全スレッド起動待機\n初期化完了同期")

    Component(stop_condition, "pthread_cond_t", "Condition", "停止条件変数\n終了シグナル\nクリーンシャットダウン")

    Component(thread_mutex, "pthread_mutex_t", "Mutex", "スレッド間排他制御\n共有データ保護\nアトミック操作")

    ' Signal Handling
    Component(signal_handler, "signal_handler()", "Function", "シグナルハンドラ\nSIGINT/SIGTERM処理\nクリーンアップ実行\n全スレッド停止")

    Component(cleanup_handler, "thread_cleanup_handler()", "Function", "スレッドクリーンアップ\nリソース解放\nファイルディスクリプタ閉じる")

    ' Priority Control
    Component(set_thread_priority, "set_realtime_priority()", "Function", "リアルタイム優先度設定\nsched_setscheduler\nSCHED_FIFO\n優先度110-95")

    Component(adjust_priority, "adjust_thread_priority()", "Function", "動的優先度調整\n負荷状況対応\nUSBスレッド100→105")
}

Container_Boundary(camera_control_system, "Camera Control System") {

    ' V4L2 Camera Interface
    Component(camera_device_struct, "camera_device_t", "Struct", "カメラデバイス情報\nfd: ファイルディスクリプタ\nfmt: フォーマット情報\nbuffers: バッファ配列\nbuffer_count: バッファ数")

    Component(camera_init, "camera_init()", "Function", "カメラ初期化\nV4L2デバイス開く\nフォーマット設定\nバッファ確保")

    Component(camera_set_format, "camera_set_format()", "Function", "フォーマット設定\nVIDIOC_S_FMT\n解像度・FPS設定\nJPEG圧縮設定")

    Component(camera_start_streaming, "camera_start_streaming()", "Function", "ストリーミング開始\nVIDIOC_STREAMON\nバッファキューイング")

    Component(camera_stop_streaming, "camera_stop_streaming()", "Function", "ストリーミング停止\nVIDIOC_STREAMOFF\nバッファ回収")

    Component(camera_capture_frame, "camera_capture_frame()", "Function", "フレーム取得\nVIDIOC_DQBUF\nタイムスタンプ記録\nJPEGサイズ取得")

    Component(camera_return_buffer, "camera_return_buffer()", "Function", "バッファ返却\nVIDIOC_QBUF\nバッファ再利用")

    ' Camera Configuration
    Component(camera_config_struct, "camera_config_t", "Struct", "カメラ設定\nwidth: 320-1280\nheight: 240-720\nfps: フレームレート\nformat: V4L2_PIX_FMT_JPEG")

    Component(camera_controls, "camera_controls_t", "Struct", "カメラ制御\nbrightness: 輝度\ncontrast: コントラスト\nwhite_balance: ホワイトバランス\nexposure: 露出")

    Component(set_camera_controls, "set_camera_controls()", "Function", "カメラ制御設定\nVIDIOC_S_CTRL\n画質パラメータ調整")

    ' Runtime FPS Control
    Component(set_fps_runtime, "camera_set_fps_runtime()", "Function", "実行時FPS変更\nV4L2_CID_EXPOSURE\nフレームレート動的調整\n制御フィードバック")

    Component(get_current_fps, "camera_get_current_fps()", "Function", "現在FPS取得\nフレーム間隔測定\n実測値計算")
}

Container_Boundary(network_client_management, "Network Client Management") {
    Component(client_manager, "client_manager_t", "Struct", "クライアント管理\nsocket_fd: ソケット\nclient_addr: アドレス\nconnect_time: 接続時間\nis_active: 接続状態")

    Component(accept_client, "accept_new_client()", "Function", "新規クライアント受付\naccept() システムコール\nクライアント登録")

    Component(send_mjpeg_stream, "send_mjpeg_stream()", "Function", "MJPEGストリーム送信\nHTTPヘッダー\nマルチパート送信")
}

' Thread Creation Flow
Rel(start_camera_thread, thread_config_struct, "設定初期化", "Config Setup")
Rel(start_camera_thread, set_thread_priority, "優先度設定", "Priority Set")
Rel(start_camera_thread, camera_thread_main, "スレッド実行", "pthread_create")

Rel(start_jpeg_sender, sender_thread_args, "引数設定", "Args Setup")
Rel(start_jpeg_sender, jpeg_sender_main, "送信スレッド実行", "pthread_create")

' Thread Synchronization
Rel(camera_thread_main, thread_barrier, "同期待機", "Barrier Wait")
Rel(jpeg_sender_main, thread_barrier, "同期待機", "Barrier Wait")
Rel(signal_handler, stop_condition, "停止通知", "Condition Signal")
Rel(camera_thread_main, stop_condition, "停止監視", "Condition Check")

' Camera Operations Integration
Rel(camera_thread_main, camera_capture_frame, "フレーム取得", "V4L2 Operation")
Rel(camera_capture_frame, camera_device_struct, "デバイスアクセス", "FD Access")
Rel(camera_init, camera_set_format, "フォーマット初期化", "Initial Setup")
Rel(camera_thread_main, camera_return_buffer, "バッファ返却", "V4L2 QBUF")

' Runtime Control Integration
Rel(set_fps_runtime, camera_config_struct, "設定更新", "Config Update")
Rel(get_current_fps, camera_device_struct, "FPS測定", "Frame Timing")
Rel(adjust_priority, thread_config_struct, "優先度更新", "Priority Adjust")

' Network Integration
Rel(jpeg_sender_main, accept_client, "クライアント管理", "Connection Accept")
Rel(jpeg_sender_main, send_mjpeg_stream, "ストリーム送信", "Network Send")
Rel(send_mjpeg_stream, client_manager, "クライアント状態", "Connection Status")

' Control System Interface
Rel(camera_thread_main, set_camera_controls, "画質制御", "V4L2 Controls")
Rel(set_camera_controls, camera_controls, "制御パラメータ", "Control Values")

' Cleanup and Error Handling
Rel(signal_handler, cleanup_handler, "クリーンアップ実行", "Resource Cleanup")
Rel(cleanup_handler, camera_stop_streaming, "カメラ停止", "V4L2 Stop")

@enduml
```

## Level 4: Code Diagram - Metrics Collection & Stream Arbitration System

```plantuml
@startuml c4_code_metrics_arbitration_system
!include https://raw.githubusercontent.com/plantuml-stdlib/C4-PlantUML/master/C4_Component.puml

LAYOUT_WITH_LEGEND()

title Code Diagram - Metrics Collection & Stream Arbitration System Implementation

Container_Boundary(metrics_collection_system, "Metrics Collection System") {

    ' Core Metrics Data Structures
    Component(perf_frame_metrics_struct, "perf_frame_metrics_t", "Struct", "フレーム毎メトリクス\nframe_num, interval_us\ntimestamp群(CLOCK_MONOTONIC)\njpeg_size, packet_size, usb_written\nlatency_camera, latency_pack, latency_usb")

    Component(perf_stats_struct, "perf_stats_t", "Struct", "統計データ集計\nwindow_start_us, frames_in_window\ntotal_jpeg/packet/usb_bytes\navg_latency群, min/max値\ntotal_usb_retries, dropped_frames")

    Component(metrics_packet_struct, "metrics_packet_t", "Struct", "メトリクスパケット(58bytes)\nsync_word: 0xCAFEBEEF\nsequence, timestamp_ms\ncamera_frames, usb_packets\naction_q_depth, avg_packet_size\ntcp_health統計, crc16")

    ' Collection Functions
    Component(perf_logger_init, "perf_logger_init()", "Function", "性能ログ初期化\nreset_stats()実行\nframe_timestamp初期化\nログ間隔設定(30frames)")

    Component(perf_record_frame, "perf_logger_record_frame()", "Function", "フレーム毎記録\nCLOCK_MONOTONIC取得\nlatency計算・累積\nmin/max更新")

    Component(perf_print_stats, "perf_logger_print_stats()", "Function", "統計出力\nFPS計算\nthroughput_mbps算出\nUSB帯域使用率(12Mbps基準)\n包括統計ログ出力")

    Component(collect_current_metrics, "collect_current_metrics()", "Function", "現在メトリクス収集\nget_uptime_ms()\nframe_queue_depth()\navg_packet_size計算\nTCP統計取得")

    ' Timing Control
    Component(metrics_timer, "metrics_timing_control", "Component", "メトリクス送信タイミング制御\nMETRICS_INTERVAL_MS: 1000ms\nPERF_LOG_INTERVAL: 30frames\nlast_metrics_time管理")
}

Container_Boundary(stream_arbitration_system, "Stream Arbitration System") {

    ' Priority Management
    Component(thread_priority_config, "thread_priority_config", "Config", "スレッド優先度設定\nCAMERA_THREAD_PRIORITY: 110\nUSB_THREAD_PRIORITY: 100\nCONTROL_THREAD_PRIORITY: 95")

    Component(priority_inheritance_mutex, "priority_inheritance_mutex", "pthread_mutex_t", "優先度継承ミューテックス\nPTHREAD_PRIO_INHERIT\n優先度逆転防止")

    Component(adjust_thread_priority, "adjust_thread_priority()", "Function", "動的優先度調整\nUSBスレッド100→105\nキュー飽和時ブースト\nQUEUE_SATURATION_THRESHOLD: 6")

    ' Queue Management & Arbitration
    Component(action_queue_mgmt, "action_queue_management", "Component", "Actionキュー管理\nFIFO順序保証\n映像フレーム+メトリクス統合\n優先度: 映像>メトリクス")

    Component(frame_drop_logic, "frame_drop_logic", "Component", "フレーム廃棄判定\ntime-based: >250ms×3回\nqueue-based: depth>=6\nDROP_FRAME_COUNT: 3\nメトリクス廃棄なし")

    Component(bandwidth_allocation, "bandwidth_allocation", "Component", "帯域配分制御\nUSB Full Speed: 12Mbps\nメトリクス: <0.1%消費\n映像: 残り帯域使用\n使用率監視・警告")

    ' Congestion Control
    Component(slow_send_detection, "slow_send_detection()", "Function", "送信遅延検出\nSLOW_SEND_THRESHOLD: 250ms\nSLOW_SEND_COUNT_MAX: 3\n連続遅延カウント")

    Component(queue_depth_monitor, "queue_depth_monitor()", "Function", "キュー深度監視\naction_queue深度取得\nsaturation閾値判定\n統計収集・ログ出力")

    Component(drop_old_frames, "drop_old_frames()", "Function", "古フレーム廃棄実行\nActionキューから古いフレーム除去\nEmptyキューにリサイクル\ng_dropped_frames更新")
}

Container_Boundary(transmission_coordination, "Transmission Coordination System") {

    ' Metrics Transmission
    Component(send_metrics_packet, "send_metrics_packet()", "Function", "メトリクスパケット送信\n1000ms間隔実行\ncurrent metrics収集\nmjpeg_pack_metrics()呼出\nActionキューに投入")

    Component(mjpeg_pack_metrics, "mjpeg_pack_metrics()", "Function", "メトリクスパケット化\n12フィールド格納\nCRC16_CCITT計算\nsequence番号増分\n58bytes固定サイズ")

    Component(usb_write_coordination, "usb_write_coordination()", "Function", "USB送信調整\n映像フレーム優先処理\nメトリクスは確実送信\ntimeout制御・retry処理")

    ' Protocol Handling
    Component(packet_type_dispatch, "packet_type_dispatch()", "Function", "パケットタイプ振り分け\n映像: 0xCAFEBABE\nメトリクス: 0xCAFEBEEF\n適切なhandlerに転送")

    Component(crc_validation, "mjpeg_crc16_ccitt()", "Function", "CRC16チェックサム\n映像・メトリクス共通\npacket integrity保証\nCCITTアルゴリズム")

    ' Statistics Integration
    Component(global_metrics_vars, "global_metrics_variables", "Static Variables", "グローバルメトリクス変数\ng_total_camera_frames\ng_total_usb_packets\ng_total_packet_bytes\ng_dropped_frames, g_drop_events")
}

Container_Boundary(performance_monitoring, "Performance Monitoring Integration") {

    ' Resource Monitoring
    Component(resource_usage_monitor, "resource_usage_monitor", "Component", "リソース使用率監視\nUSB帯域: 12Mbps基準\nメモリ使用量\nCPU使用率\nスレッド競合検出")

    Component(tcp_health_integration, "tcp_health_stats_integration", "Component", "TCP健全性統計統合\nmoving_avg_send_time\nspike_count\nPhase 9.2統計\nメトリクスパケットに含有")

    Component(exit_statistics, "exit_statistics_logger", "Component", "終了時統計出力\n処理フレーム総数\nJPEG検証エラー率\n平均JPEG サイズ\n包括性能サマリー")
}

' Metrics Collection Flow
Rel(perf_logger_init, perf_stats_struct, "統計構造初期化", "Memory Setup")
Rel(perf_record_frame, perf_frame_metrics_struct, "フレーム記録", "Data Fill")
Rel(perf_record_frame, perf_stats_struct, "統計累積", "Accumulation")
Rel(perf_print_stats, perf_stats_struct, "統計計算・出力", "Analysis")

' Timing Control Integration
Rel(metrics_timer, collect_current_metrics, "1000ms間隔トリガー", "Timer Event")
Rel(collect_current_metrics, send_metrics_packet, "メトリクス送信", "Data Ready")
Rel(send_metrics_packet, mjpeg_pack_metrics, "パケット化", "Serialization")

' Arbitration System Integration
Rel(thread_priority_config, priority_inheritance_mutex, "優先度継承設定", "Mutex Config")
Rel(queue_depth_monitor, adjust_thread_priority, "動的優先度調整", "Priority Boost")
Rel(slow_send_detection, frame_drop_logic, "廃棄判定", "Congestion Response")
Rel(frame_drop_logic, drop_old_frames, "廃棄実行", "Frame Removal")

' Transmission Coordination
Rel(send_metrics_packet, action_queue_mgmt, "メトリクス投入", "Queue Push")
Rel(action_queue_mgmt, packet_type_dispatch, "パケット振り分け", "Type Detection")
Rel(mjpeg_pack_metrics, crc_validation, "CRC計算", "Integrity Check")
Rel(usb_write_coordination, bandwidth_allocation, "帯域制御", "Bandwidth Check")

' Performance Integration
Rel(resource_usage_monitor, bandwidth_allocation, "使用率監視", "Resource Check")
Rel(tcp_health_integration, metrics_packet_struct, "健全性データ", "Health Stats")
Rel(global_metrics_vars, collect_current_metrics, "統計データ読取", "Stats Access")

' Cross-System Dependencies
Rel(perf_record_frame, global_metrics_vars, "グローバル統計更新", "Stats Update")
Rel(drop_old_frames, global_metrics_vars, "廃棄カウント更新", "Drop Stats")
Rel(queue_depth_monitor, perf_stats_struct, "キュー統計", "Queue Stats")

@enduml
```

## Level 4: Code Diagram - Batch Processing & Advanced Metrics System

```plantuml
@startuml c4_code_batch_advanced_metrics
!include https://raw.githubusercontent.com/plantuml-stdlib/C4-PlantUML/master/C4_Component.puml

LAYOUT_WITH_LEGEND()

title Code Diagram - Batch Processing & Advanced Metrics System Implementation

Container_Boundary(batch_processing_system, "Multi-Frame Batch Processing System (Phase 7.2a)") {

    ' Batch Data Structures
    Component(mjpeg_batch_header_struct, "mjpeg_batch_header_t", "Struct", "バッチヘッダー\nsync_word: 0xCAFEBABF\nbatch_sequence\nframe_count: 1-3 frames\ntotal_size: JPEG合計サイズ")

    Component(mjpeg_frame_meta_struct, "mjpeg_frame_meta_t", "Struct", "フレームメタデータ\nframe_sequence: 個別sequence\nframe_size: JPEGサイズ\n各フレームの識別情報")

    Component(batch_buffer_struct, "batch_buffer_t", "Struct", "バッチバッファ\nframes[MJPEG_BATCH_SIZE]\ncurrent_count, total_size\ntimeout_timestamp\nbatch完成状態管理")

    ' Batch Configuration
    Component(batch_config, "batch_configuration", "Config", "バッチ設定\nMJPEG_BATCHING_ENABLED: 0(無効)\nMJPEG_BATCH_SIZE: 2 frames\nMJPEG_BATCH_TIMEOUT_MS: 100\nMAX_BATCH_PACKET: ~185KB")

    ' Batch Processing Functions
    Component(batch_add_frame, "batch_add_frame()", "Function", "フレーム追加\nbatchバッファに追加\nsize制限チェック\ntimeout管理\n完成判定")

    Component(batch_pack_frames, "mjpeg_pack_batch_frames()", "Function", "バッチパケット化\nheader + meta[] + jpeg_data[]\nCRC16計算(全体)\nsequence管理")

    Component(batch_timeout_check, "batch_timeout_check()", "Function", "タイムアウトチェック\n100ms経過判定\n部分batch送信\nタイマーリセット")

    Component(batch_send_complete, "batch_send_complete()", "Function", "完成batch送信\nTCP single write\n~185KB一括送信\n効率向上")
}

Container_Boundary(advanced_metrics_system, "Advanced Metrics & Statistics System") {

    ' Phase-specific Metrics
    Component(phase4_metrics, "phase4_metrics_t", "Struct", "Phase 4.1メトリクス\nmetrics_packet作成統計\nJPEG validation errors\npacket作成時間")

    Component(phase7_metrics, "phase7_metrics_t", "Struct", "Phase 7.3.3メトリクス\nframe_drop_events\ndropped_frames総数\nTCP send時間統計\nmax_send_us記録")

    Component(phase9_metrics, "phase9_metrics_t", "Struct", "Phase 9.2メトリクス\ntcp_health_moving_avg_ms\ntcp_health_total_spikes\n健全性分類統計\n予防的再接続回数")

    Component(phase11_metrics, "phase11_metrics_t", "Struct", "Phase 11メトリクス\nframe_complexity統計\nadaptive_control実績\nPID制御パラメータ履歴\ncontrol_effectiveness")

    ' Advanced Statistical Functions
    Component(moving_average_calc, "calculate_moving_average()", "Function", "移動平均計算\nwindow_size設定可能\nexponential smoothing\noutlier除去")

    Component(spike_detection, "detect_performance_spikes()", "Function", "性能スパイク検出\nthreshold設定(Phase 9.2)\nspike_count更新\nalert generation")

    Component(variance_analysis, "calculate_variance_stats()", "Function", "分散解析\ncoefficient of variation\nstability metrics\ncontrol quality assessment")

    Component(trend_analysis, "analyze_performance_trends()", "Function", "性能トレンド解析\nregression analysis\nperformance prediction\ndegradation detection")

    ' Complex Metrics Integration
    Component(multi_phase_aggregator, "multi_phase_metrics_aggregator", "Component", "複数Phase統計統合\nPhase 4→7→9.2→11\n横断的性能評価\n統合レポート生成")

    Component(health_classifier, "tcp_health_classifier", "Component", "TCP健全性分類\nEXCELLENT/GOOD/FAIR/POOR/CRITICAL\n5段階分類アルゴリズム\nhysteresis制御")
}

Container_Boundary(real_time_monitoring, "Real-time Monitoring & Alert System") {

    ' Real-time Thresholds
    Component(threshold_config, "performance_thresholds", "Config", "性能閾値設定\nUSB_BANDWIDTH_WARN: 80%\nUSB_BANDWIDTH_ERROR: 100%\nQUEUE_SATURATION: 6 frames\nSLOW_SEND_THRESHOLD: 250ms")

    Component(alert_generator, "generate_performance_alert()", "Function", "性能アラート生成\nthreshold violation検出\nLOG_WARN/LOG_ERROR出力\nescalation logic")

    Component(bandwidth_monitor, "usb_bandwidth_monitor()", "Function", "帯域監視\nthroughput_mbps計算\n12Mbps基準使用率\n超過検出・警告")

    Component(queue_saturation_detector, "queue_saturation_detector()", "Function", "キュー飽和検出\naction_queue深度監視\nsaturation予測\nproactive measures")

    ' Performance Violation Handlers
    Component(bandwidth_violation_handler, "handle_bandwidth_violation()", "Function", "帯域違反処理\nframe drop trigger\nquality reduction\nrecovery strategy")

    Component(latency_violation_handler, "handle_latency_violation()", "Function", "遅延違反処理\nslow send detection\nconsecutive threshold\nframe drop activation")

    ' System Health Dashboard
    Component(health_dashboard_data, "system_health_dashboard_data", "Struct", "システム健全性データ\noverall_health_score\ncomponent_health[]\nrecent_violations[]\nsystem_stability_index")
}

Container_Boundary(debug_diagnostics_system, "Debug & Diagnostics System") {

    ' Debug Data Structures
    Component(debug_trace_buffer, "debug_trace_buffer_t", "Struct", "デバッグトレースバッファ\ntimestamp_us[]\nevent_type[]\nevent_data[]\ncircular buffer")

    Component(error_tracking, "error_tracking_stats_t", "Struct", "エラー追跡統計\nerror_type_counts[]\nerror_recovery_times[]\nerror_patterns[]\nrecurrent_error_detection")

    ' Diagnostic Functions
    Component(trace_event_log, "trace_event_log()", "Function", "イベントトレース記録\nhigh-resolution timestamp\nevent categorization\ncircular buffer管理")

    Component(performance_profiler, "performance_profiler()", "Function", "性能プロファイル\nfunction execution times\nbottleneck identification\ncode path analysis")

    Component(memory_leak_detector, "memory_leak_detector()", "Function", "メモリリーク検出\nbuffer allocation tracking\nmemory pool monitoring\nleak pattern analysis")

    Component(system_state_dump, "dump_system_state()", "Function", "システム状態ダンプ\nthread states\nqueue contents\nresource utilization\ncomprehensive snapshot")
}

' Batch Processing Flow
Rel(batch_add_frame, batch_buffer_struct, "フレーム蓄積", "Buffer Fill")
Rel(batch_timeout_check, batch_add_frame, "タイムアウト監視", "Timer Check")
Rel(batch_pack_frames, mjpeg_batch_header_struct, "バッチヘッダー作成", "Header Fill")
Rel(batch_send_complete, batch_pack_frames, "完成batch処理", "Batch Ready")

' Advanced Metrics Integration
Rel(phase7_metrics, moving_average_calc, "TCP送信時間統計", "Moving Avg")
Rel(phase9_metrics, spike_detection, "健全性スパイク検出", "Spike Analysis")
Rel(phase11_metrics, variance_analysis, "制御分散解析", "Control Stats")
Rel(multi_phase_aggregator, trend_analysis, "統合トレンド解析", "Trend Calc")

' Real-time Monitoring Integration
Rel(bandwidth_monitor, threshold_config, "閾値参照", "Threshold Check")
Rel(alert_generator, bandwidth_violation_handler, "帯域違反対応", "Violation Response")
Rel(queue_saturation_detector, latency_violation_handler, "遅延違反処理", "Latency Response")
Rel(health_classifier, health_dashboard_data, "健全性更新", "Health Update")

' Debug System Integration
Rel(trace_event_log, debug_trace_buffer, "トレース記録", "Event Log")
Rel(performance_profiler, error_tracking, "性能問題追跡", "Error Analysis")
Rel(memory_leak_detector, system_state_dump, "メモリ状態記録", "State Capture")

' Cross-System Dependencies
Rel(batch_config, batch_add_frame, "設定参照", "Config Access")
Rel(phase7_metrics, alert_generator, "Phase 7統計", "Stats Input")
Rel(health_dashboard_data, system_state_dump, "健全性状態", "Health Snapshot")

@enduml
```

## 図の説明

### Level 1: System Context
- システム全体の外部関係者とのやり取りを示す
- 監視オペレータ、システム管理者の役割
- 外部システム（ネットワーク、ストレージ）との関係

### Level 2: Container
- SpresenseエッジデバイスとPCホストシステムの主要コンテナ
- 各コンテナ間の通信プロトコルと技術スタック
- WiFi/USB通信チャネルの使い分け

### Level 3: Component (Spresense)
- Spresenseアプリケーション内の詳細コンポーネント構造
- Phase 10-11制御システムの実装構成
- フレーム処理パイプラインとメトリクス収集

### Level 4: Code (FPS制御)
- FPS制御システムの具体的な関数・構造体レベル
- PID制御器とPhase 11拡張制御の実装関係
- フレーム統計システムとの連携