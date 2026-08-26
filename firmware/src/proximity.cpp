#include "proximity.h"
#include "pins.h"
#include "config.h"
#include <M5Unified.h>

static constexpr uint8_t REG_PS_CONTR     = 0x81;
static constexpr uint8_t REG_PS_LED       = 0x82;
static constexpr uint8_t REG_PS_N_PULSES  = 0x83;
static constexpr uint8_t REG_PS_MEAS_RATE = 0x84;
static constexpr uint8_t REG_ALS_CONTR    = 0x80;
static constexpr uint8_t REG_PART_ID      = 0x86;
static constexpr uint8_t REG_PS_DATA_0    = 0x8D;

static bool g_ok = false;

static m5::I2C_Class& bus() { return m5::In_I2C; }

static void wr(uint8_t reg, uint8_t v) {
    bus().writeRegister8(pins::LTR553_ADDR, reg, v, 100000);
}
static uint8_t rd(uint8_t reg) {
    return bus().readRegister8(pins::LTR553_ADDR, reg, 100000);
}

void proximityBegin() {
    g_ok = false;
    uint8_t part = rd(REG_PART_ID);
    if (part == 0x00 || part == 0xFF) {
        Serial.printf("[prox] LTR-553 not found (part=0x%02X)\n", part);
        return;
    }
    wr(REG_PS_LED, 0x7B);
    wr(REG_PS_N_PULSES, 0x08);
    wr(REG_PS_MEAS_RATE, 0x02);
    wr(REG_PS_CONTR, 0x03);
    wr(REG_ALS_CONTR, 0x01);
    delay(60);
    g_ok = true;
    Serial.printf("[prox] LTR-553 ok part=0x%02X\n", part);
}

bool proximityAvailable() { return g_ok; }

uint16_t proximityRaw() {
    if (!g_ok) return 0;
    uint8_t buf[2] = {0, 0};
    bus().readRegister(pins::LTR553_ADDR, REG_PS_DATA_0, buf, 2, 100000);
    return (uint16_t)(buf[0] | ((buf[1] & 0x07) << 8));
}

int proximityRangeMm() {
    uint16_t ps = proximityRaw();
    if (ps < 8) return 900;
    long mm = 25000L / (long)(ps + 15);
    if (mm < 40) mm = 40;
    if (mm > 900) mm = 900;
    return (int)mm;
}

bool proximityPersonPresent() {
    if (!g_ok) return false;
    return proximityRaw() >= config::PERSON_PS_THRESHOLD;
}
