#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <driver/gpio.h>
#include <esp_system.h>
#include <soc/rtc_cntl_reg.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "usb_host.h"
#include "usb_main.h"
#include "cJSON.h"
#include "manager.h"
#include "ble_device.h"
#include "peripherals.h"
#include "macro.h"
#include "storage.h"

/*
response_t必须由make_xxx_response系列函数返回，否则会导致越界访问和内存泄漏

request op codes
0   echo                data_req: any           data_ret: any
1   get_info            data_req: any           data_ret: info object 
2   restart             data_req: number        data_ret:  no ret
3   set_device_name     data_req: str           data_ret: str
4   get_macros          data_req: any           data_ret: macro obj
5   set_macro           data_req: macro_obj     data_ret: str
6   remove_macro        data_req: name str      data_ret: str

macro obj:
{
    mouse: {
        name: string
        version: number
        trigger_buttons_mask: number
        cancel_input_report: boolean
        output_model: number
        action_type: number
        action_delay: number
        report_duration: number
        mouse_output_report: hex string
    }
}
*/

#define LONG_MSG_BLOCK_DATA_LENGTH  (sizeof(((long_msg_block_t *)0)->data))
#define compute_block_count(msg_length) (msg_length + (LONG_MSG_BLOCK_DATA_LENGTH - 1)) / LONG_MSG_BLOCK_DATA_LENGTH

#define LONG_MSG_COUNT  16
#define LONG_MSG_LIVE_MS    2000
#define LONG_MSG_CLEAR_INTERVAL 1000

typedef struct{
    uint8_t block_count;
    uint8_t ttl;
    uint16_t session;     // 0 if not used
    uint32_t transferd_blocks;
    uint8_t *data;
}long_msg_t;

long_msg_t long_msgs[LONG_MSG_COUNT];
SemaphoreHandle_t long_msg_lock;

void clear_long_msg(void *param){
    while(true){
        if(xSemaphoreTake(long_msg_lock, portMAX_DELAY)){
            for(int i=0;i<LONG_MSG_COUNT;i++){
                long_msg_t *msg = &long_msgs[i];
                if(msg->session == 0) continue;
                if(msg->ttl-- == 0){
                    free(msg->data);
                    msg->session = 0;
                }
            }
            xSemaphoreGive(long_msg_lock);
        }
        vTaskDelay(LONG_MSG_CLEAR_INTERVAL / portTICK_PERIOD_MS);
    }
}

void manager_init(){
    long_msg_lock = xSemaphoreCreateMutex();
    xTaskCreate(clear_long_msg, "crear_long_msg", 512, NULL, 1, NULL);
}

response_t *make_string_response(uint16_t session, char *msg){
    size_t l = strlen(msg);
    response_t *response;
    size_t msg_size = offsetof(response_t, data) + l;
    if(msg_size >= sizeof(response_t)){  
        msg_size = compute_block_count(msg_size) * LONG_MSG_BLOCK_DATA_LENGTH;   // 向上取block data长度整数倍
        response = malloc(msg_size + offsetof(long_msg_block_t, data) + 1);                     // 为block 0包头预留空间
        response = (response_t *) (((uint8_t *)response) + offsetof(long_msg_block_t, data));
    }else{
       response = malloc(sizeof(response_t)); 
    }
    
    response->session = session;
    response->success = true;
    response->length = l;
    memcpy(response->data, msg, l);
    ((char *)response->data)[l] = '\0';
    return response;
}

response_t *make_fail_response(uint16_t session, char *reason){
    response_t *resp = make_string_response(session, reason);
    resp->success = false;
    return resp;
}

response_t *make_object_response(uint16_t session, cJSON *obj){
    char *s = cJSON_PrintUnformatted(obj);
    response_t *resp = make_string_response(session, s);
    cJSON_free(s);
    return resp;
}

