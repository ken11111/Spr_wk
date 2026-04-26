# 技術特化ライブラリ使用例 - セキュリティカメラシステム

## ネットワーク図（Font Awesome 5使用）

### Network Infrastructure Diagram

```plantuml
@startuml network_infrastructure
!theme plain

title Security Camera Network Infrastructure

package "Edge Device Network" {
    rectangle "Spresense\nSecurity Camera" as spresense {
        rectangle "ISX012\nCamera" as camera
        rectangle "GS2200M\nWiFi Module" as wifi_module
    }
}

package "Network Infrastructure" {
    rectangle "WiFi Router\n(IDY iS110B)" as router
    rectangle "Network\nGateway" as gateway
    rectangle "Internet\nCloud" as internet
}

package "Host Environment" {
    rectangle "PC Host\nPython Application" as pc_host
    rectangle "Local Storage\n(Recordings)" as storage
    rectangle "Web Dashboard\n(Monitoring)" as dashboard
}

' Network Connections
spresense -right-> router : "TCP/IP\n10080 Port\nMJPEG Stream"
router -down-> gateway : "DHCP\n192.168.11.x"
gateway -right-> internet : "Internet Access"

router -down-> pc_host : "TCP Connection\nMetrics & Video"
pc_host -down-> storage : "File I/O\nRecording Data"
pc_host -right-> dashboard : "HTTP/WebSocket\nControl Interface"

' USB Fallback
spresense -up-> pc_host : "USB CDC-ACM\n/dev/ttyACM0\n(Fallback)"

note right of spresense
  **Network Capabilities**
  • WiFi 802.11 b/g/n
  • TCP Server: Port 10080
  • USB CDC-ACM Backup
  • MJPEG Protocol v3.0
end note

note right of pc_host
  **Host Functions**
  • Multi-camera Reception (4台)
  • Real-time Metrics Analysis
  • Performance Dashboard
  • Control System Analysis
end note

@enduml
```

## SysML-Style Block Definition Diagram (Class Diagram with SysML Stereotypes)

### System Architecture using SysML-Style Notation

