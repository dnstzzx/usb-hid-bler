#pragma once
#include <stdint.h>
#include <stdlib.h>

#define MANAGER_USAGE_PAGE (0xFF<<8 | 0x01)
#define MANAGER_USAGE      (0x01)
#define MANAGE_REPORT_MAP_ID    (0)
#define MANAGER_INPUT_REPORT_ID  (0x03)
#define MANAGER_OUTPUT_REPORT_ID  (0x03)
#define MANAGE_MESSAGE_LENGTH     (128)

#pragma pack(1)
typedef struct{
    uint16_t opcode;    // 0
    uint16_t session;   // 2
    uint16_t length;    // 4
    uint8_t  data[MANAGE_MESSAGE_LENGTH - 6];   // 6
    uint8_t  endchar;
}request_t;

typedef struct{
    uint16_t session;   //  0
    uint16_t length;    //  2
    uint8_t  data[MANAGE_MESSAGE_LENGTH - 4];   // 4
}response_t;
#pragma pack()

extern const uint8_t manager_report_desc[];
extern const size_t manager_report_desc_length;

void on_manager_output(uint8_t *data, uint16_t length);