response_t *echo(request_t *request_in){
    return make_string_response(request_in->session, (char *)request_in->data);
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
response_t *get_info(request_t *request_in){
    cJSON *json = cJSON_CreateObject();
    cJSON_AddNumberToObject(json, "battery_voltage", battery_voltage);
    cJSON *slots = cJSON_AddArrayToObject(json, "slots");
    for(int i=0;i<NUM_USB;i++){
        cJSON *slot = cJSON_CreateObject();
        cJSON_AddNumberToObject(slot, "id", i);
        cJSON_AddNumberToObject(slot, "mode", install_statuses[i][0].mode);
        cJSON_AddItemToArray(slots, slot);
    }
    response_t *resp = make_object_response(request_in->session, json);
    cJSON_Delete(json);
    return resp;
}

response_t *restart(request_t *request_in){
    if(request_in->length != 1 || (request_in->data[0] != '0' && request_in->data[0] != '1')){
        return make_fail_response(request_in->session, "参数格式错误");
    }
    uint8_t download_mode = request_in->data[0] == '1';
    if(download_mode){
        *((uint32_t *)RTC_CNTL_OPTION1_REG) = 1;
    }
    esp_restart();
}

response_t *set_device_name(request_t *request_in){
    return make_string_response(request_in->session, "ok");
}

response_t *get_macros(request_t *request_in){
    cJSON *json = cJSON_CreateObject();
    cJSON *mouse_macros_json = cJSON_AddArrayToObject(json, "mouse");
    
    macro_t *macro;
    list_for_each_entry(macro, (saved_list_head_t *)macros, head){
        cJSON *macro_json = macro_to_json(macro);
        cJSON_AddItemToArray(mouse_macros_json, macro_json);
    }

    return make_object_response(request_in->session ,json);
}
#define CHECK_NULL(x) if(x==NULL) goto fail;
response_t *_set_macro(request_t *request_in){
    cJSON *json = cJSON_Parse((const char *)request_in->data);
    CHECK_NULL(json);
    cJSON *mouse_json = cJSON_GetObjectItem(json, "mouse");
    CHECK_NULL(mouse_json);
    macro_t macro;
    CHECK_NULL(macro_from_json(mouse_json, &macro));
    set_macro(&macro);
    return make_string_response(request_in->session, "ok");
    fail:
    return make_fail_response(request_in->session, "数据格式错误");
}

response_t *remove_macro(request_t *request_in){
    size_t l = strlen((const char *)request_in->data);
    if(l != request_in->length || l >= sizeof(((macro_t *)0)->head.name)){
        return make_fail_response(request_in->session, "格式错误");
    }
    return delete_macro((const char *)request_in->data) ? \
            make_string_response(request_in->session, "ok") : make_fail_response(request_in->session, "操作失败");
}


typedef response_t *(*handler_t)(request_t *);
handler_t handlers[] = {
    echo, get_info, restart, set_device_name, get_macros, _set_macro, remove_macro
};

#define DEBUG_PRINT_OUTPUT_MSG
void handle_request(request_t *request){
    response_t *response;
    if(request->opcode >= sizeof(handlers) / sizeof(handler_t)){
        response = make_fail_response(request->session, "Unsupported function");
    }else{
        response = handlers[request->opcode](request);
    }
    
    #ifdef DEBUG_PRINT_OUTPUT_MSG
        printf("request in, opcode=%d,session=%d,length=%d,data=%s\n", request->opcode, request->session, request->length, request->data);
        printf("response session=%d,length=%d,data=%s\n", response->session, response->length, response->data);
    #endif
    if(response->length <= sizeof(response->data)){
        ble_send(MANAGE_REPORT_MAP_ID, MANAGER_SHORT_MESSAGE_REPORT_ID, (uint8_t *)response, MANAGER_SHORT_MESSAGE_LENGTH);
        free(response);
    }else{
        size_t msg_size = offsetof(response_t, data) + response->length;
        size_t blk_count = compute_block_count(msg_size);
        uint16_t session = response->session;
        for(int i=0;i<blk_count;i++){
            long_msg_block_t *blk = (long_msg_block_t *)((uint8_t *)response + i * LONG_MSG_BLOCK_DATA_LENGTH - offsetof(long_msg_block_t, data));
            blk->block_count = blk_count;
            blk->block_id = i;
            blk->session = session;
            ble_send(MANAGE_REPORT_MAP_ID, MANAGER_LONG_MESSAGE_REPORT_ID, (uint8_t *)blk, sizeof(long_msg_block_t));
        }
        free((uint8_t *)response - offsetof(long_msg_block_t, data));
    }
}

void on_manager_output_short(uint8_t *data, uint16_t length){
    if(length != MANAGER_SHORT_MESSAGE_LENGTH) return;
    request_t *request = (request_t *)data;
    if(request->length > sizeof(request->data))   return;
    handle_request(request);
}

void on_manager_output_long(uint8_t *data, uint16_t length){
    if(length != MANAGER_LONG_MESSAGE_BLOCK_LENGTH) return;
    long_msg_block_t *blk = (long_msg_block_t *)data;
    if(xSemaphoreTake(long_msg_lock, portMAX_DELAY)){
        int free_idx = -1;
        int found_idx = -1;
        for(int i=0;i<LONG_MSG_COUNT;i++){
            if(long_msgs[i].session == blk->session){
                found_idx = i;
                break;
            }else if(free_idx == -1 && long_msgs[i].session == 0){
                free_idx = i;
            }
        }
        if(found_idx == -1 && free_idx == -1){
            printf("warning: new long msg(session: %d)but no free space, ignoring\n", blk->session);
            xSemaphoreGive(long_msg_lock);
            return;
        }
        if(found_idx == -1){  // 新消息
            found_idx = free_idx;
            long_msg_t *msg = &long_msgs[found_idx];
            msg->session = blk->session;
            msg->block_count = blk->block_count;
            msg->transferd_blocks = 0;
            msg->data = malloc(msg->block_count * LONG_MSG_BLOCK_DATA_LENGTH);
        }

        long_msg_t *msg = &long_msgs[found_idx];
        memcpy(msg->data + blk->block_id * LONG_MSG_BLOCK_DATA_LENGTH, blk->data, LONG_MSG_BLOCK_DATA_LENGTH);
        msg->ttl = LONG_MSG_LIVE_MS / LONG_MSG_CLEAR_INTERVAL;
        msg->transferd_blocks |= 1 << blk->block_id;
        if(msg->transferd_blocks == (1 << msg->block_count) - 1){   // 全部传输完成
            request_t *request = (request_t *)msg->data;
            if(request->length + offsetof(request_t, data) > msg->block_count * LONG_MSG_BLOCK_DATA_LENGTH) return;
            handle_request(request);
            free(request);
            msg->session = 0;
        }
        
    }
    xSemaphoreGive(long_msg_lock);
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
    0x96, 0x80, 0x00,  //   Report Count (128)
    0x91, 0x02,        //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x09, 0x01,        //   Usage (0x01)
    0x25, 0x00,        //   Logical Maximum (0)
    0x26, 0xFF, 0x00,  //   Logical Maximum (255)
    0x75, 0x08,        //   Report Size (8)
    0x96, 0x80, 0x00,  //   Report Count (128)
    0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x85, 0x04,        //   Report ID (4)
    0x09, 0x02,        //   Usage (0x02)
    0x15, 0x00,        //   Logical Minimum (0)
    0x26, 0xFF, 0x00,  //   Logical Maximum (255)
    0x75, 0x08,        //   Report Size (8)
    0x96, 0x00, 0x02,  //   Report Count (512)
    0x91, 0x02,        //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x09, 0x02,        //   Usage (0x02)
    0x25, 0x00,        //   Logical Maximum (0)
    0x26, 0xFF, 0x00,  //   Logical Maximum (255)
    0x75, 0x08,        //   Report Size (8)
    0x96, 0x00, 0x02,  //   Report Count (512)
    0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0xC0,              // End Collection
};

const size_t manager_report_desc_length = sizeof(manager_report_desc);

