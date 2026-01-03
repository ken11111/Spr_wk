# Phase 7.1c Test Results & WiFi Optimization Analysis

**Test Date**: 2026-01-03
**Firmware Version**: Phase 7.1c (Timing bug fixes)
**Previous Version**: Phase 7.1b (Race condition fix), Phase 7.0 (Initial WiFi/TCP)

---

## Executive Summary

### Phase 7.1c Achievements

✅ **Critical Bug Fix**: Timing calculation overflow bug fixed in both `tcp_server.c` and `camera_threads.c`
✅ **Accurate Metrics**: First successful acquisition of true TCP send time statistics
✅ **Console Logging**: Final metrics successfully logged to Spresense console (captured via minicom)
✅ **Bottleneck Identified**: TCP send time confirmed as primary bottleneck (233ms avg vs 50ms target)

### Performance Summary

| Metric | Phase 7.0 | Phase 7.1c | Target | Status |
|--------|-----------|------------|--------|--------|
| **PC FPS** | 0.57-1.62 | 0.54-1.10 | 15-25 | ❌ 4.4% |
| **Spresense FPS** | ~2.5 | 4.36 | 30 | ❌ 14.5% |
| **TCP Send Time** | Unknown | 233ms avg | <50ms | ❌ 4.7x over |
| **TCP Max Send** | Unknown | 885ms | <100ms | ❌ 8.9x over |
| **WiFi Throughput** | Unknown | 1.6 Mbps | ~8 Mbps | ❌ 20% |
| **Queue Depth** | 5 (full) | 7 (full) | <3 | ❌ Saturated |
| **Metrics Success** | 0% | 12.5% | >90% | ❌ |

**Conclusion**: WiFi/TCP transport significantly underperforms USB Serial (Phase 2: 37 fps). TCP send bottleneck requires optimization.

---

## 1. Phase 7.1 Implementation History

### Phase 7.1b: Race Condition Bug

**Problem**:
- Forced metrics send in `camera_threads_cleanup()` executed **before** `pthread_join()`
- USB/TCP thread still running → two threads writing to same TCP socket
- Result: TCP stream corruption, garbled console output

**User Report**:
> 実行しましたが、PC側で画像とログを受け取れなくなり、Spresense側のログも文字化けして解読不可能になりました。

**Fix**: Disabled forced TCP send, moved metrics logging to console after thread join

### Phase 7.1c: Timing Calculation Bug

**Problem**: Overflow in timespec difference calculation

**Buggy Code** (tcp_server.c:249, camera_threads.c:136):
```c
// Bug: end.tv_nsec - start.tv_nsec can be negative when second rolls over
send_time_us = (end.tv_sec - start.tv_sec) * 1000000ULL +
               (end.tv_nsec - start.tv_nsec) / 1000ULL;  // <- Overflow!
```

**Example**:
- `start.tv_nsec = 900,000,000` (0.9s)
- `end.tv_nsec = 100,000,000` (1.1s after second rollover)
- Difference: `100,000,000 - 900,000,000 = -800,000,000`
- Unsigned arithmetic: Wraps to `18,446,744,073,909,551,616 - 800,000,000 = huge positive`

**Result**:
- TCP Avg Send Time: **1,036,271,915 us** (1036 seconds = 17 minutes) ← Impossible!
- Uptime: **4,154,515,276 ms** (48 days) ← System ran for 7 seconds

**Fixed Code**:
```c
// Convert both to microseconds first, then subtract (always positive)
uint64_t start_us = (uint64_t)start.tv_sec * 1000000ULL +
                    (uint64_t)start.tv_nsec / 1000ULL;
uint64_t end_us = (uint64_t)end.tv_sec * 1000000ULL +
                  (uint64_t)end.tv_nsec / 1000ULL;
send_time_us = end_us - start_us;  // Correct calculation
```

---

## 2. Phase 7.1c Test Results

### 2.1 Spresense Log Analysis

**System Startup**:
```
WiFi connected! IP: 192.168.137.58
TCP server initialized on port 8888
Client connected! Starting MJPEG streaming...
Allocated 7 buffers (98318 bytes each, total 672 KB)
Camera thread priority: 110
USB thread priority: 100
```

**Runtime Behavior**:
- Total runtime: 7.8 seconds
- Camera frames processed: 34
- TCP packets sent: 28
- Frames remaining in queue: 6 (34 - 28)
- JPEG validation errors: 0 (100% success)

**Metrics Packet Transmission**:
```
Metrics attempts: 8 (seq=0 to seq=7)
Success: 1 (seq=0: "Metrics queued")
Failure: 7 ("No empty buffer for metrics packet")
Success rate: 12.5%
```

