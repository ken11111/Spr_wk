/****************************************************************************
 * security_camera/config.h
 *
 *   Copyright 2025 Security Camera Project
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

#ifndef __SECURITY_CAMERA_CONFIG_H
#define __SECURITY_CAMERA_CONFIG_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <nuttx/video/video.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Camera Configuration */

#ifdef CONFIG_EXAMPLES_SECURITY_CAMERA_CAMERA_WIDTH
#  define CONFIG_CAMERA_WIDTH          CONFIG_EXAMPLES_SECURITY_CAMERA_CAMERA_WIDTH
#else
#  define CONFIG_CAMERA_WIDTH          640  /* VGA (Phase 1.5) */
#endif

#ifdef CONFIG_EXAMPLES_SECURITY_CAMERA_CAMERA_HEIGHT
#  define CONFIG_CAMERA_HEIGHT         CONFIG_EXAMPLES_SECURITY_CAMERA_CAMERA_HEIGHT
#else
#  define CONFIG_CAMERA_HEIGHT         480  /* VGA (Phase 1.5) */
#endif

#ifdef CONFIG_EXAMPLES_SECURITY_CAMERA_FPS
#  define CONFIG_CAMERA_FPS            CONFIG_EXAMPLES_SECURITY_CAMERA_FPS
#else
#  define CONFIG_CAMERA_FPS            30
#endif

#define CONFIG_CAMERA_FORMAT         V4L2_PIX_FMT_JPEG  /* JPEG using camera's built-in encoder */

#ifdef CONFIG_EXAMPLES_SECURITY_CAMERA_HDR_ENABLE
#  define CONFIG_CAMERA_HDR_ENABLE     true
#else
#  define CONFIG_CAMERA_HDR_ENABLE     false
#endif

/* Encoder Configuration */

#define CONFIG_ENCODER_CODEC         VIDEO_CODEC_TYPE_H264

#ifdef CONFIG_EXAMPLES_SECURITY_CAMERA_BITRATE
#  define CONFIG_ENCODER_BITRATE       CONFIG_EXAMPLES_SECURITY_CAMERA_BITRATE
#else
#  define CONFIG_ENCODER_BITRATE       2000000  /* 2 Mbps */
#endif

#define CONFIG_ENCODER_GOP_SIZE      30
#define CONFIG_ENCODER_PROFILE       0  /* H.264 Baseline profile */

/* Protocol Configuration */

#define CONFIG_PACKET_MAGIC          0x5350  /* 'SP' */
#define CONFIG_PACKET_VERSION        0x01
#define CONFIG_MAX_PAYLOAD_SIZE      4096

/* USB Configuration */

#define CONFIG_USB_DEVICE_PATH       "/dev/ttyACM0"
#define CONFIG_USB_TX_BUFFER_COUNT   4
#define CONFIG_USB_TX_BUFFER_SIZE    8192   /* 8KB - Optimal (confirmed by testing) */
#define CONFIG_USB_WRITE_TIMEOUT_MS  1000

/* Application Configuration */

#ifdef CONFIG_EXAMPLES_SECURITY_CAMERA_PRIORITY
#  define CONFIG_APP_PRIORITY          CONFIG_EXAMPLES_SECURITY_CAMERA_PRIORITY
#else
#  define CONFIG_APP_PRIORITY          100
#endif

#ifdef CONFIG_EXAMPLES_SECURITY_CAMERA_STACKSIZE
#  define CONFIG_APP_STACK_SIZE        CONFIG_EXAMPLES_SECURITY_CAMERA_STACKSIZE
#else
#  define CONFIG_APP_STACK_SIZE        (8 * 1024)  /* 8KB */
#endif

#define CONFIG_MAX_RECONNECT_RETRY   3
#define CONFIG_RECONNECT_DELAY_MS    1000

/* Phase 10: Adaptive Buffer Control */

#define CONFIG_QUEUE_DEPTH_MIN       5      /* Minimum queue depth */
#define CONFIG_QUEUE_DEPTH_MAX       9      /* Maximum queue depth */
#define CONFIG_QUEUE_DEPTH_DEFAULT   7      /* Default queue depth */

/* Phase 11: Enhanced Adaptive Control System */

#ifndef CONFIG_PHASE11_ENABLE
#define CONFIG_PHASE11_ENABLE        1      /* Enable Phase 11 enhanced control */
#endif

/* Frame Statistics Configuration */
#define CONFIG_FRAME_STATISTICS_WINDOW_SIZE   10     /* Frame statistics window size */
#define CONFIG_COMPLEXITY_THRESHOLD           0.5f   /* Scene complexity threshold */
#define CONFIG_STATISTICS_UPDATE_INTERVAL_MS  100    /* Statistics update interval */
#define CONFIG_COMPLEXITY_SMOOTHING_FACTOR    0.8f   /* Complexity smoothing factor */

/* Multi-Variable Control Configuration */
#define CONFIG_CONTROL_WEIGHT_QUEUE          0.6f   /* Queue depth weight */
#define CONFIG_CONTROL_WEIGHT_SIZE           0.2f   /* Frame size weight */
#define CONFIG_CONTROL_WEIGHT_TRANSMISSION   0.1f   /* Transmission time weight */
#define CONFIG_CONTROL_WEIGHT_PREDICTION     0.1f   /* Prediction weight */

