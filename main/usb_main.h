#pragma once
#include "usb_host.h"
#include "report_map.h"
#include "translate.h"

typedef struct{
    enum Process_Mode mode;
    mouse_translate_t mouse_translate;
    int mapid;
}install_status_t;

extern install_status_t install_statuses[NUM_USB][4];


void usb_main();