```plantuml
@startuml sysml_block_definition
!theme plain

title SysML-Style Block Definition - Security Camera System

package "Security Camera System" <<system>> {

  class "Spresense Device" <<(B,#FFB6C1) block>> {
    .. Parts ..
    camera_sensor : ISX012_Camera_Sensor
    main_processor : CXD5602_ARM_Processor
    wifi_module : GS2200M_WiFi_Module
    encoder : Hardware_JPEG_Encoder

    .. Values (Properties) ..
    operating_voltage : Real = 5.0 [V]
    power_consumption : Real = 2.5 [W]
    operating_temp : Real = -10..85 [°C]
  }

  note right of "Spresense Device"
    **Constraints:**
    • max_fps <= 30
    • jpeg_quality >= 50 && jpeg_quality <= 95
    • usb_bandwidth <= 12 [Mbps]
  end note

  class "ISX012_Camera_Sensor" <<(S,#87CEEB) sensor>> {
    .. Values (Properties) ..
    resolution_width : Integer = 640 [pixels]
    resolution_height : Integer = 480 [pixels]
    max_fps : Real = 30 [fps]
    pixel_format : String = "JPEG"

    .. Operations ..
    + capture_frame() : VideoFrame
    + set_resolution(width, height)
    + set_fps(fps)
  }

  class "CXD5602_ARM_Processor" <<(P,#98FB98) processor>> {
    .. Values (Properties) ..
    core_count : Integer = 6
    clock_speed : Real = 156 [MHz]
    architecture : String = "ARM Cortex-M4F"

    .. Parts (Threads) ..
    camera_thread : Thread [priority=110]
    usb_thread : Thread [priority=100]
    control_thread : Thread [priority=95]

    .. Operations ..
    + schedule_threads()
    + process_frame(frame)
  }

  class "GS2200M_WiFi_Module" <<(W,#DDA0DD) communication>> {
    .. Values (Properties) ..
    standards : String = "802.11 b/g/n"
    frequency : Real = 2.4 [GHz]
    max_throughput : Real = 65 [Mbps]

    .. Operations ..
    + connect(ssid, password)
    + send_tcp(data, port)
    + get_connection_status()
  }

  class "PC_Host_System" <<(H,#F0E68C) software>> {
    .. Parts ..
    stream_receiver : NetImgReceiver
    metrics_analyzer : MetricsAnalyzer
    display_system : MultiCamFrame
    dashboard : WebDashboard

    .. Values (Properties) ..
    supported_cameras : Integer = 4
    display_fps : Real = 60 [Hz]
    analysis_interval : Real = 1000 [ms]

    .. Operations ..
    + receive_stream()
    + analyze_metrics()
    + display_video()
  }

  class "Performance_Control_System" <<(C,#FFD700) control>> {
    .. Parts ..
    fps_controller : PID_Controller
    frame_statistics : StatisticsAnalyzer
    adaptive_control : Phase11_Controller

    .. Values (Control Parameters) ..
    control_period : Real = 100 [ms]
    setpoint : Real = 3.5 [frames]
    kp : Real = 0.15
    ki : Real = 0.02
    kd : Real = 0.0

    .. Operations ..
    + update_control()
    + calculate_fps_adjustment()
  }

  note right of "Performance_Control_System"
    **Constraints:**
    • queue_depth >= 5 && queue_depth <= 9
    • fps_output >= 5 && fps_output <= 30
    • control_stability > 0.8
  end note
}

' Relationships (Composition and Association)
"Spresense Device" *-- "ISX012_Camera_Sensor" : contains
"Spresense Device" *-- "CXD5602_ARM_Processor" : contains
"Spresense Device" *-- "GS2200M_WiFi_Module" : contains
"CXD5602_ARM_Processor" *-- "Performance_Control_System" : contains

"ISX012_Camera_Sensor" --> "CXD5602_ARM_Processor" : video_data
"CXD5602_ARM_Processor" --> "GS2200M_WiFi_Module" : tcp_packets
"GS2200M_WiFi_Module" --> "PC_Host_System" : network_stream

@enduml
```

## SysML Requirements Diagram

### System Requirements with Traceability

