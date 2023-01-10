#include "peripherals.h"
#include "macro.h"
#include "manager.h"
#include "storage.h"
#include "ble_device.h"
#include "usb_main.h"
#include "report_map.h"
#include <stdio.h>

void app_main(void){
    usb_init();
    peripherals_init();
    map_init();
    storage_init();
    macro_init();
    manager_init();
    ble_main();
}