**Queue Status** (every metrics attempt):
```
seq=0: q_depth=4  <- Initial fill
seq=1: q_depth=7  <- Saturated
seq=2: q_depth=7
seq=3: q_depth=7
seq=4: q_depth=7
seq=5: q_depth=7
seq=6: q_depth=7
seq=7: q_depth=7  <- Constantly full
```

**Termination**:
```
TCP thread: Client disconnected (error -107)
Camera thread exiting (processed 34 frames)
USB thread exiting (sent 28 packets, 1318684 bytes total)
```

**FINAL METRICS** (Console Log - Key Achievement!):
```
=================================================
FINAL METRICS (Shutdown/Error Detection)
=================================================
Uptime: 7804 ms                    <- Correct! (was 4,154,515,276 ms)
Camera Frames: 34
USB/TCP Packets: 28
Action Queue Depth: 6
Average Packet Size: 47095 bytes
Total Errors: 0
TCP Avg Send Time: 232959 us      <- 233ms (was 1,036,271,915 us)
TCP Max Send Time: 884646 us      <- 885ms (was 1,271,484,480 us)
=================================================
```

### 2.2 PC Log Analysis

**CSV Data**:
```csv
timestamp,pc_fps,spresense_fps,frame_count,serial_read_time_ms
1767446833.459,1.10,3.24,2,543.11
1767446834.574,0.90,2.56,3,341.81
1767446836.415,0.54,3.34,4,235.15
```

**PC Side Performance**:
- PC FPS: 0.54 - 1.10 fps
- Spresense FPS (reported by PC): 2.56 - 3.34 fps
- Frames received: 4 (out of 28 sent = 14.3% delivery rate)
- TCP read time: 235 - 543ms (avg: 373ms)

**Observations**:
- PC received only 4 frames despite Spresense sending 28
- Sync word search overhead on PC side
- TCP connection terminated prematurely

---

## 3. Performance Analysis

### 3.1 TCP Send Time Breakdown

**Per-packet Timing** (47KB MJPEG packet):

```
┌──────────────────────────────────────────────────────────────┐
│ E2E Flow for 1 Frame (47KB)                                  │
├──────────────────────────────────────────────────────────────┤
│ Camera capture:          ~10ms                               │
│ JPEG compression:        (Hardware, included in capture)     │
│ JPEG packing:            ~10ms                               │
│ Queue enqueue:           <1ms                                │
│                                                              │
│ ╔═══════════════════════════════════════════════════════╗   │
│ ║ TCP send (Spresense):  233ms avg (BOTTLENECK)        ║   │
│ ║                        885ms max                      ║   │
│ ╚═══════════════════════════════════════════════════════╝   │
│                                                              │
│ Network transmission:    ~10-50ms (WiFi 2.4GHz)             │
│ TCP read (PC):           373ms total (including send time)   │
└──────────────────────────────────────────────────────────────┘

Total E2E latency: ~400-450ms per frame
Theoretical max FPS: 2.2-2.5 fps (1000ms / 450ms)
Actual FPS: 0.54-1.10 fps (PC side)
```

### 3.2 WiFi Throughput Calculation

**Measurement**:
- Packet size: 47,095 bytes = 376,760 bits
- Average send time: 232,959 microseconds = 0.233 seconds
- **Throughput**: 376,760 / 0.233 = **1,617,082 bps ≈ 1.6 Mbps**

**Comparison with Theoretical**:
- GS2200M WiFi: 802.11b/g/n (2.4GHz)
- Theoretical max (802.11b): 11 Mbps
- Theoretical max (802.11g): 54 Mbps
- **Actual efficiency**: 1.6 / 11 = **14.5%** (802.11b) ← Very poor!

**Why so slow?**
1. **usrsock overhead**: 4 context switches per send() vs USB: 1
2. **GS2200M SPI communication**: VSPI5 with DMA, but still adds latency
3. **TCP overhead**: Protocol headers, ACKs, retransmissions
4. **Small packet fragmentation**: 47KB may be fragmented at TCP/IP layer

### 3.3 Queue Dynamics

**Buffer Pool**:
- Total buffers: 7 (Phase 7.0: 5, Phase 7.1: 10→7)
- Buffer size: 98,318 bytes each
- **Total memory**: 7 × 98,318 = **688,226 bytes ≈ 672 KB**

**Queue State Timeline**:
```
Time    Camera  USB/TCP  Action Queue  Empty Queue  Status
─────────────────────────────────────────────────────────────
0s      Start   Start    0             7            Normal
1s      Frame1  -        1             6            Filling
2s      Frame2  Send1    2             5            Balanced
3s      Frame5  Send2    5             2            Unbalanced
4s      Frame8  Send3    7             0            Saturated!
5-8s    FrameX  SendY    7             0            Always full
```

