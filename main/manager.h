#pragma once
#include <stdint.h>
#include <stdlib.h>

#define MANAGER_USAGE_PAGE (0xFF<<8 | 0x01)
#define MANAGE_REPORT_MAP_ID    (0)

#define MANAGER_SHORT_MESSAGE_USAGE      (0x01)
#define MANAGER_SHORT_MESSAGE_REPORT_ID  (0x03)
#define MANAGER_SHORT_MESSAGE_LENGTH     (128)

#define MANAGER_LONG_MESSAGE_USAGE      (0x02)
#define MANAGER_LONG_MESSAGE_REPORT_ID  (0x04)
#define MANAGER_LONG_MESSAGE_BLOCK_LENGTH  (512)

#pragma pack(1)
typedef struct{
    uint16_t opcode;    // 0
    uint16_t session;   // 2
    uint16_t length;    // 4
    uint8_t  data[MANAGER_SHORT_MESSAGE_LENGTH - 6];   // 6
}request_t;

typedef struct{
    uint16_t session;   //  0
    uint16_t length;    //  2
    uint8_t  success;   //  4
    uint8_t  data[MANAGER_SHORT_MESSAGE_LENGTH - 5];   // 5
}response_t;

typedef struct{
    uint16_t session;       // 0
    uint8_t block_count;   // 2
    uint8_t block_id;      // 3
    uint8_t  data[MANAGER_LONG_MESSAGE_BLOCK_LENGTH - 4]; // 4
}long_msg_block_t;

#pragma pack()


typedef struct{
    uint8_t block_count;
    uint16_t session;
    uint32_t transferd_blocks;
    uint8_t *data;
}long_msg_t;

extern const uint8_t manager_report_desc[];
extern const size_t manager_report_desc_length;

void on_manager_output_short(uint8_t *data, uint16_t length);
//void on_manager_output_long(uint8_t *data, uint16_t length);


