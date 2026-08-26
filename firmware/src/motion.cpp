#include "motion.h"
#include "pins.h"
#include "config.h"
#include <M5Unified.h>
#include <HardwareSerial.h>
#include <math.h>

static constexpr uint8_t INST_WRITE = 0x03;
static constexpr uint8_t REG_TORQUE = 0x28;
static constexpr uint8_t REG_GOAL   = 0x2A;

static Look g_look;
static bool g_ready = false;

float clampPitch(float pitch_deg) {
    if (pitch_deg < config::PITCH_SAFE_MIN_DEG) return config::PITCH_SAFE_MIN_DEG;
    if (pitch_deg > config::PITCH_SAFE_MAX_DEG) return config::PITCH_SAFE_MAX_DEG;
    return pitch_deg;
}

static float clampYaw(float yaw_deg) {
    if (yaw_deg < config::YAW_MIN_DEG) return config::YAW_MIN_DEG;
    if (yaw_deg > config::YAW_MAX_DEG) return config::YAW_MAX_DEG;
    return yaw_deg;
}

static HardwareSerial ServoUart(pins::SERVO_UART_NUM);

static void scsWrite(const uint8_t* p, size_t n) {
    ServoUart.write(p, n);
    ServoUart.flush();
}

static void scsWriteBuf(uint8_t id, uint8_t addr, const uint8_t* data, uint8_t n) {
    uint8_t len = (uint8_t)(n + 3);
    uint8_t hdr[6] = {0xFF, 0xFF, id, len, INST_WRITE, addr};
    uint8_t sum = (uint8_t)(id + len + INST_WRITE + addr);
    for (uint8_t i = 0; i < n; i++) sum = (uint8_t)(sum + data[i]);
    sum = (uint8_t)~sum;
    scsWrite(hdr, 6);
    scsWrite(data, n);
    scsWrite(&sum, 1);
}

static void scsWritePos(uint8_t id, uint16_t pos, uint16_t time, uint16_t speed) {
    uint8_t b[6] = {
        (uint8_t)(pos >> 8),   (uint8_t)(pos & 0xFF),
        (uint8_t)(time >> 8),  (uint8_t)(time & 0xFF),
        (uint8_t)(speed >> 8), (uint8_t)(speed & 0xFF),
    };
    scsWriteBuf(id, REG_GOAL, b, 6);
}

static void scsTorque(uint8_t id, bool on) {
    uint8_t v = on ? 1 : 0;
    scsWriteBuf(id, REG_TORQUE, &v, 1);
}

static int degToRaw(int zero_raw, float deg) {
    int raw = zero_raw + (int)lroundf(deg / config::SERVO_DEG_PER_RAW);
    if (raw < pins::SERVO_RAW_MIN) raw = pins::SERVO_RAW_MIN;
    if (raw > pins::SERVO_RAW_MAX) raw = pins::SERVO_RAW_MAX;
    return raw;
}

void motionBegin() {
    ServoUart.begin(pins::SERVO_UART_BAUD, SERIAL_8N1, pins::SERVO_RX, pins::SERVO_TX);
    delay(50);
    scsTorque(pins::SERVO_ID_YAW, true);
    scsTorque(pins::SERVO_ID_PITCH, true);
    g_ready = true;
    Serial.println("[motion] UART1 1Mbps TX=GPIO6 RX=GPIO7  yaw=ID1 pitch=ID2");
    motionHome();
}

void motionLook(float yaw_deg, float pitch_deg, uint16_t time_ms) {
    if (!g_ready) return;
    yaw_deg = clampYaw(yaw_deg);
    pitch_deg = clampPitch(pitch_deg);
    g_look.yaw_deg = yaw_deg;
    g_look.pitch_deg = pitch_deg;

    uint16_t t = time_ms;
    int yaw_raw = degToRaw(pins::SERVO_YAW_ZERO_RAW, yaw_deg);
    int pitch_raw = degToRaw(pins::SERVO_PITCH_ZERO_RAW, pitch_deg);
    Serial.printf("[motion] look yaw=%.1f pitch=%.1f (clamped) raw=%d/%d\n",
                     yaw_deg, pitch_deg, yaw_raw, pitch_raw);
    scsWritePos(pins::SERVO_ID_YAW, (uint16_t)yaw_raw, t, 0);
    scsWritePos(pins::SERVO_ID_PITCH, (uint16_t)pitch_raw, t, 0);
}

void motionLook(const Look& look, uint16_t time_ms) {
    motionLook(look.yaw_deg, look.pitch_deg, time_ms);
}

void motionHome() {
    motionLook(config::YAW_HOME_DEG, config::PITCH_HOME_DEG, 400);
}

void motionSnapTowardPerson() {
    motionLook(0.0f, 20.0f, 180);
}

Look motionCurrent() { return g_look; }
