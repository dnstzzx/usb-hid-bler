#include "blink.h"
#include "ble_device.h"
#include "report_map.h"
#include <stdio.h>

void app_main(void){
    blink_init();
    init_map();
    ble_main();
}

void data_transform(uint8_t *data, uint8_t length){
    if(length == 7){
        int8_t wheel = (int8_t)data[6];
        data[6] = (uint8_t) (-1 * wheel);
    }
}


