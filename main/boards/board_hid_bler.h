#pragma once

#include "board_common.h"

#define BOARD_HAS_LED
#define BOARD_PIN_LED   (3)   

#define BOARD_HAS_FINGERPRINT_MODULE
#define BOARD_FINGERPRINT_PIN_EN    (10)
#define BOARD_FINGERPRINT_PIN_TX    (0)
#define BOARD_FINGERPRINT_PIN_RX    (1)
#define BOARD_FINGERPRINT_PIN_IRQ   (18)

#define BOARD_HAS_BATTARY
#define BOARD_PIN_BATTARY   (2)
#define BOARD_BATTARY_VOLTAGE_SCALE (2)

#define BOARD_USB1_PIN_DP   (5)
#define BOARD_USB1_PIN_DM   (4)
#define BOARD_USB2_PIN_DP   (6)
#define BOARD_USB2_PIN_DM   (7)