```plantuml
@startuml sysml_requirements
!theme plain

title SysML Requirements - Security Camera System

package "Functional Requirements" <<requirements>> {

  class "FR-001: Video Capture" <<(R,#FFB6C1) requirement>> {
    id: FR-001
    text: "System shall capture video at configurable frame rates between 5-30 fps"
    verifyMethod: Test
    priority: High
    rationale: "Core functionality for security monitoring"
  }

  class "FR-002: Stream Transmission" <<(R,#87CEEB) requirement>> {
    id: FR-002
    text: "System shall transmit video stream via WiFi TCP connection"
    verifyMethod: Test
    priority: High
    rationale: "Remote monitoring capability"
  }

  class "FR-003: Performance Monitoring" <<(R,#98FB98) requirement>> {
    id: FR-003
    text: "System shall collect and transmit performance metrics every 1000ms"
    verifyMethod: Inspection
    priority: Medium
    rationale: "System health monitoring and optimization"
  }

  class "FR-004: Adaptive Control" <<(R,#DDA0DD) requirement>> {
    id: FR-004
    text: "System shall adapt FPS based on queue depth and network conditions"
    verifyMethod: Analysis
    priority: High
    rationale: "Maintain stream quality under varying conditions"
  }
}

package "Performance Requirements" <<requirements>> {

  class "PR-001: Latency" <<(P,#F0E68C) requirement>> {
    id: PR-001
    text: "End-to-end latency shall be less than 150ms"
    verifyMethod: Test
    priority: High
    rationale: "Real-time monitoring requirement"
  }

  class "PR-002: Frame Quality" <<(P,#FFDAB9) requirement>> {
    id: PR-002
    text: "JPEG quality shall be maintained between 50-95%"
    verifyMethod: Test
    priority: Medium
    rationale: "Balance between quality and bandwidth"
  }

  class "PR-003: Bandwidth Efficiency" <<(P,#FFE4E1) requirement>> {
    id: PR-003
    text: "USB bandwidth utilization shall not exceed 80% under normal operation"
    verifyMethod: Analysis
    priority: Medium
    rationale: "Prevent system saturation"
  }

  class "PR-004: Control Stability" <<(P,#E0E6FF) requirement>> {
    id: PR-004
    text: "PID control system shall maintain queue depth at 3.5 ± 0.5 frames"
    verifyMethod: Test
    priority: High
    rationale: "Stable system operation"
  }
}

package "Safety Requirements" <<requirements>> {

  class "SR-001: Graceful Degradation" <<(S,#FFD700) requirement>> {
    id: SR-001
    text: "System shall drop video frames before dropping metrics packets"
    verifyMethod: Test
    priority: High
    rationale: "Maintain system observability"
  }

  class "SR-002: Resource Protection" <<(S,#FF6B6B) requirement>> {
    id: SR-002
    text: "System shall prevent memory leaks and buffer overflows"
    verifyMethod: Analysis
    priority: Critical
    rationale: "System stability and security"
  }
}

' Requirement Relationships
"FR-001: Video Capture" --> "FR-002: Stream Transmission" : enables
"FR-002: Stream Transmission" --> "FR-003: Performance Monitoring" : requires
"FR-003: Performance Monitoring" --> "FR-004: Adaptive Control" : supports
"FR-004: Adaptive Control" --> "PR-001: Latency" : satisfies
"FR-004: Adaptive Control" --> "PR-004: Control Stability" : implements

"PR-001: Latency" --> "PR-002: Frame Quality" : constrains
"PR-002: Frame Quality" --> "PR-003: Bandwidth Efficiency" : balances

"FR-004: Adaptive Control" --> "SR-001: Graceful Degradation" : implements
"SR-001: Graceful Degradation" --> "SR-002: Resource Protection" : ensures

note right of "FR-004: Adaptive Control"
  **Implementation Note**
  Phase 11 multi-variable control
  with adaptive PID gains based on
  scene complexity and network conditions
end note

@enduml
```

## SysML-Style Internal Block Diagram (Component Diagram)

### Thread Architecture & Data Flow

```plantuml
@startuml sysml_internal_block
!theme plain

title SysML-Style Internal Block - Spresense Thread Architecture

component "Spresense_Security_Camera" {

  ' Thread Components
  component "Camera_Thread" as camera_thread <<(T,#FFB6C1) Thread[priority=110]>> {
    portin frame_out
    portout control_in
  }

  component "USB_Thread" as usb_thread <<(T,#87CEEB) Thread[priority=100]>> {
    portin data_in
    portout usb_out
    portin priority_control
  }

  component "Control_Thread" as control_thread <<(T,#98FB98) Thread[priority=95]>> {
    portin metrics_in
    portout control_out
  }

  ' Queue Systems
  component "Action_Queue" as action_queue <<(Q,#DDA0DD) Queue[size=5..9]>> {
    portin push_port
    portout pull_port
    portout depth_monitor
  }

  component "Empty_Queue" as empty_queue <<(Q,#F0E68C) Queue>> {
    portin recycle_port
    portout allocate_port
  }

  ' Control Systems
  component "FPS_Controller" as fps_controller <<(C,#FFD700) Controller>> {
    portin error_in
    portout fps_out
  }

  component "Metrics_Collector" as metrics_collector <<(M,#FFDAB9) Collector>> {
    portin stats_in
    portout metrics_out
  }
}

' Internal Connections
camera_thread.frame_out --> action_queue.push_port : "JPEG_Frames"
action_queue.pull_port --> usb_thread.data_in : "Frame_Buffers"
usb_thread.usb_out --> empty_queue.recycle_port : "Empty_Buffers"
empty_queue.allocate_port --> camera_thread : "Buffer_Allocation"

action_queue.depth_monitor --> control_thread.metrics_in : "Queue_Depth"
control_thread.control_out --> fps_controller.error_in : "Control_Signal"
fps_controller.fps_out --> camera_thread.control_in : "FPS_Command"

camera_thread --> metrics_collector.stats_in : "Frame_Stats"
usb_thread --> metrics_collector.stats_in : "Transmission_Stats"
metrics_collector.metrics_out --> action_queue.push_port : "Metrics_Packet"

' Priority Control (Dynamic)
action_queue.depth_monitor --> usb_thread.priority_control : "Priority_Boost[>=6_frames]"

note top of camera_thread
  **Highest Priority**
  Ensures frame capture
  from V4L2 driver

  **Control Parameters:**
  • kp = 0.15
  • ki = 0.02
  • setpoint = 3.5
  • period = 100 [ms]
end note

note top of usb_thread
  **Medium Priority**
  Can boost to 105 when
  queue depth >= 6
end note

note top of control_thread
  **Lowest Priority**
  Background control
  100ms cycle
end note

@enduml
```

