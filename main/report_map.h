#pragma once
#include <stdint.h>
#include <string.h>
#include "translate.h"

#define MAP_BUFF_SIZE 0x100
#define PREDEFINED_MAP_COUNT 1
#define STORE_MAP_COUNT 32

typedef uint8_t Report_Map_BUFF[MAP_BUFF_SIZE];
enum Process_Mode {PROCESS_MODE_NONE, PROCESS_MODE_PASSTHROUGH, PROCESS_MODE_TRANSLATE_MOUSE, PROCESS_MODE_TRANSLATE_KEYBOARD};

typedef struct{
    uint32_t length;
    uint32_t sum;
    uint32_t process_mode;
}Map_Info_Item;

typedef struct{
    size_t map_count;
    Map_Info_Item indexes[STORE_MAP_COUNT];
}Map_Info_Table;


extern Map_Info_Table map_info_table;
extern Report_Map_BUFF saved_maps[STORE_MAP_COUNT];

void init_map();
void save_map_info_table();
void save_report_map(int map_index);

/* returns map index in table, -1 if table is already full */
int add_report_map(uint8_t *map, size_t length);

/* 
    check whether a report map in table
    returns: map index if found else -1
*/
int check_report_map(Report_Map_BUFF map, size_t length);