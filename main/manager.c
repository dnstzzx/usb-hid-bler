#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <driver/gpio.h>
#include <esp_system.h>
#include <soc/rtc_cntl_reg.h>
#include "usb_host.h"
#include "usb_main.h"
#include "cJSON.h"
#include "manager.h"
#include "ble_device.h"
#include "peripherals.h"


/*requests

0   echo
1   get_info
2   restart

*/


void make_fail_response(response_t *response, uint16_t session, char *reason){
    size_t l = strlen(reason);
    if(l >= sizeof(response->data)) return make_fail_response(response, session, "返回数据长度超出编码限制");
    response->session = session;
    response->success = 0;
    response->length = l;
    strcpy((char *)response->data, reason);
}

void make_string_response(response_t *response, uint16_t session, char *msg){
    size_t l = strlen(msg);
    if(l >= sizeof(response->data)) return make_fail_response(response, session, "响应数据长度超出编码限制");
    response->session = session;
    response->success = 1;
    response->length = l;
    strcpy((char *)response->data, msg);
}

void make_object_response(response_t *response, uint16_t session, cJSON *obj){
    char *s = cJSON_PrintUnformatted(obj);
    make_string_response(response, session, s);
    cJSON_free(s);
}

void echo(request_t *request_in, response_t *response_out){
    response_out->session = request_in->session;
    response_out->length = request_in->length;
    response_out->success = 1;
    memcpy(response_out->data, request_in->data, request_in->length);
    if(request_in->data[request_in->length - 1] != '\0'){
        response_out->data[request_in->length] = '\0';
        response_out->length ++;
    }
}

/*
interface Device_Info{
    battery_voltage: number,
    slots: {
        id: number,
        mode: number
    }[]
}
*/
void get_info(request_t *request_in, response_t *response_out){
    cJSON *json = cJSON_CreateObject();
    cJSON_AddNumberToObject(json, "battery_voltage", battery_voltage);
    cJSON *slots = cJSON_AddArrayToObject(json, "slots");
    for(int i=0;i<NUM_USB;i++){
        cJSON *slot = cJSON_CreateObject();
        cJSON_AddNumberToObject(slot, "id", i);
        cJSON_AddNumberToObject(slot, "mode", install_statuses[i][0].mode);
        cJSON_AddItemToArray(slots, slot);
    }
    make_object_response(response_out, request_in->session, json);
    cJSON_Delete(json);
}

void restart(request_t *request_in, response_t *response_out){
    if(request_in->length != 1 || (request_in->data[0] != '0' && request_in->data[0] != '1')){
        make_fail_response(response_out, request_in->session, "参数格式错误");
    }
    uint8_t download_mode = request_in->data[0] == '1';
    if(download_mode){
        *((uint32_t *)RTC_CNTL_OPTION1_REG) = 1;
    }
    esp_restart();
    
}

typedef void(*handler_t)(request_t *, response_t *);
handler_t handlers[] = {
    echo, get_info, restart
};

#define DEBUG_PRINT_OUTPUT_MSG
void on_manager_output(uint8_t *data, uint16_t length){
    if(length != MANAGE_MESSAGE_LENGTH) return;
    request_t request;
    response_t response;
    memcpy(&request, data, length);
    if(request.length > sizeof(request.data))   return;
    request.data[request.length] = '\0';
    if(request.opcode >= sizeof(handlers) / sizeof(handler_t)) return;
    handlers[request.opcode](&request, &response);
    #ifdef DEBUG_PRINT_OUTPUT_MSG
        printf("request in, opcode=%d,session=%d,length=%d,data=%s\n", request.opcode, request.session, request.length, request.data);
        printf("response session=%d,length=%d,data=%s\n", response.session, response.length, response.data);
    #endif
    ble_send(MANAGE_REPORT_MAP_ID, MANAGER_INPUT_REPORT_ID, (uint8_t *)&response, MANAGE_MESSAGE_LENGTH);
}


const uint8_t manager_report_desc[] = {
    0x06, 0x01, 0xFF,  // Usage Page (Vendor Defined 0xFF01)
    0x09, 0x01,        // Usage (0x01)
    0xA1, 0x01,        // Collection (Application)
    0x85, 0x03,        //   Report ID (3)
    0x09, 0x01,        //   Usage (0x01)
    0x15, 0x00,        //   Logical Minimum (0)
    0x26, 0xFF, 0x00,  //   Logical Maximum (255)
    0x75, 0x08,        //   Report Size (8)
    0x95, 0x80,        //   Report Count (128)
    0x91, 0x02,        //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x09, 0x01,        //   Usage (0x01)
    0x25, 0x00,        //   Logical Maximum (0)
    0x26, 0xFF, 0x00,  //   Logical Maximum (255)
    0x75, 0x08,        //   Report Size (8)
    0x95, 0x80,        //   Report Count (128)
    0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0xC0,              // End Collection
};

const size_t manager_report_desc_length = sizeof(manager_report_desc);

