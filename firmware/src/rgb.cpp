#include "rgb.h"
#include "pins.h"
#include <M5Unified.h>
#include <cstdio>
#include <cstring>

static constexpr uint8_t REG_VERSION   = 0x02;
static constexpr uint8_t REG_GPIO_M_L  = 0x03;
static constexpr uint8_t REG_GPIO_M_H  = 0x04;
static constexpr uint8_t REG_GPIO_O_L  = 0x05;
static constexpr uint8_t REG_GPIO_O_H  = 0x06;
static constexpr uint8_t REG_GPIO_PU_L = 0x09;
static constexpr uint8_t REG_GPIO_PU_H = 0x0A;
static constexpr uint8_t REG_GPIO_PD_L = 0x0B;
static constexpr uint8_t REG_GPIO_PD_H = 0x0C;
static constexpr uint8_t REG_GPIO_DRV_L = 0x13;
static constexpr uint8_t REG_GPIO_DRV_H = 0x14;
static constexpr uint8_t REG_LED_CFG   = 0x24;
static constexpr uint8_t REG_LED_RAM   = 0x30;

static uint8_t g_addr = pins::PY32_ADDR_DEFAULT;
static bool g_ok = false;

static m5::I2C_Class& bus() { return m5::In_I2C; }

static uint8_t rd8(uint8_t reg) { return bus().readRegister8(g_addr, reg, 100000); }
static void wr8(uint8_t reg, uint8_t v) { bus().writeRegister8(g_addr, reg, v, 100000); }

static void writeBit(uint8_t reg_l, uint8_t reg_h, uint8_t pin, bool value) {
    uint8_t reg = (pin < 8) ? reg_l : reg_h;
    uint8_t bit = (pin < 8) ? pin : (uint8_t)(pin - 8);
    uint8_t cur = rd8(reg);
    if (value) cur = (uint8_t)(cur | (1u << bit));
    else cur = (uint8_t)(cur & ~(1u << bit));
    wr8(reg, cur);
}

static bool probe(uint8_t addr) {
    g_addr = addr;
    uint8_t ver = rd8(REG_VERSION);
    return ver != 0 && ver != 0xFF;
}

void rgbSetServoPower(bool on) {
    if (!g_ok) return;
    writeBit(REG_GPIO_O_L, REG_GPIO_O_H, pins::PY32_PIN_VM_EN, on);
}

static void setLedCount(uint8_t count) {
    if (count > 32) count = 32;
    wr8(REG_LED_CFG, count & 0x3F);
}

static void setLedColor(uint8_t index, uint8_t r, uint8_t g, uint8_t b) {
    if (index >= 32) return;
    uint16_t c565 = (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
    uint8_t data[2] = {(uint8_t)(c565 & 0xFF), (uint8_t)((c565 >> 8) & 0xFF)};
    bus().writeRegister(g_addr, (uint8_t)(REG_LED_RAM + index * 2), data, 2, 100000);
}

static void refresh() {
    uint8_t val = rd8(REG_LED_CFG);
    wr8(REG_LED_CFG, (uint8_t)(val | (1u << 6)));
}

void rgbBegin() {
    g_ok = false;
    uint32_t start = millis();
    while (millis() - start < 1200) {
        delay(200);
        if (probe(pins::PY32_ADDR_DEFAULT) || probe(pins::PY32_ADDR_ALT)) {
            g_ok = true;
            break;
        }
    }
    if (!g_ok) {
        Serial.println("[rgb] PY32 expander not found (0x6F/0x71) — RGB + servo power unavailable");
        return;
    }
    Serial.printf("[rgb] PY32 ok addr=0x%02X ver=%u\n", g_addr, rd8(REG_VERSION));

    writeBit(REG_GPIO_M_L, REG_GPIO_M_H, pins::PY32_PIN_VM_EN, true);
    writeBit(REG_GPIO_PD_L, REG_GPIO_PD_H, pins::PY32_PIN_VM_EN, false);
    writeBit(REG_GPIO_PU_L, REG_GPIO_PU_H, pins::PY32_PIN_VM_EN, true);
    rgbSetServoPower(true);
    delay(200);

    writeBit(REG_GPIO_M_L, REG_GPIO_M_H, pins::PY32_PIN_RGB, true);
    writeBit(REG_GPIO_PD_L, REG_GPIO_PD_H, pins::PY32_PIN_RGB, false);
    writeBit(REG_GPIO_PU_L, REG_GPIO_PU_H, pins::PY32_PIN_RGB, true);
    writeBit(REG_GPIO_DRV_L, REG_GPIO_DRV_H, pins::PY32_PIN_RGB, false);
    setLedCount(pins::RGB_LED_COUNT);
    delay(200);
    rgbOff();
    delay(50);
    rgbOff();
}

void rgbSetAll(uint8_t r, uint8_t g, uint8_t b) {
    if (!g_ok) return;
    for (int i = 0; i < pins::RGB_LED_COUNT; i++) setLedColor((uint8_t)i, r, g, b);
    refresh();
}

void rgbOff() { rgbSetAll(0, 0, 0); }

void rgbSetEmotion(const char* name) {
    if (!name) {
        rgbOff();
        return;
    }
    if (!strcasecmp(name, "orange"))      rgbSetAll(255, 110, 16);
    else if (!strcasecmp(name, "blue"))   rgbSetAll(32, 80, 220);
    else if (!strcasecmp(name, "sleep"))  rgbSetAll(8, 12, 40);
    else if (!strcasecmp(name, "green"))  rgbSetAll(32, 180, 48);
    else if (!strcasecmp(name, "red"))    rgbSetAll(220, 24, 24);
    else if (!strcasecmp(name, "purple")) rgbSetAll(160, 32, 200);
    else if (!strcasecmp(name, "yellow")) rgbSetAll(220, 180, 16);
    else if (!strcasecmp(name, "white"))  rgbSetAll(160, 160, 160);
    else if (!strcasecmp(name, "off") || !strcasecmp(name, "black")) rgbOff();
    else if (name[0] == '#') {
        unsigned r = 0, g = 0, b = 0;
        if (sscanf(name, "#%02x%02x%02x", &r, &g, &b) == 3) rgbSetAll((uint8_t)r, (uint8_t)g, (uint8_t)b);
    } else {
        rgbSetAll(255, 110, 16);
    }
}

bool rgbAvailable() { return g_ok; }