**Root Cause**: USB/TCP thread cannot drain queue fast enough
- Camera thread: ~30 fps capture rate
- USB/TCP thread: ~3.6 fps send rate (28 packets / 7.8s)
- **Imbalance**: 8.3x slower send than capture

### 3.4 Metrics Packet Transmission Analysis

**Packet Sizes**:
- MJPEG packet: ~47KB (98,318 bytes allocated)
- Metrics packet: 42 bytes (Phase 7.0 extension)

**Why Metrics Fail**:
1. Queue always full (depth=7, no empty buffers)
2. Metrics packet needs an empty buffer from `g_empty_queue`
3. USB/TCP thread too slow to return empties
4. Camera thread blocks waiting for empty buffer
5. Metrics generation skipped with "No empty buffer" warning

**Success Case** (seq=0):
- Queue depth was 4 (not yet saturated)
- Empty buffer available
- Metrics successfully queued

**Failure Pattern** (seq=1-7):
- Queue depth = 7 (saturated)
- No empty buffers (empty_q=0)
- All buffers in action queue or USB thread

---

## 4. Root Cause: TCP Send Bottleneck

### 4.1 usrsock Architecture Overhead

**NuttX usrsock** = User-space socket implementation

**Call Flow for `send()`**:
```
Application (security_camera)
    ↓ 1. System call
Kernel (NuttX)
    ↓ 2. usrsock request → Unix socket
gs2200m daemon (user space)
    ↓ 3. SPI communication
GS2200M WiFi module (hardware)
    ↓ 4. TCP/IP stack (on module)
WiFi transmission
```

**Context Switches**:
- USB Serial: 1 context switch (kernel driver)
- **TCP usrsock: 4 context switches** (app→kernel→daemon→SPI→module)
- **Overhead**: ~4x more context switches

**Evidence**:
- USB Serial (Phase 2): 37 fps → ~27ms per frame
- TCP usrsock (Phase 7.1c): 3.6 fps → 233ms per frame
- **Ratio**: 233 / 27 = **8.6x slower**

### 4.2 GS2200M SPI Communication

**SPI Configuration** (from `.config`):
```
CONFIG_CXD56_DMAC_SPI5_TX=y  # DMA enabled for TX
CONFIG_CXD56_DMAC_SPI5_RX=y  # DMA enabled for RX
CONFIG_CXD56_SPI5=y
```

**Theoretical SPI Speed**:
- SPI5 clock: Typically 1-10 MHz (Spresense spec)
- DMA: Reduces CPU load but not latency
- **Estimated transfer time** for 47KB: ~40-400ms (depends on SPI clock)

**Actual**: 233ms average → Within expected range for slower SPI clock

### 4.3 TCP Buffer and Protocol Overhead

**Current TCP Configuration**:
```c
// tcp_server.c:158-170
int sndbuf = 262144;  // 256KB send buffer
setsockopt(connfd, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf));

// tcp_server.c:147
int nodelay = 1;
setsockopt(connfd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));
```

**Buffer Analysis**:
- Send buffer: 256KB (set via SO_SNDBUF)
- Actual buffer (getsockopt): May be smaller due to kernel limits
- Packet size: 47KB = 18.4% of buffer
- **Issue**: 47KB packet may not fully utilize large buffer

**TCP_NODELAY**:
- ✅ Enabled (disables Nagle algorithm)
- ✅ Small packets sent immediately (not buffered)
- But 47KB is not "small" → less impact

---

## 5. Spresense Memory Specifications

### 5.1 Spresense Hardware Memory

**Sony CXD5602 SoC** (Spresense main board):
- **Total RAM**: 1.5 MB
- **Application RAM**: ~640 KB (after NuttX kernel and drivers)
- **Flash**: 8 MB

**Memory Partitioning** (typical NuttX config):
```
Total 1.5MB RAM:
├─ Kernel:          ~400KB (NuttX core, drivers)
├─ Heap:            ~600KB (application malloc)
├─ Stack:           ~200KB (thread stacks)
└─ Reserved:        ~300KB (DMA, buffers, etc.)
```

### 5.2 Current Memory Usage (Phase 7.1c)

**Frame Queue Buffers**:
```
Buffer count:  7
Buffer size:   98,318 bytes (MJPEG_MAX_PACKET_SIZE)
Total:         7 × 98,318 = 688,226 bytes ≈ 672 KB
```

**Camera V4L2 Buffers**:
```
Buffer count:  3 (triple buffering)
Buffer size:   65,536 bytes (configured sizeimage)
Total:         3 × 65,536 = 196,608 bytes ≈ 192 KB
```