/* Adaptive PID Configuration */
#define CONFIG_ADAPTIVE_PID_BASE_KP          0.15f  /* Base Kp gain */
#define CONFIG_ADAPTIVE_PID_BASE_KI          0.02f  /* Base Ki gain */
#define CONFIG_ADAPTIVE_PID_BASE_KD          0.0f   /* Base Kd gain */
#define CONFIG_ADAPTATION_FACTOR             0.8f   /* Adaptation factor */
#define CONFIG_MAX_ADAPTATION_RATE           0.2f   /* Maximum adaptation rate */

/* Predictive Control Configuration */
#define CONFIG_PREDICTION_WINDOW_SIZE        5      /* Prediction window size */
#define CONFIG_PREDICTION_ACCURACY_THRESHOLD 0.8f   /* Minimum prediction accuracy */
#define CONFIG_TRANSMISSION_TIME_THRESHOLD   150.0f /* Transmission time threshold (ms) */

/* Buffer Management Configuration */
#define CONFIG_BUFFER_RESIZE_THRESHOLD       0.9f   /* Buffer resize threshold */
#define CONFIG_MIN_BUFFER_SIZE_FRAMES        5      /* Minimum buffer size in frames */
#define CONFIG_MAX_BUFFER_SIZE_FRAMES        9      /* Maximum buffer size in frames */
#define CONFIG_BUFFER_SIZE_MULTIPLIER        1.2f   /* Buffer size multiplier */

/* Stability and Safety Configuration */
#define CONFIG_STABILITY_VARIANCE_THRESHOLD  0.5f   /* Stability variance threshold */
#define CONFIG_STABILITY_HISTORY_SIZE        5      /* Stability history size */
#define CONFIG_FALLBACK_TRIGGER_COUNT        3      /* Fallback trigger count */
#define CONFIG_ENHANCED_CONTROL_CYCLE_MS     100    /* Enhanced control cycle (ms) */
#define CONFIG_ENHANCED_CONTROL_TIMEOUT_MS   500    /* Enhanced control timeout (ms) */

/* Normalization Parameters */
#define CONFIG_NORMALIZE_QUEUE_DEPTH_MAX     10.0f  /* Queue depth normalization max */
#define CONFIG_NORMALIZE_FRAME_SIZE_MAX_KB   128.0f /* Frame size normalization max (KB) */
#define CONFIG_NORMALIZE_TRANSMISSION_MAX_MS 200.0f /* Transmission time normalization max (ms) */
#define CONFIG_NORMALIZE_COMPLEXITY_MAX      2.0f   /* Complexity normalization max */

/* Memory Limits */
#define CONFIG_PHASE11_MAX_MEMORY_KB         200    /* Maximum additional memory for Phase 11 (KB) */

/* Debug Configuration */

#define CONFIG_DEBUG_ENABLE          1
#define CONFIG_LOG_LEVEL             LOG_INFO  /* LOG_DEBUG, LOG_INFO, LOG_WARNING, LOG_ERR */

/* Error Codes */

#define ERR_OK                       0
#define ERR_CAMERA_INIT             -1
#define ERR_CAMERA_OPEN             -2
#define ERR_CAMERA_CONFIG           -3
#define ERR_CAMERA_CAPTURE          -4
#define ERR_ENCODER_INIT            -5
#define ERR_ENCODER_OPEN            -6
#define ERR_ENCODER_CONFIG          -7
#define ERR_ENCODER_ENCODE          -8
#define ERR_USB_INIT                -9
#define ERR_USB_OPEN                -10
#define ERR_USB_WRITE               -11
#define ERR_USB_DISCONNECTED        -12
#define ERR_PROTOCOL_INVALID        -13
#define ERR_NOMEM                   -14
#define ERR_TIMEOUT                 -15

/* Logging Macros */

#undef LOG_DEBUG
#undef LOG_INFO
#undef LOG_WARN
#undef LOG_ERROR

#if CONFIG_DEBUG_ENABLE
#  define LOG_DEBUG(fmt, ...) \
     syslog(7, "[CAM] " fmt "\n", ##__VA_ARGS__)
#  define LOG_INFO(fmt, ...) \
     syslog(6, "[CAM] " fmt "\n", ##__VA_ARGS__)
#  define LOG_WARN(fmt, ...) \
     syslog(4, "[CAM] " fmt "\n", ##__VA_ARGS__)
#  define LOG_ERROR(fmt, ...) \
     syslog(3, "[CAM] " fmt "\n", ##__VA_ARGS__)
#else
#  define LOG_DEBUG(fmt, ...)
#  define LOG_INFO(fmt, ...)
#  define LOG_WARN(fmt, ...)
#  define LOG_ERROR(fmt, ...) \
     syslog(3, "[CAM] " fmt "\n", ##__VA_ARGS__)
#endif

#endif /* __SECURITY_CAMERA_CONFIG_H */
