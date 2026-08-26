#pragma once
//
// Desk Rocky — StackChan CoreS3 (SKU K151-R) pin map
// Sources: https://docs.m5stack.com/en/stackchan PinMap
//          https://docs.m5stack.com/en/core/CoreS3
//          m5stack/StackChan-BSP 1.0.1 (servo_init: UART1, 1 Mbps, TX=6, RX=7)
//
// If a pin is marked UNCERTAIN, it is a named constant with a comment — never
// a magic number buried in motion code.

#include <cstdint>

namespace pins {

constexpr int SERVO_TX = 6;
constexpr int SERVO_RX = 7;
constexpr int IR_SEND  = 5;
constexpr int IR_REC   = 10;

constexpr int I2C_SDA = 12;
constexpr int I2C_SCL = 11;

constexpr uint8_t PY32_ADDR_DEFAULT = 0x6F;
constexpr uint8_t PY32_ADDR_ALT     = 0x71;
constexpr uint8_t INA226_ADDR       = 0x41;
constexpr uint8_t NFC_ADDR          = 0x50;
constexpr uint8_t TOUCH_SI12T_ADDR  = 0x68;
constexpr uint8_t LTR553_ADDR       = 0x23;

constexpr uint8_t PY32_PIN_VM_EN = 0;
constexpr uint8_t PY32_PIN_RGB   = 13;
constexpr int RGB_LED_COUNT      = 12;

constexpr uint8_t SERVO_ID_YAW   = 1;
constexpr uint8_t SERVO_ID_PITCH = 2;
constexpr int SERVO_UART_BAUD    = 1000000;
constexpr int SERVO_UART_NUM     = 1;

constexpr int SERVO_YAW_ZERO_RAW   = 460;
constexpr int SERVO_PITCH_ZERO_RAW = 620;
constexpr int SERVO_RAW_MIN        = 0;
constexpr int SERVO_RAW_MAX        = 1000;

}  // namespace pins