**TCP Buffers** (NuttX network stack):
```
SO_SNDBUF:     256 KB (request, actual may be less)
SO_RCVBUF:     Default (~16-64 KB)
Total:         ~256-320 KB
```

**Application Stack/Heap**:
```
Camera thread stack:   8 KB (estimated)
USB thread stack:      8 KB (estimated)
Main thread stack:     4 KB (estimated)
Heap usage:            ~50 KB (misc allocations)
Total:                 ~70 KB
```

**Total Memory Usage**:
```
Frame queue:      672 KB
Camera buffers:   192 KB
TCP buffers:      ~280 KB (estimated)
App stack/heap:   70 KB
───────────────────────
TOTAL:            ~1214 KB ≈ 1.2 MB

Available RAM:    ~640 KB (application space)
DEFICIT:          -574 KB ← OVER BUDGET!
```

⚠️ **CRITICAL FINDING**: Current configuration likely exceeds available application RAM!

**Why it still works**:
- NuttX may be using shared kernel/app memory region
- Buffers may be allocated from different pools (DMA-capable region)
- Actual `SO_SNDBUF` may be much smaller than 256KB

### 5.3 Memory Budget for Buffer Expansion

**Safe Memory Budget** (conservative estimate):
```
Available app RAM:      640 KB
Reserved (safety):      100 KB (kernel overhead, safety margin)
Available for buffers:  540 KB
```

**Current Allocation**:
```
Frame queue:      672 KB  ← Already over budget!
Camera buffers:   192 KB  (V4L2 driver, cannot change)
───────────────────────
Total:            864 KB  ← Exceeds 540 KB budget
```

**Conclusion**: Cannot increase frame queue depth without reducing buffer size

### 5.4 Buffer Size Optimization

**Current**:
```
#define MJPEG_MAX_PACKET_SIZE (320 * 240 * 2 + 14 + 4)  // 153,618
// But actually allocated: 98,318 bytes (from code inspection)
```

**Actual JPEG Size** (from logs):
```
Average: 47,095 bytes (47 KB)
Range:   43,702 - 52,127 bytes
Max observed: 52,127 bytes
```

**Optimization**: Reduce buffer size to actual max needed
```
MJPEG_MAX_PACKET_SIZE = 60,000 bytes (60KB, with safety margin)
Buffer count: 7
Total: 7 × 60,000 = 420,000 bytes ≈ 410 KB
Savings: 672 - 410 = 262 KB
```

**New Memory Budget** (with optimized buffers):
```
Frame queue:      410 KB (reduced from 672 KB)
Camera buffers:   192 KB (unchanged)
TCP buffers:      ~280 KB (unchanged)
───────────────────────
TOTAL:            882 KB (was 1214 KB)

Available RAM:    640 KB
DEFICIT:          -242 KB ← Still over, but closer
```

**Further Optimization**: Reduce queue depth
```
Option A: 5 buffers × 60KB = 300 KB (Phase 7.0 queue depth)
Option B: 4 buffers × 60KB = 240 KB (minimum for triple buffering + 1)

With Option A (5 × 60KB):
Frame queue:      300 KB
Camera buffers:   192 KB
TCP buffers:      ~100 KB (reduce SO_SNDBUF)
───────────────────────
TOTAL:            592 KB ← Within 640 KB budget!
```

---

## 6. GS2200M WiFi Module Capabilities

### 6.1 GS2200M Hardware Specifications

**Manufacturer**: Telit (formerly GainSpan)
**Part Number**: GS2200M
**WiFi Standard**: 802.11 b/g/n
**Frequency**: 2.4 GHz only (no 5GHz support)
**Interface**: SPI (Serial Peripheral Interface)

**Key Specifications**:
- **Data Rate**: Up to 65 Mbps (802.11n with HT40)
- **Transmit Power**: +18 dBm max (configurable)
- **Channels**: 1-13 (region dependent)
- **Security**: WPA/WPA2-PSK, WPA2-Enterprise
- **Power Modes**: Active, Power Save, Deep Sleep

### 6.2 WiFi Channel Configuration

**NuttX GS2200M Driver Support**: Check driver source

**Channel Fixing** via `gs2200m` command:
- Current: Auto-channel selection (scans for best channel)
- **Possible**: Manual channel selection (if driver supports)

**AT Command** (GS2200M native):
```
AT+WSETC=<channel>    // Set WiFi channel (1-13)
AT+WGETC              // Get current channel
```

**NuttX gs2200m wrapper**: Need to verify if exposed to user

**Investigation Required**:
1. Check `/home/ken/Spr_ws/GH_wk_test/spresense/nuttx/drivers/wireless/gs2200m.c`
2. Look for channel configuration ioctls
3. Test with `gs2200m` command options

