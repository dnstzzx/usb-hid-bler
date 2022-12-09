#include "peripherals.h"
#include "macro.h"
#include "manager.h"
#include "storage.h"
#include "ble_device.h"
#include "report_map.h"
#include <stdio.h>

void app_main(void){
    peripherals_init();
    map_init();
    storage_init();
    macro_init();
    manager_init();
    ble_main();
}

void data_transform(uint8_t *data, uint8_t length){
    if(length == 7){
        int8_t wheel = (int8_t)data[6];
        data[6] = (uint8_t) (-1 * wheel);
    }
}


