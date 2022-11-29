#pragma once
#include <stdint.h>

extern int  battery_voltage;

void peripherals_init();
void set_blink(uint8_t enable);