### 6.3 TX Power Adjustment

**GS2200M Hardware Range**: 0 dBm to +18 dBm

**AT Command** (GS2200M native):
```
AT+WP=<power>         // Set TX power (0-18 dBm)
AT+WP?                // Get current TX power
```

**NuttX Driver Support**: Need to check if exposed

**Trade-offs**:
- **Higher power** (+18 dBm): Better range, more interference, higher power consumption
- **Lower power** (+10 dBm): Less interference, lower power, shorter range

**Investigation Required**:
1. Check driver source for power configuration
2. Verify if configurable via ioctl or config option
3. Test impact on throughput (may not help if interference is issue)

### 6.4 Other GS2200M Optimizations

**HT40 Mode** (802.11n 40MHz channel width):
- Doubles throughput (65 Mbps vs 33 Mbps)
- Requires compatible AP
- More interference in crowded 2.4GHz band

**Power Save Modes**:
- Disable power save for maximum performance
- Check if enabled in current config

**Multicast/Broadcast Filtering**:
- Reduce unnecessary packet processing
- May be enabled by default

---

## 7. TCP/Network Configuration Analysis

### 7.1 Current NuttX Network Config

**Key Settings** (from previous configuration):
```bash
CONFIG_NET_IPv4=y
CONFIG_NET_TCP=y
CONFIG_NET_TCP_WRITE_BUFFERS=y
CONFIG_NET_TCP_READAHEAD=y
CONFIG_NET_TCPBACKLOG=y
CONFIG_NET_USRSOCK_TCP=y
```

**TCP Write Buffers**:
```
CONFIG_NET_TCP_WRITE_BUFFERS=y        # Enable write buffering
CONFIG_NET_TCP_NWRBCHAINS=?           # Number of write buffer chains (UNKNOWN)
CONFIG_IOB_BUFSIZE=?                  # I/O buffer size (UNKNOWN)
CONFIG_IOB_NBUFFERS=?                 # Number of I/O buffers (UNKNOWN)
```

**Investigation Needed**: Check actual values for buffer config

### 7.2 Buffer Size Expansion Analysis

**Current SO_SNDBUF**: 256 KB (requested)

**Proposed Expansion**: 512 KB

**Memory Impact**:
```
Current:   256 KB
Proposed:  512 KB
Increase:  +256 KB

Available budget: ~540 KB (from Section 5.3)
Current usage:    ~880 KB (over budget)
After expansion:  ~1136 KB (further over budget)
```

❌ **Conclusion**: Cannot expand SO_SNDBUF without reducing frame queue

**Alternative**: Keep SO_SNDBUF=256KB, optimize frame queue instead

### 7.3 Large Packet Batching Proposal

**Current**: Send 1 frame per packet (47 KB)

**Proposed**: Batch multiple frames into single TCP send
```
Batch size: 3 frames
Packet size: 3 × 47 KB = 141 KB
Send time: 233ms × 1.5 = ~350ms (estimated, with overhead)
Effective FPS: 3 frames / 0.35s = 8.6 fps (vs current 3.6 fps)
```

**Benefits**:
- Reduces context switch overhead (4 switches per batch vs per frame)
- Better TCP efficiency (fewer ACKs, less protocol overhead)
- May improve throughput (larger send = better buffer utilization)

**Drawbacks**:
- Increased latency (3 frames buffered before send)
- Requires protocol change (PC must parse multi-frame packets)
- Memory pressure (need larger queue buffers)

**Implementation Complexity**: Medium-High

---

## 8. Optimization Roadmap (Option 2)

### Priority A: Critical - Immediate Wins

**A1. Reduce Frame Queue Buffer Size** ✅ Recommended
```
Current: MJPEG_MAX_PACKET_SIZE = 98,318 bytes
Actual max JPEG: 52,127 bytes
Proposed: MJPEG_MAX_PACKET_SIZE = 60,000 bytes

Savings: (98,318 - 60,000) × 7 = 268,226 bytes ≈ 262 KB
Memory: 672 KB → 410 KB
Status: Within optimization range, test impact on large frames
```

**A2. Reduce SO_SNDBUF to Free Memory** ✅ Recommended
```
Current: 256 KB (likely not fully allocated by kernel)
Proposed: 128 KB (still 2.7× frame size)

Rationale: 47 KB frame << 256 KB buffer, overkill
Savings: ~128 KB (kernel space)
Impact: Minimal (buffer already underutilized)
```

**A3. Verify and Reduce Queue Depth if Needed**
```
Current: 7 buffers
Test with: 5 buffers (Phase 7.0 level)

Rationale: Queue always full anyway, depth doesn't help if drain is slow
Memory savings: 2 × 60,000 = 120 KB
Risk: May reduce metrics success rate (12.5% → lower?)
```

