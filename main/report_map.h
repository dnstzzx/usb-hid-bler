#pragma once
#include <stdint.h>
#include <string.h>

#define MAP_BUFF_SIZE 0x100
#define STORE_MAP_COUNT 32

enum Process_Mode {NONE, PASSTHROUGH, TRANSFORM_MOUSE, TRANSEFORM_KEYBOARD};

typedef uint8_t Report_Map_BUFF[MAP_BUFF_SIZE];


typedef struct{
    uint32_t length;
    uint32_t sum;
    uint32_t process_mode;
}Map_Index_Item;

typedef struct{
    size_t map_count;
    Map_Index_Item indexes[STORE_MAP_COUNT];
}Map_Index;


extern Map_Index map_index;
extern Report_Map_BUFF saved_maps[STORE_MAP_COUNT];

void init_map();
//void load_report_maps();
void save_map_index();
//size_t get_report_map(char *map_id, Report_Map_BUFF buff);
void save_report_map(int map_index);
int add_report_map(uint8_t *map, size_t length);

/* 
    check whether a report map stored in nvs
    returns: map index if found else -1
*/
int check_report_map(Report_Map_BUFF map, size_t length);