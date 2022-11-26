#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "manager.h"
#include "ble_device.h"


/*requests

0   echo
1   get_info
2   restart

*/

void echo(request_t *request_in, response_t *response_out){
    response_out->session = request_in->session;
    response_out->length = request_in->length;
    memcpy(response_out->data, request_in->data, request_in->length);
    if(request_in->data[request_in->length - 1] != '\0'){
        response_out->data[request_in->length] = '\0';
        response_out->length ++;
    }
}

typedef void(*handler_t)(request_t *, response_t *);
handler_t handlers[] = {
    echo
};

#define DEBUG_PRINT_OUTPUT_MSG
void on_manager_output(uint8_t *data, uint16_t length){
    if(length != MANAGE_MESSAGE_LENGTH) return;
    request_t request;
    response_t response;
    memcpy(&request, data, length);
    request.data[length] = 0;
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