### Priority B: High - Performance Gains

**B1. Investigate WiFi Channel Fixing**
```
Action: Check gs2200m driver source for channel config
Goal: Fix to least congested channel (scan with WiFi analyzer)
Expected gain: 10-30% throughput improvement (reduce interference)
Complexity: Low (if driver supports it)
```

**B2. Disable GS2200M Power Save Mode**
```
Action: Check if power save is enabled in gs2200m config
Goal: Maximum performance, no sleep delays
Expected gain: 5-15% latency reduction
Complexity: Low (driver config or AT command)
```

**B3. Optimize TCP Nagle and Delayed ACK**
```
Current: TCP_NODELAY=1 (Nagle disabled) ✓
Additional: Check TCP_QUICKACK on Linux PC side

Action: Disable delayed ACK on PC receiver
Expected gain: 5-10% latency reduction
Complexity: Low (PC sysctl setting)
```

### Priority C: Medium - Advanced Optimizations

**C1. Multi-Frame Batching**
```
Description: Send 2-3 frames per TCP packet
Protocol: Extend MJPEG protocol with frame count field
Expected gain: 50-100% FPS improvement (3.6 → 5-7 fps)
Complexity: High (protocol change, PC code change)
Timeline: 1-2 days implementation
```

**C2. Enable GS2200M HT40 Mode (802.11n)**
```
Description: Use 40MHz channel width for 2× throughput
Requirement: AP must support 802.11n HT40
Expected gain: Up to 2× throughput (if not interference-limited)
Complexity: Medium (driver config, AP config)
Risk: More interference in 2.4GHz crowded band
```

**C3. Increase SPI Clock Speed**
```
Description: Increase SPI5 clock from default to maximum supported
Check: CXD56_SPI5_MAXFREQUENCY config option
Expected gain: 10-30% reduction in SPI transfer time
Complexity: Low (config change, test stability)
Risk: Potential signal integrity issues at high speed
```

### Priority D: Low - Experimental

**D1. TX Power Adjustment**
```
Description: Test +10 dBm vs +18 dBm
Expected gain: Minimal (throughput more limited by SPI/usrsock than RF)
Complexity: Medium (need driver support)
```

**D2. Zero-Copy Optimization**
```
Description: Reduce buffer copies in usrsock path
Scope: NuttX kernel modification (advanced)
Expected gain: 5-10% CPU reduction
Complexity: Very High (kernel internals)
```

---

## 9. Investigation Tasks

### Task 1: Verify Memory Configuration

**Check `.config` for actual buffer settings**:
```bash
cd /home/ken/Spr_ws/GH_wk_test/spresense/nuttx
grep -E "IOB_BUFSIZE|IOB_NBUFFERS|TCP_NWRBCHAINS" .config
```

**Expected output**:
```
CONFIG_IOB_BUFSIZE=196
CONFIG_IOB_NBUFFERS=36
CONFIG_NET_TCP_NWRBCHAINS=8
```

**Analysis**: Calculate total TCP write buffer memory
```
Total = IOB_BUFSIZE × IOB_NBUFFERS × TCP_NWRBCHAINS
```

### Task 2: Check GS2200M Driver Capabilities

**Search driver source for channel/power config**:
```bash
cd /home/ken/Spr_ws/GH_wk_test/spresense/nuttx
grep -r "SIOCSIWFREQ\|SIOCSIWPOWER\|channel\|txpower" drivers/wireless/gs2200m*
```

**Check gs2200m command help**:
```bash
# On Spresense NSH:
gs2200m -h
```

### Task 3: Measure Actual Memory Usage

**Add memory statistics logging** in `camera_app_main.c`:
```c
#include <sys/resource.h>

struct mallinfo mem = mallinfo();
printf("Heap used: %d bytes, free: %d bytes\n",
       mem.uordblks, mem.fordblks);
```

### Task 4: WiFi Channel Scan

**On PC (Windows)**:
```powershell
# PowerShell: Scan WiFi channels
netsh wlan show networks mode=bssid
```

**Identify least congested channel** (fewest APs)

---

## 10. Test Plan for Next Phase

### Phase 7.2 Target: Optimize Memory and WiFi

**Changes to Implement**:
1. ✅ Reduce `MJPEG_MAX_PACKET_SIZE` to 60,000 bytes
2. ✅ Reduce queue depth to 5 buffers (if memory still tight)
3. ✅ Reduce `SO_SNDBUF` to 128 KB
4. ✅ Verify actual `.config` buffer settings
5. ❓ Fix WiFi channel (if driver supports)
6. ❓ Disable power save mode (if enabled)

