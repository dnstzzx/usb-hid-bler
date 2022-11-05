#pragma once
#include <stdlib.h>
void usb_main();
void ble_main();

//void ble_send_mouse(uint8_t *data, uint8_t length);
void ble_send(size_t mapid, uint8_t *data, uint8_t length);
void data_transform(uint8_t *data, uint8_t length);

void blink_init();
void set_blink(uint8_t enable);
void print_hex_dump(const char *name,uint8_t *buffer, int len);