## Elastic Stack Architecture (ログ解析システム例)

### Security Camera Metrics Analysis Pipeline

```plantuml
@startuml camera_metrics_analysis
!theme plain

title Security Camera Metrics Analysis Pipeline (ELK Stack Style)

package "Data Collection" {
  rectangle "Spresense\nCamera" as spresense
  rectangle "PC Host\nAnalysis" as pc_host
  rectangle "CSV Metrics\nFiles" as csv_files
}

package "Log Processing Pipeline" {
  rectangle "File Monitor" as filebeat
  rectangle "Data Parser" as logstash {
    rectangle "CSV Filter"
    rectangle "Data Transform"
    rectangle "JSON Output"
  }
}

package "Storage & Search" {
  rectangle "Metrics Database" as elasticsearch {
    rectangle "camera-metrics-*"
    rectangle "control-logs-*"
    rectangle "performance-*"
  }
}

package "Visualization & Monitoring" {
  rectangle "Analytics Dashboard" as kibana {
    rectangle "FPS Trends"
    rectangle "Queue Analysis"
    rectangle "Control Performance"
    rectangle "Alert Manager"
  }
}

' Data Flow Pipeline
spresense --> pc_host : "Real-time Metrics\n(1000ms interval)"
pc_host --> csv_files : "CSV Files\n(performance logs)"
csv_files --> filebeat : "File Monitoring\n(inotify watch)"
filebeat --> logstash : "Raw Log Events"
logstash --> elasticsearch : "Structured Data\n(JSON documents)"
elasticsearch --> kibana : "Query Results\n(aggregations)"

' Configuration Details
note right of filebeat
  **Filebeat Configuration**
  ```yaml
  filebeat.inputs:
  - type: log
    paths:
      - /metrics/camera_*.csv
      - /metrics/control_*.csv
    fields:
      system: security_camera
      environment: production
  output.logstash:
    hosts: ["localhost:5044"]
  ```
end note

note right of logstash
  **Logstash Pipeline**
  ```ruby
  input { beats { port => 5044 } }
  filter {
    csv {
      columns => ["timestamp", "fps",
                  "queue_depth", "latency",
                  "jpeg_size", "tcp_time"]
    }
    mutate {
      convert => {
        "fps" => "float"
        "queue_depth" => "integer"
        "latency" => "float"
      }
    }
  }
  output {
    elasticsearch {
      hosts => ["localhost:9200"]
      index => "camera-metrics-%{+YYYY.MM.dd}"
    }
  }
  ```
end note

note right of kibana
  **Kibana Dashboards**
  • FPS Performance Trends (Line Chart)
  • Queue Depth Heatmaps (Heatmap)
  • Control System Analysis (Time Series)
  • Network Latency Monitoring (Gauge)
  • Alert Management (Watcher)

  **Useful Visualizations:**
  • Phase 10 vs Phase 11 comparison
  • PID control effectiveness
  • Adaptive control response time
end note

@enduml
```