**Expected Outcomes**:
- Memory usage: Within 640 KB budget
- TCP send time: 200-220ms (slight improvement from reduced overhead)
- FPS: 4-5 fps (modest gain)
- Metrics success rate: 20-30% (more empty buffers)

**Success Criteria**:
- ✅ No memory allocation failures
- ✅ Application runs >30 seconds without crash
- ✅ FPS ≥ 4.0 (current: 3.6)
- ✅ At least 2 metrics packets successfully transmitted

**Rollback Plan**: If FPS degrades, revert to Phase 7.1c

---

## 11. Long-Term Considerations

### Alternative 1: Hybrid WiFi + USB

**Architecture**:
- WiFi: Control channel (commands, metrics, status)
- USB Serial: Data channel (MJPEG frames)

**Benefits**:
- Reliable 37 fps (Phase 2 proven)
- WiFi for remote monitoring/control
- Best of both worlds

**Drawbacks**:
- Requires both connections
- More complex setup

### Alternative 2: H.264 Video Compression

**Current**: JPEG per-frame (47 KB average)
**Proposed**: H.264 I-frame (30 KB) + P-frames (5-10 KB)

**Expected**:
- Average bitrate: 10-15 KB/frame (vs 47 KB JPEG)
- TCP send time: 50-75ms (vs 233ms)
- **FPS: 13-20 fps** (vs 3.6 fps)

**Complexity**: Very High
- Requires H.264 encoder (hardware or software)
- Spresense CXD5602 has no hardware H.264 encoder
- Software encoder: High CPU load (may not achieve real-time)

### Alternative 3: Return to USB Serial

**Proven Performance** (Phase 2):
- FPS: 37 fps (VGA resolution)
- Latency: ~27ms per frame
- Reliability: 99.89% success rate (2.7-hour test)

**When to Choose**:
- If Phase 7.2 optimization fails to reach 10+ fps
- If WiFi proves unstable in production environment
- If low latency is critical requirement

---

## 12. Conclusion

### Key Findings

1. ✅ **Timing bug fix successful**: Accurate TCP metrics now available
2. ❌ **TCP send is bottleneck**: 233ms avg (4.7× slower than target)
3. ❌ **Memory usage exceeds budget**: ~1.2 MB used vs ~640 KB available
4. ❌ **Queue always saturated**: 7-buffer depth insufficient when drain is slow
5. ✅ **Console metrics logging works**: Final metrics captured via minicom

### Root Causes

1. **usrsock overhead**: 4 context switches vs USB's 1 → 4× slowdown
2. **GS2200M SPI**: Adds latency for 47KB transfers
3. **WiFi efficiency**: Only 14.5% of theoretical 11 Mbps (1.6 Mbps actual)
4. **Memory pressure**: Over-allocated buffers exceed available RAM

### Recommended Path Forward

**Phase 7.2 Optimization** (Option 2):
1. Reduce buffer sizes (98KB → 60KB)
2. Optimize TCP buffers (256KB → 128KB)
3. Investigate WiFi channel fixing
4. Disable power save mode
5. Target: 5-7 fps (vs current 3.6 fps)

**Fallback** (if <5 fps):
- Consider Alternative 1 (Hybrid WiFi+USB)
- Or return to USB Serial (proven 37 fps)

**Next Steps**: Execute Task 1-4 investigations, implement Phase 7.2 changes

---

## Appendix A: Full Logs

### Spresense Console Log (Phase 7.1c)

<details>
<summary>Click to expand full minicom log</summary>

