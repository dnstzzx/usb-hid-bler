#pragma once
#include <stdlib.h>

void ble_main();
void ble_send(size_t mapid, size_t report_id, uint8_t *data, size_t length);