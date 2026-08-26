#pragma once

#include "pins.h"

namespace config {

// SAFETY: docs.m5stack.com/en/stackchan — Y-axis (pitch) 5°..85° only.
// Extreme angles stall the servo and can permanently damage it.
constexpr float PITCH_SAFE_MIN_DEG = 5.0f;
constexpr float PITCH_SAFE_MAX_DEG = 85.0f;
constexpr float PITCH_HOME_DEG     = 45.0f;  // screen perpendicular to base
constexpr float YAW_HOME_DEG       = 0.0f;
constexpr float YAW_MIN_DEG        = -90.0f;
constexpr float YAW_MAX_DEG        = 90.0f;

// SCS0009: 0.3125° per raw step (BSP: mapped = zero + angle_ddeg * 16 / 5 / 10)
constexpr float SERVO_DEG_PER_RAW = 0.3125f;

constexpr int SPEAKER_VOLUME     = 180;  // 0..255
constexpr int DISPLAY_BRIGHTNESS = 128;

constexpr uint32_t WIFI_TIMEOUT_MS     = 12000;
constexpr uint32_t BRAIN_TIMEOUT_MS    = 5000;
constexpr uint32_t LLM_TIMEOUT_MS      = 20000;
constexpr uint32_t LLM_CMD_GAP_MS      = 1600;
constexpr uint16_t LLM_MAX_RESPONSE    = 8192;
constexpr uint32_t PERSON_COOLDOWN_MS  = 4000;
constexpr uint16_t PERSON_PS_THRESHOLD = 80;  // raw LTR-553 counts; tune on desk

constexpr int WS_RECONNECT_MS = 4000;
constexpr uint32_t VOICE_CONNECT_TIMEOUT_MS = 12000;

}  // namespace config