## Network Protocol Stack Diagram

### Detailed Protocol Analysis

```plantuml
@startuml network_protocol_stack
!theme plain

title Security Camera Network Protocol Stack

rectangle "Application Layer" as app_layer #E3F2FD {
  rectangle "MJPEG Protocol v3.0" as mjpeg_protocol
  note right of mjpeg_protocol : "SYNC: 0xCAFEBABE\nMetrics: 0xCAFEBEEF\nCRC16-CCITT Validation"
}

rectangle "Presentation Layer" as pres_layer #E8F5E8 {
  rectangle "JPEG Encoding/Decoding" as encoding
  note right of encoding : "Hardware JPEG Encoder\nQuality: 50-95%\nSize: 1KB-128KB"
}

rectangle "Session Layer" as sess_layer #FFF8E1 {
  rectangle "Stream Management" as stream_mgmt
  rectangle "Connection Lifecycle" as conn_mgmt
  note right of sess_layer : "TCP Keep-alive\nReconnection Logic\nSession State"
}

rectangle "Transport Layer" as trans_layer #FCE4EC {
  rectangle "TCP Protocol" as tcp
  rectangle "Port 10080" as port
  note right of trans_layer : "Reliable Delivery\nFlow Control\nCongestion Control"
}

rectangle "Network Layer" as net_layer #F3E5F5 {
  rectangle "IPv4" as ipv4
  rectangle "192.168.11.x" as ip_range
  note right of net_layer : "Routing\nFragmentation\nDHCP Client"
}

rectangle "Data Link Layer" as link_layer #E0F2F1 {
  rectangle "802.11 WiFi" as wifi
  rectangle "MAC Layer" as mac
  note right of link_layer : "WiFi b/g/n\nMAC Addressing\nFrame Control"
}

rectangle "Physical Layer" as phy_layer #EFEBE9 {
  rectangle "2.4GHz RF" as rf
  rectangle "GS2200M Chip" as chip
  note right of phy_layer : "Radio Transmission\nAntenna\nSignal Modulation"
}

' Protocol Data Flow
app_layer -down-> pres_layer : "Protocol Data Unit\n(PDU)"
pres_layer -down-> sess_layer : "Encoded Data"
sess_layer -down-> trans_layer : "Session Data"
trans_layer -down-> net_layer : "TCP Segment"
net_layer -down-> link_layer : "IP Packet"
link_layer -down-> phy_layer : "WiFi Frame"

' Specific Protocol Details
rectangle "MJPEG Frame Structure" as mjpeg_detail #E3F2FD {
  rectangle "Header\n(16 bytes)" as header
  rectangle "JPEG Data\n(Variable)" as jpeg_data
  rectangle "CRC16\n(2 bytes)" as crc

  header -right-> jpeg_data
  jpeg_data -right-> crc
}

rectangle "Metrics Packet (58 bytes)" as metrics_detail #E8F5E8 {
  rectangle "Header\n(6 bytes)" as metrics_header
  rectangle "Metrics Data\n(50 bytes)" as metrics_data
  rectangle "CRC16\n(2 bytes)" as metrics_crc

  metrics_header -right-> metrics_data
  metrics_data -right-> metrics_crc
}

app_layer -right-> mjpeg_detail : "Video Frames"
app_layer -right-> metrics_detail : "Performance Data"

@enduml
```

これらの図は、技術特化ライブラリの具体的な使用例として、ネットワークインフラ、SysMLシステム設計、要求工学、ログ解析パイプライン、プロトコルスタック等の様々な視点からセキュリティカメラシステムを表現しています。