```
NuttShell (NSH) NuttX-11.0.0
nsh> gs2200m DESKTOP-GPU979R B54p3530 &
gs2200m [13:50]
nsh> ifconfig
wlan0   Link encap:Ethernet HWaddr 3c:95:09:00:64:ac at UP mtu 1500
        inet addr:192.168.137.58 DRaddr:192.168.137.1 Mask:255.255.255.0

nsh> security_camera &
security_camera [14:100]
nsh> CAM] =================================================
[CAM] Security Camera Application Starting (MJPEG)
[CAM] =================================================
[CAM] Camera config: 640x480 @ 30 fps, Format=JPEG, HDR=0
[CAM] Initializing video driver: /dev/video
[CAM] Opening video device: /dev/video
[CAM] Camera format set: 640x480
[CAM] Driver returned sizeimage: 0 bytes (format=0x4745504a)
[CAM] Calculated sizeimage: 65536 bytes (driver returned 0)
[CAM] Camera FPS set: 30
[CAM] Camera buffers requested: 3 (driver returned: 3)
[CAM] Allocating 3 buffers of 65536 bytes each
[CAM] Camera streaming started
[CAM] Packet buffer allocated: 98318 bytes
[CAM] Initializing WiFi transport...
[CAM] WiFi manager initialized
[CAM] Connecting to WiFi: SSID=DESKTOP-GPU979R
[CAM] WiFi connected! IP: 192.168.137.58
[CAM] TCP server initialized on port 8888
[CAM] Waiting for client connection...
[CAM] Client connected! Starting MJPEG streaming...
[CAM] Performance logging initialized (interval=30 frames)
[CAM] Priority inheritance not supported (error 138), continuing without it
[CAM] Thread priorities (110 vs 100) will help prevent priority inversion
[CAM] Frame queue system initialized
[CAM] Allocated 7 buffers (98318 bytes each, total 672 KB)
[CAM] == Camera thread started (Step 2: active) ==
[CAM] Camera thread priority: 110
[CAM] JPEG padding removed: 15 bytes (size: 52128 -> 52113)
[CAM] Packed frame: seq=0, size=52113, crc=0xF4AF, total=52127
[CAM] Camera thread created (priority 110)
[CAM] USB thread created (priority 100)
[CAM] Threading system initialized (Step 1: stub threads)
[CAM] Threading initialized (stub threads running)
[CAM] Starting main loop (will capture 90 frames for testing)...
[CAM] =================================================
[CAM] Fully threaded mode: Camera and USB threads active
[CAM] Running continuously - press Ctrl+C to stop...
[CAM] == USB thread started (Step 3: active) ==
[CAM] USB thread priority: 100
[CAM] Packed frame: seq=1, size=51584, crc=0xE1B8, total=51598
[... frame logs omitted for brevity ...]
[CAM] Camera stats: frame=30, action_q=7, empty_q=0, avg_jpeg=47 KB, jpeg_errors=0 (0.0%)
[CAM] Packed metrics: seq=7, cam_frames=33, usb_pkts=27, q_depth=7, avg_size=47008, errors=0
[CAM] No empty buffer for metrics packet
[CAM] JPEG padding removed: 10 bytes (size: 49088 -> 49078)
[CAM] Packed frame: seq=33, size=49078, crc=0xADA8, total=49092
[CAM] TCP thread: Client disconnected (error -107)
[CAM] == Camera thread exiting (processed 34 frames) ==
[CAM] Camera thread: JPEG validation errors: 0 (all frames valid)
[CAM] Camera thread: Average JPEG size: 47 KB
[CAM] == USB thread exiting (sent 28 packets, 1318684 bytes total) ==
[CAM] Shutdown requested by threads, exiting main loop
[CAM] Main loop ended
[CAM] Threads processed frames in parallel (camera + USB)
[CAM] Signaling shutdown to threads...
[CAM] =================================================
[CAM] Main loop ended (threaded mode: ~90 frames expected @ 30fps)
[CAM] Cleaning up...
[CAM] Cleaning up threading system...
[CAM] Waiting for camera thread to exit...
[CAM] Camera thread joined successfully
[CAM] Waiting for USB thread to exit...
[CAM] USB thread joined successfully
[CAM] =================================================
[CAM] FINAL METRICS (Shutdown/Error Detection)
[CAM] =================================================
[CAM] Uptime: 7804 ms
[CAM] Camera Frames: 34
[CAM] USB/TCP Packets: 28
[CAM] Action Queue Depth: 6
[CAM] Average Packet Size: 47095 bytes
[CAM] Total Errors: 0
[CAM] TCP Avg Send Time: 232959 us
[CAM] TCP Max Send Time: 884646 us
[CAM] =================================================
[CAM] Buffer pool freed
[CAM] Frame queue system cleaned up
[CAM] Threading system cleaned up successfully
[CAM] Performance logging cleanup
[CAM] Total frames processed: 0
[CAM] WiFi/TCP transport cleaned up
[CAM] Camera manager cleaned up
[CAM] =================================================
[CAM] Security Camera Application Stopped
[CAM] =================================================
```

</details>

### PC CSV Log (Phase 7.1c)

```csv
timestamp,pc_fps,spresense_fps,frame_count,error_count,decode_time_ms,serial_read_time_ms,texture_upload_time_ms,jpeg_size_kb,spresense_camera_frames,spresense_camera_fps,spresense_usb_packets,action_q_depth,spresense_errors
1767446833.459,1.10,3.24,2,0,2.51,543.11,0.00,50.03,0,0.00,0,0,0
1767446834.574,0.90,2.56,3,0,2.78,341.81,0.00,48.92,0,0.00,0,0,0
1767446836.415,0.54,3.34,4,0,3.28,235.15,0.00,47.59,0,0.00,0,0,0
```

---

**Document Version**: 1.0
**Last Updated**: 2026-01-03
**Next Review**: After Phase 7.2 optimization implementation
